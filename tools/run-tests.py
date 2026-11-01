#!/usr/bin/env python3
"""Run the distinct product checks once; duplicate diagnostic runs are opt-in."""
import argparse
from datetime import datetime, timezone
import fcntl
import fnmatch
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import signal
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
PLAN = ROOT / 'tools/test-plan.json'


def test_id(command):
    base = Path(command[0]).stem.removeprefix('test-')
    options = [part.lstrip('-') for part in command[1:]
               if not part.startswith('@') and part not in ('--case', '--shard', '--corpus')]
    return ':'.join([base, *options])


def expand_commands(definition, include_redundant=False, monolithic=False):
    selected = definition['full'] + (definition['redundant'] if include_redundant else [])
    chunks = {} if monolithic else definition.get('chunks', {})
    if not selected or shlex.split(selected[0])[0] != 'tools/test-native-host.py':
        raise ValueError('Test plan must begin with the host prerequisite')
    expanded = []
    for line in selected:
        suffixes = chunks.get(line, [''])
        if not suffixes:
            raise ValueError(f'Empty chunks would omit coverage for {line}')
        parts = [shlex.split(suffix) for suffix in suffixes]
        if any('--shard' in part for part in parts):
            try:
                if any(len(part) != 2 or part[0] != '--shard' for part in parts):raise ValueError
                shards = [tuple(map(int, part[1].split('/'))) for part in parts]
                total = shards[0][1]
                if not 1 <= total <= 200 or any(len(shard) != 2 or shard[1] != total for shard in shards):raise ValueError
                if sorted(index for index, count in shards) != list(range(1, total+1)):raise ValueError
            except (ValueError, IndexError):
                raise ValueError(f'Chunks for {line} must contain each shard 1/TOTAL through TOTAL/TOTAL exactly once') from None
        expanded.extend(shlex.split(line) + part for part in parts)
    return expanded


def select_commands(plan, patterns):
    """Select explicit tests plus the successful prerequisites they consume."""
    if not patterns:
        return plan
    selected = set()
    for pattern in patterns:
        matches = {i for i, cmd in enumerate(plan) if
                   any(fnmatch.fnmatchcase(value, pattern) for value in
                       (test_id(cmd), test_id(cmd).split(':')[0], cmd[0], Path(cmd[0]).name))}
        if not matches:
            raise ValueError(f'No tests match {pattern!r}; inspect --list')
        selected.update(matches)
    while True:
        required = {dep-1 for index in selected for dep in dependencies(plan, index+1)[0]}
        if required <= selected:
            return [cmd for index, cmd in enumerate(plan) if index in selected]
        selected.update(required)


def isolated_chunk(command):
    # These modes only read build inputs and write timestamp+PID capture paths.
    # Other runners may compile into fixed host directories: keep them ordered.
    return ((command[0] in ('tools/test-editor-native.py', 'tools/test-native.py',
                            'tools/test-math.py', 'tools/test-files.py', 'tools/test-options.py',
                            'tools/test-text.py', 'tools/test-output-interrupt.py') and '--case' in command) or
            (command[0] == 'tools/test-language-conformance.py' and '--shard' in command))


def worker_slots(command, budget):
    return min(3, budget) if command[0] == 'tools/test-output-interrupt.py' and '--case' not in command else 1


def dependencies(plan, index):
    """Separate successful prerequisites from shared-output ordering."""
    command = plan[index-1]
    required = {1} if index != 1 else set()
    if '@params' in command:
        required.update(i for i, cmd in enumerate(plan, 1)
                        if cmd[0] == 'tools/test-params.py')
    ordered = {i for i, cmd in enumerate(plan, 1)
               if i < index and cmd[0] == command[0] and
               not (isolated_chunk(command) and isolated_chunk(cmd))}
    return required, ordered


def inputs():
    paths = [ROOT / 'CMakeLists.txt', PLAN, Path(__file__).resolve(),
             ROOT / 'tests/test_runner.py', ROOT / 'tests/test_native_chunks.py']
    for folder in ('src', 'tests/native', 'tools', 'examples'):
        paths.extend(p for p in (ROOT / folder).rglob('*')
                     if p.is_file() and '__pycache__' not in p.parts)
    paths.extend(p for p in (ROOT / 'build').iterdir()
                 if p.is_file() and p.suffix in
                 ('.bin', '.zpl', '.map', '.json', '.h', '.packed', '.private', '.profile'))
    return {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(set(paths))}


