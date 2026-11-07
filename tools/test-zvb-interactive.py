#!/usr/bin/env python3
"""Exercise saved Snake's normal controls with real emulator keyboard events."""
import argparse
import hashlib
import importlib.util
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snake', type=Path, default=ROOT/'build/zvb/SNAKE.COM')
    parser.add_argument('--emulator', type=Path)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('native', ROOT/'tools/test-native.py')
    n = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(n)
    if args.emulator:
        n.EMULATOR = args.emulator
    source = (ROOT/'examples/zvb/SNAKE.PAS').read_text()
    # Pin the observer to the main program's declared storage, not incidental
    # procedure workspaces or code addresses. Fail if that declaration changes.
    declaration = source.split('var Context: TGfxContext;', 1)[1].split('{ BEGIN GENERATED INCLUDE: VIDEO.INC }', 1)[0]
    assert ' '.join(declaration.split()) == ' '.join('''
      { Zero is empty;1..4 links an occupied cell toward the head. }
      Board: array[0..279] of Byte;
      TailX, TailY, HeadX, HeadY: Byte;
      SnakeLength, Score: Integer;
      Frames: Word;
      Direction, NextDirection, AppleX, AppleY, Tick, SoundFrames: Byte;
      Error, OldDirection: Byte;
      GameOver, Quit, Demo, SoundUsed: Boolean;
      Key: Char;
      Waiting: Boolean;
      Speed: Byte;
    '''.split()), 'Snake storage observer needs updating'
    assert 'VideoMode, BitsPerPixel, AlphaIndex: Byte;\n    VirtualPage, BackupPage: Byte;' in (ROOT/'libraries/ZVB.PAS').read_text()
    private_names = ['RP_INPUT_READY'] + re.findall(r'RP_[A-Z_]+', (ROOT/'src/compiler.h').read_text().split('enum { RP_INPUT_READY', 1)[1].split('RP_COUNT }', 1)[0])
    private = (ROOT/'build/pascal_runtime.private').read_bytes()
    raw = struct.unpack_from('<H', private, 2*private_names.index('RP_INPUT_RAW_KEY'))[0]
    image = args.snake.read_bytes()
    observations = {}
    checks = {}
    base = 0x8000 + 5 + 280

    class Controls(n.DebugConsole):
        def command(self, command, capture=False):
            if command != '@snake-controls':
                return super().command(command, capture)
            call = lambda value: super(Controls, self).command(value)

            def sample(name):
                mem = n.memory(call(f'rb 0x{base:x} 24'))
                word = lambda offset: mem[base+offset] | mem[base+offset+1] << 8
                state = dict(tail=mem[base]+20*mem[base+1],
                             head=mem[base+2]+20*mem[base+3], length=word(4),
                             score=word(6), frames=word(8),
                             direction=mem[base+10], next_direction=mem[base+11],
                             game_over=mem[base+18], quit=mem[base+19],
                             demo=mem[base+20], key=mem[base+22], waiting=mem[base+23])
                observations[name] = state
                return state

            call(f'bp 0x{raw:x}')
            call('run 1000000000')
            registers = call('regs')
            assert f'PC={raw:04x} ' in registers, 'Snake never reached input'
            call(f'bc 0x{raw:x}')
            call('s')
            call('run 5000000')
            ready = sample('ready')
            checks['normal mode waits at start'] = ready['waiting'] == 1 and ready['head'] == 150 and ready['tail'] == 149 and not ready['game_over'] and not ready['demo']
            call('tap UP')
            call('run 5000000')
            up = sample('up')
            checks['arrow starts and steers'] = not up['waiting'] and up['direction'] == 0 and up['head'] < 150 and not up['game_over']
            # Observe VRAM through the target's own bounded mapping leaf.
            # Pause after OUT(F0), before it writes the first reset row; do not
            # change MMU state or inject code. The previous board remains live.
            mapper = bytes.fromhex('dd7e0ed3f078b1')
            assert image.count(mapper) == 1
            mapped = 0x4000 + image.index(mapper) + 5
            call(f'bp 0x{mapped:x}')
            call('tap R')
            call('run 5000000')
            assert f'PC={mapped:04x} ' in call('regs')
            field = n.memory(call('rb 0x0000 1140'))
            label = n.memory(call('rb 0x146a 9'))
            checks['original checkerboard layout'] = all(
                field[y*80+x] == 32 + ((x+y) & 1)
                for y in range(14) for x in range(20))
            checks['original dark score strip'] = all(field[14*80+x] == 34 for x in range(20))
            checks['original SCORE colon and three digits'] = bytes(label[0x146a+x] for x in range(9)) == bytes([65,66,67,68,69,58,48,48,48])
            call(f'bc 0x{mapped:x}')
            call('s')
            call('run 5000000')
            restart = sample('restart')
            checks['R resets and waits'] = restart['waiting'] == 1 and restart['head'] == 150 and restart['tail'] == 149 and not restart['game_over'] and restart['score'] == 0
            call('tap D')
            call('run 5000000')
            right = sample('right')
            checks['WASD starts and steers'] = not right['waiting'] and right['direction'] == 1 and right['head'] > 150
            call('run 50000000')
            dead = sample('wall')
            checks['wall collision reached'] = dead['game_over'] == 1
            call('tap R')
            call('run 5000000')
            restart = sample('restart-after-wall')
            checks['R works after game over'] = restart['waiting'] == 1 and restart['head'] == 150 and not restart['game_over']
            call('tap ESCAPE')
            call('run 10000000')
            registers = call('regs')
            pc = int(re.search(r'PC=([0-9a-fA-F]+)', registers)[1], 16)
            checks['Escape returns to shell'] = pc < 0x4000
            return registers

    commands = ['run 100000000'] + n.type_text('H:/zvb/SNAKE.COM') + [
        'bp 0x4000', 'tap ENTER', 'run 2000000000', 'bc 0x4000', 's',
        '@snake-controls', 'quit']
    n.execute('snake-interactive', commands, {'zvb/SNAKE.COM': image},
              timeout=180, console_class=Controls)
    failure_path = n.RUN/'snake-interactive/input-error.json'
    failure = json.loads(failure_path.read_text()) if failure_path.exists() else None
    checks['all control and layout observations completed'] = not failure and len(checks) == 10
    console = (n.RUN/'snake-interactive/console-all.txt').read_text()
    cycles = re.findall(r'Paused @ 0x[0-9a-fA-F]+ \(cyc=(\d+)\)', console)
    startup_cycles = int(cycles[1]) - int(cycles[0]) if len(cycles) >= 2 else None
    report = dict(passed=all(checks.values()), checks=checks,
                  observer_failure=failure, initialization_cycles=startup_cycles,
                  observations=observations, emulator=str(n.EMULATOR),
                  saved_bytes=len(image), saved_sha256=hashlib.sha256(image).hexdigest(),
                  capture_directory=str(n.RUN.relative_to(ROOT)))
    (n.RUN/'snake-interactive-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
