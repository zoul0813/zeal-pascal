# Lighthouse

A four-room text adventure in a classic 80x40 dashboard: find supplies and light
a stormbound lighthouse. The room, inventory, cellar door and lamp status remain
visible while you type commands. Gray scenery and bright headings/items provide
emphasis using the port's recovered TP3 terminal routines.

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
After the beam grows from the lantern, press Enter to leave. The ending is: `The beam sweeps the water. A ship turns toward harbour.`

Commands: N, S, U, D, LOOK, INV, TAKE KEY, TAKE MATCHES, TAKE OIL,
TAKE WICK, OPEN, LIGHT, ? and Q. Commands ignore case; Enter repeats LOOK.

`demo.in` contains exact input lines, including blank lines where needed.
It is a human replay transcript and an automated-test fixture; no shell input
redirection support is assumed. End of input (Ctrl-Z) exits cleanly. The final screen waits for Enter during
interactive play; EOF at that prompt leaves after the short animation and hold.
`quit.in` checks long input, help, case-insensitive commands, LOOK and Q;
`eof.in` leaves mid-game using Ctrl-Z after its two lines; `win-eof.in`
uses Ctrl-Z at the final winning prompt.

## Pascal features on show

- Enumerated rooms and items make the world model readable.
- `belongings = set of item` represents inventory; `take` adds a member.
- `[oil,wick,matches] <= bag` expresses the complete winning requirement.
- Procedures and Case statements separate navigation from the puzzle.
- Typed constant record/room arrays and With keep the fixed layout in data.
- CrtInit/CrtExit save and restore terminal cursor, scroll and rendition state.
- ClrScr, GotoXY and ClrEol maintain the dashboard without scrolling transcripts.
- LowVideo and HighVideo distinguish scenery, headings and collected items.
- Delay paces the winning beam, followed by a one-second hold.

## Files and native behavior

The folder contains `lighthouse.pas`, standalone `lighthouse.bin`, this README, and the four `.in` replay fixtures.
No data files are written. Inventory and progress last for one run.

These are native Zeal programs in the port's current Turbo Pascal 3 dialect.
The dashboard targets the native Zeal 80-column, 40-row text console. LowVideo
is light gray on black; HighVideo and NormVideo are both white on black, as in
the recovered TP3 behavior. These routines do not select arbitrary colors.
Output uses the physical Con device so cursor positioning and text share a
screen even if Output is redirected. Redirecting Output captures only the
one-line epilogue, not a playable transcript. Input remains line-based: press
Enter after each command; arrow-key menus are not part of this example.

The frame avoids the bottom row. Up to 127 physical input characters can wrap
inside the reserved command area; ReadLn keeps the first 40 in `command`, and
the next redraw repairs the frame. On Q, Ctrl-Z or a completed game, the screen is cleared and terminal
state restored before the epilogue. Forced aborts are outside this cleanup path. CrtExit restores state, not old screen pixels.
Physical-hardware flicker and wall-clock timing have not been measured.
No CP/M BDOS calls are used; historical binary or complete language compatibility
is not implied.
