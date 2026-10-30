#!/usr/bin/env python3
"""Run the distinct product checks once; duplicate diagnostic runs are opt-in."""
import argparse
from datetime import datetime, timezone
import fcntl
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


def commands(include_redundant=False):
    plan = json.loads(PLAN.read_text())
    selected = plan['full'] + (plan['redundant'] if include_redundant else [])
    return [shlex.split(line) for line in selected]


def dependencies(plan, index):
    """Separate successful prerequisites from shared-output ordering."""
    command = plan[index-1]
    required = {1} if index != 1 else set()
    if '@params' in command:
        required.update(i for i, cmd in enumerate(plan, 1)
                        if cmd[0] == 'tools/test-params.py')
    ordered = {i for i, cmd in enumerate(plan, 1)
               if i < index and cmd[0] == command[0]}
    return required, ordered


def inputs():
    paths = [ROOT / 'CMakeLists.txt', PLAN, Path(__file__).resolve(),
             ROOT / 'tests/test_runner.py']
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
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    plan = commands(args.include_redundant)
    if args.list:
        for index, command in enumerate(plan, 1):
            print(f'{index:02}: python3 {shlex.join(command)}')
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
                slots = min(3, args.jobs) if original[0] == 'tools/test-output-interrupt.py' else 1
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
        report['passed'] = not report['changed_inputs'] and not failed and not skipped
        report['status'] = 'failed' if failed or skipped else 'passed' if report['passed'] else 'inputs-changed'
        return 0 if report['passed'] else 1
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
