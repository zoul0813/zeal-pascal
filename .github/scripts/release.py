#!/usr/bin/env python3
"""Select a retained build artifact and release its exact commit using GitHub CLI."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
from datetime import datetime, timezone
from urllib.parse import urlencode


def api(endpoint, method='GET', payload=None, upload=None):
    command = ['gh', 'api', '--method', method, endpoint,
               '--header', 'Accept: application/vnd.github+json',
               '--header', 'X-GitHub-Api-Version: 2022-11-28']
    data = None
    if upload is not None:
        command += ['--header', 'Content-Type: application/zip', '--input', str(upload)]
    elif payload is not None:
        command += ['--header', 'Content-Type: application/json', '--input', '-']
        data = json.dumps(payload)
    result = subprocess.run(command, input=data, capture_output=True, text=True)
    if result.returncode:
        # gh prints the HTTP status for API errors. Treat only 404 as absent.
        if re.search(r'\bHTTP 404\b', result.stderr):
            raise FileNotFoundError(endpoint)
        raise RuntimeError(result.stderr.strip() or f'GitHub API failed: {endpoint}')
    return json.loads(result.stdout) if result.stdout.strip() else None


def pages(endpoint, key=None):
    separator = '&' if '?' in endpoint else '?'
    page = 1
    while True:
        response = api(f'{endpoint}{separator}per_page=100&page={page}')
        values = response[key] if key is not None else response
        yield values
        if len(values) < 100:
            return
        page += 1


def version_tag(version):
    value = version.strip()
    if len(value) > 100 or not re.fullmatch(r'v?(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)\.(?:0|[1-9][0-9]*)(?:-[0-9A-Za-z]+(?:[.-][0-9A-Za-z]+)*)?', value):
        raise ValueError('Use a version such as 1.0.0, v1.0.0 or 1.1.0-rc.1.')
    return value if value.startswith('v') else 'v' + value


def ensure_unused(repository, tag):
    try:
        api(f'repos/{repository}/git/ref/tags/{tag}')
    except FileNotFoundError:
        pass
    else:
        raise ValueError(f'Tag {tag} already exists; existing versions are never overwritten.')
    for releases in pages(f'repos/{repository}/releases'):
        if any(release['tag_name'] == tag for release in releases):
            raise ValueError(f'Release {tag} already exists (possibly as a draft). Inspect it before retrying.')


def select(repository, ref, version):
    tag = version_tag(version)
    branch = api(f'repos/{repository}')['default_branch']
    if ref != f'refs/heads/{branch}':
        raise ValueError(f'Run Create release from the default branch ({branch}).')
    ensure_unused(repository, tag)
    query = urlencode({'branch': branch, 'status': 'success'})
    endpoint = f'repos/{repository}/actions/workflows/build.yml/runs?{query}'
    for runs in pages(endpoint, 'workflow_runs'):
        for run in runs:
            if (run['status'] != 'completed' or run['conclusion'] != 'success'
                    or run['head_branch'] != branch or run['event'] not in ('push', 'workflow_dispatch')
                    or (run.get('head_repository') or {}).get('full_name', '').lower() != repository.lower()):
                continue
            candidates = []
            for artifacts in pages(f'repos/{repository}/actions/runs/{run["id"]}/artifacts', 'artifacts'):
                for artifact in artifacts:
                    if artifact['name'] == 'zeal-pascal-dist' and not artifact['expired']:
                        expires = datetime.fromisoformat(artifact['expires_at'].replace('Z', '+00:00'))
                        if expires > datetime.now(timezone.utc):
                            candidates.append(artifact)
            if not candidates:
                continue
            if len(candidates) != 1:
                raise ValueError(f'Build {run["id"]} has ambiguous distribution artifacts.')
            if not re.fullmatch(r'[0-9a-f]{40}', run['head_sha']):
                raise ValueError('Build has an invalid source commit.')
            return {'tag': tag, 'commit': run['head_sha'], 'run_id': run['id'],
                    'artifact_id': candidates[0]['id'], 'run_url': run['html_url']}
    raise ValueError('No retained distribution artifact from a successful default-branch build. Run Build distribution first.')


def publish(repository, tag, commit, run_url, archive):
    if version_tag(tag) != tag or not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError('Invalid release tag or source commit.')
    # Validate before mutating GitHub: missing downloads create no tag.
    with archive.open('rb') as stream:
        if stream.read(4) != b'PK\x03\x04':
            raise ValueError('Distribution archive is not a nonempty ZIP file.')
    ensure_unused(repository, tag)
    api(f'repos/{repository}/git/refs', 'POST', {'ref': f'refs/tags/{tag}', 'sha': commit})
    try:
        release = api(f'repos/{repository}/releases', 'POST', {
            'tag_name': tag, 'target_commitish': commit, 'name': f'Zeal Pascal {tag}',
            'draft': True, 'prerelease': '-' in tag, 'generate_release_notes': True,
            'body': f'Built from commit `{commit}`.\n\n[Build and artifact]({run_url}).',
        })
        upload_url = release['upload_url'].split('{', 1)[0]
        upload_url += '?' + urlencode({'name': f'zeal-pascal-{tag}.zip'})
        api(upload_url, 'POST', upload=archive)
        api(f'repos/{repository}/releases/{release["id"]}', 'PATCH', {'draft': False})
        print(f'Published {tag} from {commit}: {release["html_url"]}')
    except Exception:
        print(f'Release failed after creating {tag}. Inspect its tag/draft before retrying; nothing was deleted or overwritten.')
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    selector = commands.add_parser('select')
    selector.add_argument('--version', required=True)
    publisher = commands.add_parser('publish')
    publisher.add_argument('--tag', required=True)
    publisher.add_argument('--commit', required=True)
    publisher.add_argument('--run-url', required=True)
    publisher.add_argument('--archive', type=Path, required=True)
    args = parser.parse_args()
    repository = os.environ['GITHUB_REPOSITORY']
    try:
        if args.command == 'select':
            result = select(repository, os.environ['GITHUB_REF'], args.version)
            with open(os.environ['GITHUB_OUTPUT'], 'a', encoding='utf-8') as stream:
                for key, value in result.items():
                    stream.write(f'{key}={value}\n')
            print(f'Selected {result["run_url"]} at {result["commit"]} for {result["tag"]}.')
        else:
            publish(repository, args.tag, args.commit, args.run_url, args.archive)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
