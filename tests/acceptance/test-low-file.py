#!/usr/bin/env python3
"""Build and verify the isolated low-file proof, retaining incremental ZDE provenance."""
import datetime
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    out = ROOT/'analysis/low-file-gate'/f'{stamp}-{os.getpid()}'
    out.mkdir(parents=True)
    sources = [ROOT/'CMakeLists.txt']
    for folder in ('src', 'tests/native', 'tools', 'cmake'):
        sources += sorted(p for p in (ROOT/folder).rglob('*') if p.is_file() and p.suffix in
                          ('.c', '.h', '.asm', '.py', '.cmake', '.in'))
    inputs = {str(p.relative_to(ROOT)): sha(p) for p in sources}
    report = {'scope': 'Incremental ZDE rebuild followed by authenticated acceptance; not a clean-build guarantee',
              'passed': False, 'input_sha256': inputs, 'commands': []}
    manifest = out/'manifest.json'

    def save():
        manifest.write_text(json.dumps(report, indent=2)+'\n')

    def run(command, name, expected=None):
        log = out/(name+'.log')
        print(name, flush=True)
        with log.open('w') as stream:
            result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
        entry = {'command': command, 'returncode': result.returncode,
                 'log': str(log.relative_to(ROOT)), 'sha256': sha(log)}
        report['commands'].append(entry)
        save()
        assert result.returncode == 0, entry
        if expected:
            assert expected in log.read_text(), (name, expected)

    save()
    run(['zde', '--no-tty', 'exec', 'cmake', '-S', '.', '-B', 'build', '-DZP_BUILD_TESTS=ON'], 'configure')
    run(['zde', '--no-tty', 'cmake', 'build', '--target', 'zlowcopytest_bin', 'zcompacttest_bin',
         'zfilecoretest_bin', 'zpascal_bin', '--parallel', '6'], 'build')
    artifacts = sorted(p for p in (ROOT/'build').iterdir() if p.is_file() and
                       p.suffix in ('.bin', '.ihx', '.map', '.zpl', '.packed', '.private', '.profile', '.json'))
    outputs = {str(p.relative_to(ROOT)): sha(p) for p in artifacts}
    report['build_output_sha256'] = outputs
    save()
    run([sys.executable, 'tests/acceptance/test-file-runtime.py', '--host', '--compact-host'], 'host')
    run([sys.executable, 'tests/acceptance/test-low-copy.py'], 'bridge')
    run([sys.executable, 'tests/acceptance/test-compact.py'], 'integration')
    run([sys.executable, 'tests/acceptance/test-file-runtime.py', '--native'], 'normal-file')
    run([sys.executable, 'tests/acceptance/test-runtime-profile.py'], 'packaging',
        'PASS: 63 private-runtime packaging and no-publication checks')
    report['unchanged_sources'] = all(sha(ROOT/p) == h for p, h in inputs.items())
    report['unchanged_build_outputs'] = all(sha(ROOT/p) == h for p, h in outputs.items())
    report['passed'] = report['unchanged_sources'] and report['unchanged_build_outputs']
    save()
    assert report['passed'], manifest
    print(manifest.relative_to(ROOT), flush=True)


if __name__ == '__main__':
    main()
