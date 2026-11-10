"""Release selection and publication must keep artifacts bound to their build commit."""
import importlib.util
import json
import os
import sys
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('release', Path(__file__).resolve().parents[1] / '.github/scripts/release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)

REPO = 'owner/pascal'
COMMIT = 'a' * 40


def run(identifier=1, **changes):
    value = {'id': identifier, 'status': 'completed', 'conclusion': 'success', 'event': 'push',
             'head_branch': 'main', 'head_sha': COMMIT, 'head_repository': {'full_name': REPO},
             'html_url': f'https://github.com/{REPO}/actions/runs/{identifier}'}
    return dict(value, **changes)


def artifact(identifier=10, **changes):
    return dict({'id': identifier, 'name': 'zeal-pascal-dist', 'expired': False,
                 'expires_at': '2099-01-01T00:00:00Z'}, **changes)


class Release(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.archive = Path(self.temp.name) / 'zeal-pascal.zip'
        self.archive.write_bytes(b'PK\x03\x04distribution bytes')
        self.calls = []
        self.runs = {1: [run()]}
        self.artifacts = {1: [artifact()]}
        self.releases = []
        self.tag_exists = False
        self.upload_error = False
        self.api_patch = patch.object(release, 'api', side_effect=self.api)
        self.api_patch.start()
        self.addCleanup(self.api_patch.stop)

    def api(self, endpoint, method='GET', payload=None, upload=None):
        if method != 'GET':
            self.calls.append((endpoint, method, payload, upload))
            if upload and self.upload_error:
                raise RuntimeError('upload failed')
            if endpoint == f'repos/{REPO}/releases':
                return {'id': 123, 'html_url': 'https://release',
                        'upload_url': 'https://uploads.github.com/release/123/assets{?name,label}'}
            return {}
        if endpoint == f'repos/{REPO}':
            return {'default_branch': 'main'}
        if '/git/ref/tags/' in endpoint:
            if self.tag_exists:
                return {'object': {'sha': 'b' * 40}}
            raise FileNotFoundError(endpoint)
        if '/actions/workflows/build.yml/runs?' in endpoint:
            self.assertIn('branch=main&status=success', endpoint)
            page = int(endpoint.rsplit('page=', 1)[1])
            return {'workflow_runs': self.runs.get(page, [])}
        if '/actions/runs/' in endpoint:
            identifier = int(endpoint.split('/runs/')[1].split('/')[0])
            return {'artifacts': self.artifacts.get(identifier, [])}
        if endpoint.startswith(f'repos/{REPO}/releases?'):
            return self.releases
        raise AssertionError(f'Unexpected API call: {endpoint}')

    def select(self, version='1.0.0', ref='refs/heads/main'):
        return release.select(REPO, ref, version)

    def publish(self, tag='v1.0.0'):
        return release.publish(REPO, tag, COMMIT, 'https://build', self.archive)

    def test_versions_are_normalized_and_unsafe_input_is_rejected(self):
        self.assertEqual(release.version_tag(' 1.2.3 '), 'v1.2.3')
        self.assertEqual(release.version_tag('v1.2.3-rc.1'), 'v1.2.3-rc.1')
        for value in ('', 'main', '1.2', '../1.2.3', '1.2.3;echo bad', '1.2.3\nother', '01.0.0', '1.00.0', '1.0.00', '١.2.3'):
            with self.subTest(value=value), self.assertRaises(ValueError):
                release.version_tag(value)

    def test_selects_retained_artifact_across_pages_and_uses_build_commit(self):
        self.runs = {1: [run(3)] * 100, 2: [run(2), run(1)]}
        self.artifacts = {3: [artifact(expired=True)], 2: [artifact(20)]}
        result = self.select()
        self.assertEqual(result['run_id'], 2)
        self.assertEqual(result['artifact_id'], 20)
        self.assertEqual(result['commit'], COMMIT)
        self.assertEqual(result['tag'], 'v1.0.0')

    def test_skips_forks_prs_failed_runs_wrong_branches_and_expired_artifacts(self):
        self.runs = {1: [run(8, head_repository={'full_name': 'fork/pascal'}),
                         run(7, event='pull_request'), run(6, conclusion='failure'),
                         run(5, head_branch='feature'), run(4), run(3), run(2), run(1)]}
        self.artifacts = {4: [artifact(expires_at='2000-01-01T00:00:00Z')],
                          3: [artifact(name='logs')], 2: [], 1: [artifact(99)]}
        self.assertEqual(self.select()['artifact_id'], 99)

    def test_rejects_wrong_branch_and_missing_or_ambiguous_artifacts(self):
        with self.assertRaisesRegex(ValueError, 'default branch'):
            self.select(ref='refs/heads/feature')
        self.artifacts = {}
        with self.assertRaisesRegex(ValueError, 'No retained'):
            self.select()
        self.artifacts = {1: [artifact(1), artifact(2)]}
        with self.assertRaisesRegex(ValueError, 'ambiguous'):
            self.select()
        self.assertEqual(self.calls, [])

    def test_existing_tags_and_drafts_are_never_overwritten(self):
        self.tag_exists = True
        with self.assertRaisesRegex(ValueError, 'already exists'):
            self.select()
        self.tag_exists = False
        self.releases = [{'tag_name': 'v1.0.0', 'draft': True}]
        with self.assertRaisesRegex(ValueError, 'already exists'):
            self.select()
        self.assertEqual(self.calls, [])

    def test_publishes_original_bytes_after_tag_and_draft_creation(self):
        self.publish()
        self.assertEqual([call[1] for call in self.calls], ['POST', 'POST', 'POST', 'PATCH'])
        self.assertEqual(self.calls[0][2], {'ref': 'refs/tags/v1.0.0', 'sha': COMMIT})
        self.assertEqual(self.calls[1][2]['target_commitish'], COMMIT)
        self.assertTrue(self.calls[1][2]['draft'])
        self.assertFalse(self.calls[1][2]['prerelease'])
        self.assertEqual(self.calls[2][3].read_bytes(), b'PK\x03\x04distribution bytes')
        self.assertIn('name=zeal-pascal-v1.0.0.zip', self.calls[2][0])
        self.assertEqual(self.calls[3][2], {'draft': False})

    def test_prerelease_versions_create_prereleases(self):
        self.publish('v2.0.0-rc.1')
        self.assertTrue(self.calls[1][2]['prerelease'])

    def test_release_notes_include_download_installation_and_generated_changelog(self):
        for tag in ('v1.0.0', 'v2.0.0-rc.1'):
            with self.subTest(tag=tag):
                self.calls.clear()
                self.publish(tag)
                payload = self.calls[1][2]
                self.assertTrue(payload['generate_release_notes'])
                body = payload['body']
                asset_name = f'zeal-pascal-{tag}.zip'
                self.assertIn(f'https://github.com/{REPO}/releases/download/{tag}/{asset_name}', body)
                self.assertIn(f'https://github.com/{REPO}/blob/{tag}/README.md#download-and-install', body)
                self.assertIn('`bin/`', body)
                self.assertIn('`examples/`', body)
                self.assertIn('**Source code** archives do not contain the built distribution', body)
                self.assertIn(f'Built from commit `{COMMIT}`', body)
                self.assertIn('[Build and artifact](https://build)', body)
                self.assertTrue(body.endswith('## Changelog\n'))
                self.assertIn(f'name={asset_name}', self.calls[2][0])

    def test_missing_or_invalid_archive_creates_no_tag(self):
        self.archive.write_bytes(b'invalid')
        with self.assertRaisesRegex(ValueError, 'ZIP'):
            self.publish()
        self.archive.unlink()
        with self.assertRaises(FileNotFoundError):
            self.publish()
        self.assertEqual(self.calls, [])

    def test_main_writes_selected_metadata_to_github_output(self):
        output = Path(self.temp.name) / 'github-output'
        env = {'GITHUB_REPOSITORY': REPO, 'GITHUB_REF': 'refs/heads/main', 'GITHUB_OUTPUT': str(output)}
        with patch.dict(os.environ, env), patch.object(sys, 'argv', ['release.py', 'select', '--version', '1.0.0']):
            release.main()
        values = dict(line.split('=', 1) for line in output.read_text().splitlines())
        self.assertEqual(values['commit'], COMMIT)
        self.assertEqual(values['tag'], 'v1.0.0')
        self.assertEqual(values['artifact_id'], '10')
        self.assertEqual(values['run_id'], '1')

    def test_second_page_release_collision_is_rejected(self):
        def api(endpoint):
            if '/git/ref/' in endpoint:
                raise FileNotFoundError(endpoint)
            if endpoint.endswith('page=1'):
                return [{'tag_name': 'other'}] * 100
            return [{'tag_name': 'v1.0.0', 'draft': True}]
        with patch.object(release, 'api', side_effect=api):
            with self.assertRaisesRegex(ValueError, 'already exists'):
                release.ensure_unused(REPO, 'v1.0.0')

    def test_second_page_distribution_artifact_is_found(self):
        api = self.api
        def paged(endpoint, *args, **kwargs):
            if '/artifacts?' in endpoint:
                if endpoint.endswith('page=1'):
                    return {'artifacts': [artifact(name='logs')] * 100}
                return {'artifacts': [artifact(55)]}
            return api(endpoint, *args, **kwargs)
        with patch.object(release, 'api', side_effect=paged):
            self.assertEqual(self.select()['artifact_id'], 55)

    def test_upload_failure_keeps_draft_unpublished(self):
        self.upload_error = True
        with self.assertRaisesRegex(RuntimeError, 'upload failed'):
            self.publish()
        self.assertEqual([call[1] for call in self.calls], ['POST', 'POST', 'POST'])


class GitHubCLI(unittest.TestCase):
    def test_only_http_404_is_treated_as_missing(self):
        for status, exception in ((404, FileNotFoundError), (403, RuntimeError), (500, RuntimeError)):
            result = subprocess.CompletedProcess([], 1, '', f'gh: failed (HTTP {status})')
            with self.subTest(status=status), patch.object(release.subprocess, 'run', return_value=result):
                with self.assertRaises(exception):
                    release.api('repos/owner/pascal')

    def test_json_is_sent_as_stdin_and_archive_as_raw_file(self):
        result = subprocess.CompletedProcess([], 0, '{}', '')
        with patch.object(release.subprocess, 'run', return_value=result) as execute:
            release.api('repos/owner/pascal/git/refs', 'POST', {'sha': COMMIT})
            self.assertEqual(json.loads(execute.call_args.kwargs['input']), {'sha': COMMIT})
            self.assertEqual(execute.call_args.args[0][-2:], ['--input', '-'])
            release.api('https://uploads.github.com/assets', 'POST', upload=Path('package.zip'))
            self.assertEqual(execute.call_args.args[0][4], 'https://uploads.github.com/assets')
            self.assertEqual(execute.call_args.args[0][-2:], ['--input', 'package.zip'])
            self.assertIsNone(execute.call_args.kwargs['input'])


if __name__ == '__main__':
    unittest.main()
