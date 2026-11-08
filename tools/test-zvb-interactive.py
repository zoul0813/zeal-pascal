#!/usr/bin/env python3
"""Run the consolidated saved-Snake control, pixel and C-parity acceptance."""
from pathlib import Path
import runpy

if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).with_name('test-snake-parity.py')), run_name='__main__')
