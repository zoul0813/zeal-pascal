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
      Body: array[0..255] of TPoint;
      Occupied: array[0..279] of Boolean;
      Tail, Head, SnakeLength, Score: Integer;
      Frames: Word;
      Direction, NextDirection, AppleX, AppleY, Tick, SoundFrames: Byte;
      Error, OldDirection: Byte;
      GameOver, Quit, Demo, SoundUsed: Boolean;
      Key: Char;
      Waiting: Boolean;
    '''.split()), 'Snake storage observer needs updating'
    assert 'VideoMode, BitsPerPixel, AlphaIndex: Byte;\n    VirtualPage, BackupPage: Byte;' in (ROOT/'libraries/ZVB.PAS').read_text()
    private_names = ['RP_INPUT_READY'] + re.findall(r'RP_[A-Z_]+', (ROOT/'src/compiler.h').read_text().split('enum { RP_INPUT_READY', 1)[1].split('RP_COUNT }', 1)[0])
    private = (ROOT/'build/pascal_runtime.private').read_bytes()
    raw = struct.unpack_from('<H', private, 2*private_names.index('RP_INPUT_RAW_KEY'))[0]
    image = args.snake.read_bytes()
    observations = {}
    checks = {}
    base = 0x8000 + 5 + 512 + 280

    class Controls(n.DebugConsole):
        def command(self, command, capture=False):
            if command != '@snake-controls':
                return super().command(command, capture)
            call = lambda value: super(Controls, self).command(value)

            def sample(name):
                mem = n.memory(call(f'rb 0x{base:x} 24'))
                word = lambda offset: mem[base+offset] | mem[base+offset+1] << 8
                state = dict(tail=word(0), head=word(2), length=word(4),
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
            checks['normal mode waits at start'] = ready['waiting'] == 1 and ready['head'] == 1 and ready['tail'] == 0 and not ready['game_over'] and not ready['demo']
            call('tap UP')
            call('run 2000000')
            up = sample('up')
            checks['arrow starts and steers'] = not up['waiting'] and up['direction'] == 0 and up['head'] > 1 and not up['game_over']
            call('tap R')
            call('run 5000000')
            restart = sample('restart')
            checks['R resets and waits'] = restart['waiting'] == 1 and restart['head'] == 1 and restart['tail'] == 0 and not restart['game_over'] and restart['score'] == 0
            call('tap D')
            call('run 2000000')
            right = sample('right')
            checks['WASD starts and steers'] = not right['waiting'] and right['direction'] == 1 and right['head'] > 1
            call('run 15000000')
            dead = sample('wall')
            checks['wall collision reached'] = dead['game_over'] == 1
            call('tap R')
            call('run 5000000')
            restart = sample('restart-after-wall')
            checks['R works after game over'] = restart['waiting'] == 1 and restart['head'] == 1 and not restart['game_over']
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
    report = dict(passed=all(checks.values()), checks=checks,
                  observations=observations, emulator=str(n.EMULATOR),
                  saved_bytes=len(image), saved_sha256=hashlib.sha256(image).hexdigest(),
                  capture_directory=str(n.RUN.relative_to(ROOT)))
    (n.RUN/'snake-interactive-report.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
