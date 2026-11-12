# ZealCalc

A VisiCalc-inspired spreadsheet written entirely in Zeal Pascal. One saved
program uses demand-loaded MMU code overlays for storage, formulas, viewport
access, painting and keyboard input. The UI uses CRT keyboard input, checked `ZOS` file access and the ZVB
80x40 text ports. Screen setup, restoration and line clearing live in UI overlays. It is a new example, not a VisiCalc port or file-format emulator.

Copy this entire folder to Zeal. Change into it, then run:

```text
./zealcalc.bin
```

Keep **zealcalc.bin and its generated ZPxxxx directory together**. The exact
companion filenames and hashes are recorded in `binary.json`. Run from that
directory: `OvrDrive(0)` finds companions on the current drive and directory.
There are no worker executables to install. Linked Units are not needed at run
time. Use native 80x40 text mode. Quit with `/Q`, then Y.

The default sheet is `sheet.zvc`; pass another path for another sheet:
`./zealcalc.bin budget.zvc`. Missing sheets are created automatically; existing
files are validated without truncation. The `.zvc` format is unchanged from
the previous three-program example.

## Working with cells

The monochrome status/prompt/entry lines, inverse borders, lettered columns and
slash commands recall VisiCalc. Labels align left and values right. Seven
columns and thirty rows are visible; addresses span A1..Z254.

- Arrows move the selection and scroll at the viewport edge; TAB moves right.
- Type to replace a cell. ENTER or `/E` edits its existing source.
- ENTER commits; arrows or TAB commit and move. ESC cancels. Backspace/Delete
  removes the last character. The entry editor appends at the end.
- `>` goes to an address (`/G` is a convenience alias). `/B` blanks a cell.
- `/?` shows a short hint. The limit and formula reference is below.

Edits save immediately. There is no separate Save command or unsaved sheet state.
Enter labels directly, or begin with an apostrophe to force a label (`'2026`).
Enter decimal numbers directly. Begin formulas with `=`:

```text
A1: Product     B1: Units     C1: Price     D1: Total
A2: Paper      B2: 12        C2: 3.50      D2: =B2*C2
A3: Pencils    B3: 8         C3: 1.25      D3: =B3*C3
A4: Total                                 D4: =@SUM(D2:D3)
```

D2 displays 42.00, D3 10.00 and D4 52.00. Change B2 to 20: D2 becomes 70.00
and D4 becomes 80.00. Quit and reopen the same file to retain the sheet.

Formulas support case-insensitive references, decimal literals, parentheses,
unary signs, `+ - * /` with ordinary precedence, and `@SUM(A1:B10)` with an
inclusive rectangular range. Blank cells and labels contribute zero. Off-screen
references are fetched from disk. Calculations retain Pascal Real precision;
the display rounds to two decimal places. A numeric result too wide for the
nine-character field shows `>>>>>>>>>`, never a truncated number. Long labels
are clipped; ENTER opens the full source in the entry line.

Errors propagate through references: `#SYNTAX`, `#REF`, `#DIV/0`, `#RANGE`,
`#CYCLE`, `#DEPTH`, `#LIMIT`. Invalid formulas are saved so they can be corrected.
Magnitudes must not exceed 1,000,000,000; nonzero divisors must be at least
0.000000001 in magnitude. Each SUM may visit at most 256 cells. Dependencies
have eight frames, and parser recursion has sixteen levels, counting operator,
parenthesis and unary-sign calls across dependencies. Each displayed result may
visit at most 2048 cells, including repeats; exceeding that budget yields
`#LIMIT`. These bounds constrain both stack use and evaluation work.

## Bounded memory and disk storage

There is no file-backed `mmap` in the current Pascal/Zeal OS interface. ZOS MAP
maps physical hardware/RAM pages, not disk files; it provides no demand paging.
ZealCalc uses an explicit bounded working set instead:

| Structure | Size | Location |
| --- | ---: | --- |
| 254 row-head record indices | 508 bytes | Shared resident data |
| Eight formula frame coordinates/positions/lengths | 32 bytes | Shared resident data |
| Shared disk record | 70 bytes | Shared resident data |
| Shared header/cell/path staging | 128 bytes | Shared resident data |
| 210 displayed cell strings | 2100 bytes | Owned RAM data bank |
| Six status/prompt/footer strings | 480 bytes | Owned RAM data bank |
| Eight saved formula sources | 512 bytes | Owned RAM data bank |

These are the principal structures, not the entire resident image. The last
three share one 16 KiB MMU RAM page and use 3092 bytes. Copies briefly map that
page into window0 with interrupts disabled, then restore both the mapping and
the caller's interrupt state. No Pascal code or syscall runs while it is mapped.
The page is released on normal exit; Zeal OS also reclaims process-owned pages
when the program terminates.