def resolve(command, host_log, params_reports):
    command = list(command)
    if '@observed' in command:
        text = host_log.read_text()
        values = [re.search(rf'{phase} checkpoint: (\d+)', text)
                  for phase in ('parser', 'seal')]
        if not all(values):
            raise RuntimeError('Host checks did not establish cancellation checkpoints')
        command = [command[0], '--parser-poll', values[0][1], '--seal-poll', values[1][1]]
    if '@params' in command:
        fresh = set((ROOT / 'analysis/native-slice').glob('*/params-report.json')) - params_reports
        if len(fresh) != 1:
            raise RuntimeError('Expected one parameter report from this validation run')
        report = json.loads(fresh.pop().read_text())
        if not report['passed']:
            raise RuntimeError('Parameter suite did not pass')
        program = ROOT / report['capture_directory'] / 'params-component/hostfs/PARAMS.COM'
        if not program.is_file():
            raise RuntimeError('Parameter suite did not produce PARAMS.COM')
        command = [command[0], str(program)]
    return [sys.executable, '-u', *command]


def stop(child):
    # Each runner owns its emulator children. Stop the entire owned group,
    # including an emulator left alive after a runner failure or interruption.
    try:
        os.killpg(child.pid, signal.SIGTERM)
    except ProcessLookupError:
        return
    try:
        child.wait(timeout=5)
    except subprocess.TimeoutExpired:
        pass
    try:
        os.killpg(child.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    child.wait()


def group_exists(child):
    try:
        os.killpg(child.pid, 0)
        return True
    except ProcessLookupError:
        return False


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list', action='store_true', help='Print the plan without executing tests')
    parser.add_argument('--include-redundant', action='store_true', help='Also repeat the three diagnostic subsets')
    parser.add_argument('--jobs', type=int, default=4, help='Concurrent worker budget (default: 4; use 1 for serial diagnostics)')
    parser.add_argument('--keep-going', action='store_true', help='Collect independent failures; skip their dependents, never waive failures')
    parser.add_argument('--only', action='append', default=[], metavar='ID_OR_GLOB',
                        help='Select a test ID, runner name/path, or quoted glob (repeatable)')
    parser.add_argument('--group', action='append', default=[], metavar='NAME',
                        help='Select a named group from test-plan.json (repeatable)')
    parser.add_argument('--monolithic', action='store_true', help='Use the original unchunked language/editor commands')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    definition = json.loads(PLAN.read_text())
    patterns = list(args.only)
    for group in args.group:
        if group not in definition.get('groups', {}):
            parser.error(f'Unknown group {group!r}; choices: {", ".join(definition.get("groups", {}))}')
        patterns.extend(definition['groups'][group])
    try:
        plan = select_commands(expand_commands(definition, args.include_redundant, args.monolithic), patterns)
    except ValueError as error:
        parser.error(str(error))
    scope = 'focused' if args.only or args.group else 'full'
    if args.list:
        print(f'Scope: {scope}; {len(plan)} commands including prerequisites; groups: {", ".join(definition.get("groups", {}))}')
        for index, command in enumerate(plan, 1):
            print(f'{index:02}: {test_id(command)} — python3 {shlex.join(command)}')
        return 0

    # Fixed host output paths are shared across runs. Only one orchestrator may
    # own a checkout at a time; its native media directories remain independent.
    runs = ROOT / 'analysis/test-runs'
    runs.mkdir(parents=True, exist_ok=True)
    lock = (runs / '.lock').open('w')
    try:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
    except BlockingIOError:
        print('Another validation run already owns this checkout', file=sys.stderr)
        lock.close()
        return 1
    directory = runs / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    directory.mkdir(parents=True)
    frozen = inputs()
    report = {'status': 'running', 'passed': False, 'commands': plan, 'jobs': args.jobs,
              'scope': scope, 'full_acceptance': False, 'selected_passed': False, 'selection': patterns,
              'input_sha256': frozen, 'results': [], 'skipped': []}
    state = directory / 'suite.json'

    def save():
        state.write_text(json.dumps(report, indent=2) + '\n')

    cancelled = False

    def interrupted(signum, frame):
        # A flag avoids interruption between spawning and registering a child,
        # and repeated signals cannot abandon cleanup halfway through.
        nonlocal cancelled
        cancelled = True

    previous = {sig: signal.signal(sig, interrupted)
                for sig in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP)}
    active = {}
    pending = list(enumerate(plan, 1))
    completed = set()
    failed = set()
    skipped = set()
    params_reports = set((ROOT / 'analysis/native-slice').glob('*/params-report.json'))
    host_log = directory / '01-test-native-host.log'
    save()
    print(f'Reports: {directory}', flush=True)
    try:
        while pending or active:
            if cancelled:
                raise KeyboardInterrupt
            for index, original in list(pending):
                if cancelled:
                    raise KeyboardInterrupt
                # First run cheap host checks; then schedule independent native
                # work. Modes of the same script share host output paths.
                required, ordered = dependencies(plan, index)
                blocked = required & (failed | skipped)
                if blocked:
                    skipped.add(index)
                    report['skipped'].append({'index': index, 'blocked_by': sorted(blocked)})
                    pending.remove((index, original))
                    continue
                slots = worker_slots(original, args.jobs)
                if not required <= completed or not ordered <= (completed | failed | skipped):
                    continue
                if sum(v['slots'] for v in active.values()) + slots > args.jobs:
                    # Reserve capacity rather than starving a multi-slot suite
                    # by continually admitting later one-slot commands.
                    break
                command = resolve(original, host_log, params_reports)
                if original[0] == 'tools/test-output-interrupt.py':
                    command += ['--jobs', str(slots)]
                log = directory / f'{index:02}-{Path(original[0]).stem}.log'
                output = log.open('w')
                try:
                    child = subprocess.Popen(command, cwd=ROOT, stdout=output,
                                             stderr=subprocess.STDOUT, start_new_session=True)
                except BaseException:
                    output.close()
                    raise
                active[index] = {'child': child, 'output': output, 'slots': slots,
                                 'start': time.monotonic(), 'notice': time.monotonic(),
                                 'command': command, 'log': str(log)}
                pending.remove((index, original))
                print(f'[{index}/{len(plan)}] START {shlex.join(command)}', flush=True)
            if pending and not active:
                raise RuntimeError('Test dependencies cannot be scheduled')
            report['running'] = [{k: v for k, v in task.items() if k in ('command', 'log')}
                                 | {'index': index} for index, task in active.items()]
            save()
            time.sleep(1)
            for index, task in list(active.items()):
                code = task['child'].poll()
                now = time.monotonic()
                if code is None:
                    if now - task['notice'] >= 60:
                        print(f'[{index}/{len(plan)}] running {now-task["start"]:.0f}s; log: {task["log"]}', flush=True)
                        task['notice'] = now
                    continue
                leftover = group_exists(task['child'])
                stop(task['child'])
                task['output'].close()
                del active[index]
                report['results'].append({'index': index, 'command': task['command'], 'exit_code': code,
                                          'leftover_processes': leftover,
                                          'seconds': round(now-task['start'], 3), 'log': task['log']})
                ok = code == 0 and not leftover
                print(f'[{index}/{len(plan)}] {"PASS" if ok else "FAIL"} {task["log"]}', flush=True)
                save()
                if ok:
                    completed.add(index)
                else:
                    failed.add(index)
                    if not args.keep_going:
                        report['status'] = 'failed'
                        return 1
        current = inputs()
        report['changed_inputs'] = sorted(p for p in frozen.keys() | current.keys()
                                          if frozen.get(p) != current.get(p))
        if cancelled:
            raise KeyboardInterrupt
        report['selected_passed'] = not report['changed_inputs'] and not failed and not skipped
        report['full_acceptance'] = report['selected_passed'] and scope == 'full'
        report['passed'] = report['full_acceptance']
        report['status'] = ('failed' if failed or skipped else 'inputs-changed' if not report['selected_passed']
                            else 'passed-focused' if scope == 'focused' else 'passed')
        return 0 if report['selected_passed'] else 1
    except KeyboardInterrupt:
        report['status'] = 'interrupted'
        return 130
    except Exception as error:
        report.update(status='error', error=str(error))
        print(f'ERROR: {error}', file=sys.stderr)
        return 1
    finally:
        for index, task in active.items():
            stop(task['child'])
            task['output'].close()
            report.setdefault('cancelled', []).append(index)
        report['not_started'] = [index for index, command in pending]
        report.pop('running', None)
        report['finished_utc'] = datetime.now(timezone.utc).isoformat()
        save()
        for sig, handler in previous.items():
            signal.signal(sig, handler)
        lock.close()


if __name__ == '__main__':
    sys.exit(main())
