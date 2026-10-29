# Lighthouse

A complete four-room text adventure: find supplies and light a stormbound lighthouse.

## Run

Copy this entire folder to a writable native Zeal drive. In ZShell, change into
it (`cd T:/examples/lighthouse` if installed there), then run `./lighthouse.bin`.
The included executable is standalone: it needs neither the IDE nor its ZPL library.

To edit and rebuild, start the installed IDE from this same directory (for example
`T:/bin/zpascal.bin`) and use W to open `lighthouse.pas`. Choose O, M, Q for
Memory output, then R. After the run, Escape returns to the IDE. For a saved
executable choose O, C, Q and C; the compiler writes `lighthouse.COM` and a source
map. That COM is the same native executable format as the supplied `.bin`.
The normal automatic Start and default End settings suffice.

Keep the current directory set to this folder: opening a source file does not
change the current directory. The IDE's L option selects drive roots, so using
it changes where relative data filenames resolve.

## Two-minute demo

Type the lines in `demo.in`. The first LIGHT fails because you are on the shore;
the first D fails because the cellar is locked. Collect the key and matches,
OPEN the cellar, collect oil and wick, return U, S, U and LIGHT the lamp.
The ending is: `The beam sweeps the water. A ship turns toward harbour.`

Commands: N, S, U, D, LOOK, INV, TAKE KEY, TAKE MATCHES, TAKE OIL,
TAKE WICK, OPEN, LIGHT, ? and Q. Commands ignore case; Enter repeats LOOK.

`demo.in` contains exact input lines, including blank lines where needed.
It is a human replay transcript and an automated-test fixture; no shell input
redirection support is assumed. End of input (Ctrl-Z) exits cleanly.

## Pascal features on show

- Enumerated rooms and items make the world model readable.
- `belongings = set of item` represents inventory; `take` adds a member.
- `[oil,wick,matches] <= bag` expresses the complete winning requirement.
- Procedures and Case statements separate navigation from the puzzle.

## Files and native behavior

The folder contains `lighthouse.pas`, standalone `lighthouse.bin`, this README, `demo.in`.
No data files are written. Inventory and progress last for one run.

These are native Zeal programs in the port's current Turbo Pascal 3 dialect.
They use no CP/M BDOS calls or terminal-specific CRT extensions. They demonstrate
application types familiar from the CP/M era; historical binary or complete
language compatibility is not implied.