Code members each fit in 16 KiB. First calls load the demanded member from its
companion file; warm calls reuse RAM. The overlay manager owns its own bank.
The compiler keeps private-call prologues and returns inside each member;
CPU frames remain in shared memory. Resident wrappers handle cross-bank calls. See [overlay support](../../docs/overlays.md)
for placement, cache recycling and error behavior. `binary.json` records the
verified resident code/data size, checked CPU stack reserve and member sizes.
The header reserve describes the largest compiler-checked call. During calculation,
`RecurPtr := StackPtr - 3200` reserves CPU space for nested evaluation plus checked
file I/O; recursive expression workspaces grow below that boundary.

No working structure grows with populated cells or edits. The UI caches only
screen strings, not the whole sheet. It reads sources from the disk viewport
as needed. After an edit, storage and calculation overlays rebuild that
viewport; the UI compares display strings and redraws only changed cells.
Moving within the viewport redraws the old/new selection without recalculating.
Headers and the grid redraw at startup or on scrolling. Line updates retain
an unchanged prefix and replace the changed tail. Row40 remains untouched by
painting.

Ordinary overlay calls pass shared state directly; there is no Execute/Chain
handoff or request file. Before evaluation, Pascal's `RecurPtr` reserves 2496
bytes for guarded CPU calls; recursive expression workspaces use the separately
checked recursion area.

The Pascal typed `.zvc` file has a four-byte count/component-size header, one
70-byte signature record and 70-byte edit records. Each
changed entry stores row, column, previous record in that row's history, source
string and additive checksum. Blank is a stored empty string, so an older value
cannot reappear after reopening. Unchanged entries use no edit
capacity. The storage overlay streams and validates the file to reconstruct row heads;
it does not load the cells. Lookups walk that row's history, newest first.
Values are recomputed, so edits
cannot leave stale dependency totals. Long histories can make refresh slower.

A file permits **4096 changed entries**, including replacements and Blank, and
occupies at most **286,794 bytes**. Cells hold at most 63 source characters. A full
file remains viewable. There is no undo, compaction, import/export, row/column
insertion, copy, relative-reference rewriting or VisiCalc file compatibility in
this example. Use another file for a new sheet; back up sheets with normal file
copying tools.

Each accepted edit is written completely, closed and reopened before being
reported saved. This is not a power-loss transaction guarantee; there is no sync
API here. An incomplete committed record, invalid address/chain, nonprintable source or
checksum failure stops opening the sheet without discarding bytes. The typed
file header determines the committed record count; extra uncommitted bytes are
not sheet entries. The additive checksum
detects simple corruption, not deliberate tampering. A storage/calculation/close failure may
occur after an edit has reached disk; restart and inspect rather than replay it
blindly. Keep backups, free disk space and a single writer per sheet.

## Scratch file and overlay lifecycle

`zc.view` is the private, bounded 7x30 viewport in the current directory. Startup
refuses an existing file with that name. Normal exit removes only the viewport
reserved by this session. After a forced interruption, inspect and remove a
stale `zc.view` before restarting; keep the `.zvc` sheet. `zc.req` is no longer
created or used. Keep a single writer per sheet and application directory;
there is no filesystem locking or exclusive-create API in this interface.

Storage validates coordinates, source lengths/characters, record checksums and
history chains before editing. Calculation marks its viewport complete only
after writing all cells. A failure stops the program instead of retrying an
edit that might already have committed. Error6 means a checksum failure;
31–37 identify checked I/O, request or resource failures. Overlay loading errors
use the runtime's fatal F0 behavior.

## Building and verification

In the native IDE, compile **zealcalc.pas** using **O → C → Q**, then **C**.
Keep `include` beside it, with matching CRT.ZPU and ZOS.ZPU available to the
compiler. Keep the generated COM and its `ZPxxxx` directory together; the
shipped COM is named `zealcalc.bin`. See the [Unit guide](../../docs/units.md).
Compile to a saved COM; direct Memory compilation does not support overlays.

From the repository root, using the installed Zeal development environment:

```sh
zde --no-tty cmake --target zpascal_bin -j8
python3 tools/author-crt.py
python3 tests/acceptance/test-zealcalc.py --package
```

In a worktree outside the usual Zeal layout, set `ZP_ZEAL_ROOT` to the existing
`/Users/higginsd/Projects/zeal` directory for the Python commands. No additional
emulator or target toolchain installation is needed.

Acceptance compiles the real program and includes on the unmodified emulator.
A test-only backend harness calls the same storage/calculation overlays for
formula and corruption fixtures. Keyboard tests exercise entry, cancel,
navigation, Blank, reopening and edit limits with the single application COM
and its companions. Read-only native output traces check incremental repainting;
screen captures use the actual ZVB character/attribute planes, font and palette.
`--package` refreshes the executable, companions, captures and provenance only
after all checks pass. This is emulator acceptance; physical-board qualification
and historical VisiCalc equivalence are separate.

## Emulator screenshots

Captured from the actual ZVB character/attribute planes, font and palette:

![Invoice with the total selected](screenshots/sheet.png)

![Z254 referencing the off-screen total](screenshots/scrolled.png)
