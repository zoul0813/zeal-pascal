# Life

Conway's Life on a scrolling, toroidal 20 by 10 world.

## Run

Copy this entire folder to a writable native Zeal drive. In ZShell, change into
it (`cd T:/examples/life` if installed there), then run `./life.bin`.
The included executable is standalone: it needs neither the IDE nor its ZPL library.

To edit and rebuild, start the installed IDE from this same directory (for example
`T:/bin/zpascal.bin`) and use W to open `life.pas`. Choose O, M, Q for
Memory output, then R. After the run, Escape returns to the IDE. For a saved
executable choose O, C, Q and C; the compiler writes `life.COM` and a source
map. That COM is the same native executable format as the supplied `.bin`.
The normal automatic Start and default End settings suffice.

Keep the current directory set to this folder: opening a source file does not
change the current directory. The IDE's L option selects drive roots, so using
it changes where relative data filenames resolve.

## Two-minute demo

The program starts with a glider. Type B, Enter, Enter, Q (`demo.in`).
The blinker has population 3 and returns to its original horizontal shape
after two steps. Try G followed by 4: the glider shifts diagonally while
retaining population 5.

Commands: Enter steps once; 1..100 advances that many generations and prints
only the final grid; G resets a glider, B resets a blinker, ? helps, Q quits.
The generation counter stops before 30000; G or B resets it.

`demo.in` contains exact input lines, including blank lines where needed.
It is a human replay transcript and an automated-test fixture; no shell input
redirection support is assumed. End of input (Ctrl-Z) exits cleanly.

## Pascal features on show

- Two-dimensional Boolean arrays describe the cells.
- `evolve` computes into a second grid, so updates are simultaneous.
- Whole-array assignment publishes the next generation.
- Nested loops count the eight neighbors, wrapping at every edge.

## Files and native behavior

The folder contains `life.pas`, standalone `life.bin`, this README, `demo.in`.
No files are read or written. Edges wrap in both dimensions. The display uses
only plain text; each grid scrolls instead of relying on CRT screen controls.
Patterns are deterministic. This is the classic cellular automaton, not a model
of physical biology.

These are native Zeal programs in the port's current Turbo Pascal 3 dialect.
They use no CP/M BDOS calls or terminal-specific CRT extensions. They demonstrate
application types familiar from the CP/M era; historical binary or complete
language compatibility is not implied.
