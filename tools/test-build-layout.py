#!/usr/bin/env python3
"""Check release archive safety using the existing ZDE CMake."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

if __name__ == '__main__':
    subprocess.run([str(ROOT.parent/'zeal-dev-environment/zde'), '--no-tty', 'exec',
                    'cmake', '-DTEST_ROOT=/src/build', '-P', '/src/tests/test_build_layout.cmake'],
                   cwd=ROOT, check=True, timeout=120)
