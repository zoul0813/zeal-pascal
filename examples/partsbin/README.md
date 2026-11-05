# Parts Bin

A small persistent stock database using a typed file of records and in-place updates.

## Run

Copy this entire folder to a writable native Zeal drive. In ZShell, change into
it (`cd T:/examples/partsbin` if installed there), then run `./partsbin.bin`.
The included executable is standalone: it needs neither the IDE nor its ZPL library.

To edit and rebuild, start the installed IDE from this same directory (for example
`T:/bin/zpascal.bin`) and use W to open `partsbin.pas`. Choose O, M, Q for
Memory output, then R. After the run, Escape returns to the IDE. For a saved
executable choose O, C, Q and C; the compiler writes `partsbin.COM` and a source
map. That COM is the same native executable format as the supplied `.bin`.
The normal automatic Start and default End settings suffice.

Keep the current directory set to this folder: opening a source file does not
change the current directory. The IDE's L option selects drive roots, so using
it changes where relative data filenames resolve.

## Two-minute demo

On a fresh directory, type `demo.in`: answer Y to create `parts.dat`, then A
adds 74HC00 / Quad NAND gate with quantity zero. U, 74HC00, 2 receives two parts.
L lists them. U, 74HC00, 10 receives ten more; U, 74HC00, -20 is refused.
L shows quantity 12. Quit, run the binary again, and type `reopen.in`: L still
shows 12.

Commands: A add a code/name at quantity zero, L list, U update by a signed
quantity (positive receives, negative issues), Q quit. ? or Enter repeats help.
Quantities are 0..30000, with at most 100 records. Codes ignore case and truncate
to six characters; names truncate to 20. A duplicate code is refused.

`demo.in` contains exact input lines, including blank lines where needed.
It is a human replay transcript and an automated-test fixture; no shell input
redirection support is assumed. End of input (Ctrl-Z) exits cleanly.

## Pascal features on show

- `file of part` stores short strings and Integers together.
- `locate` scans records; `fetch` and `put` centralize Seek/Read/Write/Flush.
- An update reads one record, changes its quantity and writes to the same slot.
- `$I-` / IOResult handle missing files and report other disk errors.
- String input plus Val rejects malformed numbers before changing stock.

## Files and native behavior

The folder contains `partsbin.pas`, standalone `partsbin.bin`, this README, `demo.in` and `reopen.in`.
Creates and updates `parts.dat` in the current directory. Creation requires Y;
only a missing-file error offers creation. An existing unreadable file is never
rewritten. The supplied `demo.in` is for an empty directory; `reopen.in` is for the
second run. To repeat from scratch, back up or remove only the generated
`parts.dat` yourself.

Use a writable TF or HostFS directory with free space. Updates are flushed but
are not transactional or power-loss safe. This is an educational database;
keep a backup of data you care about. The program explicitly closes the file
on normal quit or Ctrl-Z at any prompt to finalize its typed record count. Its typed-file header and record layout
belong to this native port; no CP/M file interchange is claimed.

These are native Zeal programs in the port's current Turbo Pascal 3 dialect.
They use no CP/M BDOS calls or terminal-specific CRT extensions. They demonstrate
application types familiar from the CP/M era; historical binary or complete
language compatibility is not implied.
