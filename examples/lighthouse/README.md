# Lighthouse

A four-room text adventure in a classic 80x40 dashboard: find supplies and light
a stormbound lighthouse. The room, inventory, cellar door and lamp status remain
visible while you type commands. CP437 clouds, a striped tower, rocks, waves and a distant ship use per-cell
foreground/background colors. The frame and scene are drawn once; later turns
update only changed room, inventory, door, lamp and feedback fields.

## Run

Copy this entire folder to a writable native Zeal drive. In ZShell, change into
it (`cd T:/examples/lighthouse` if installed there), then run `./lighthouse.bin`.
The included executable is standalone: it needs neither the IDE nor its ZPL library.

To edit and rebuild, start the installed IDE from this same directory (for example
`T:/bin/zpascal.bin`) and use W to open `lighthouse.pas`. For a saved
executable choose O, C, Q and C; the compiler writes `lighthouse.COM`. That COM is the same native executable format as the supplied `.bin`.
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
- TextColor and TextBackground give the sidebar, text and CP437 art distinct pens.
- A displayed-state snapshot avoids repainting unchanged fields.
- A command table maps complete names and aliases to a Case dispatch.
- Delay paces the lantern glow, spreading beam and ship reveal, then holds briefly.

## Files and native behavior

`lighthouse.pas` is the program entry point. It keeps initialization, command
dispatch and normal exit handling together, and includes these source files:

- `include/data.pas`: types, room descriptions, artwork tables and shared state.
- `include/screen.pas`: console helpers, initial layout and cached field updates.
- `include/game.pas`: room observations and inventory changes.
- `include/beam.pas`: the lantern, beam and ship animation.

These are textual `{$I filename}` includes, not separately compiled programs or
units. Open and compile `lighthouse.pas`; the compiler reads the includes relative
to that source file. Keep the `include` subdirectory when copying or editing the
sources. The standalone `lighthouse.bin` needs none of these source files to run.
The package manifest records each included file's hash as well as the main source
and executable hashes.

The folder also contains this README, a native screen capture, and the four `.in`
replay fixtures.
No data files are written. Inventory and progress last for one run.

These are native Zeal programs in the port's current Turbo Pascal 3 dialect.
The dashboard targets the native Zeal 80-column, 40-row text console. Run it
with normal console input/output in ZShell; redirected output is not a usable
full-screen display. Colors use the port's TP-style names and native
TextColor/TextBackground extensions. CP437 bytes are written as `#nnn` literals,
so the source remains ASCII and needs no Unicode-aware editor or font loader.

Static borders, labels, sidebar backgrounds and the harbour picture are painted
once. A successful TAKE changes only its inventory row and feedback (plus lamp
readiness when all supplies are present). Moving updates four room fields.
Only rows 32--35 are cleared between commands to repair wrapped input. Winning
changes the lantern, beam and ship, then reveals SHINING and the closing text.

TP3 `{$B-}` preserves real line terminators. ReadLn collects a full 127-character
physical line and a trailing Char; CR distinguishes Enter from physical Ctrl-Z.
Only the first 40 command characters are used. `{$C-}` disables teletype output
flow control; Q and Ctrl-Z are the explicit normal exits. Bounds checks stay on.

On Q, Ctrl-Z or a completed game the screen is cleared, saved terminal state is
restored, and a one-line epilogue is printed. CrtExit restores state, not old
screen pixels. Forced aborts are outside this cleanup path. Physical-hardware
flicker and wall-clock timing have not been measured.
No CP/M BDOS calls are used; historical binary or complete language compatibility
is not implied.
