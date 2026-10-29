# Concordance

A text-file utility that counts words and ranks the ten most frequent words.

## Run

Copy this entire folder to a writable native Zeal drive. In ZShell, change into
it (`cd T:/examples/concord` if installed there), then run `./concord.bin`.
The included executable is standalone: it needs neither the IDE nor its ZPL library.

To edit and rebuild, start the installed IDE from this same directory (for example
`T:/bin/zpascal.bin`) and use W to open `concord.pas`. Choose O, M, Q for
Memory output, then R. After the run, Escape returns to the IDE. For a saved
executable choose O, C, Q and C; the compiler writes `concord.COM` and a source
map. That COM is the same native executable format as the supplied `.bin`.
The normal automatic Start and default End settings suffice.

Keep the current directory set to this folder: opening a source file does not
change the current directory. The IDE's L option selects drive roots, so using
it changes where relative data filenames resolve.

## Two-minute demo

Press Enter (`demo.in`) to read `sample.txt`. It has four lines and 34 words.
A appears five times; MACHINE, PROGRAM and SMALL each appear three times.
Frequency ties sort alphabetically. Try another native text filename to analyze
your own writing; failure to open it reports the native I/O status.

`demo.in` contains exact input lines, including blank lines where needed.
It is a human replay transcript and an automated-test fixture; no shell input
redirection support is assumed. End of input (Ctrl-Z) exits cleanly.

## Pascal features on show

- Text file Assign/Reset/Read/Eof/Close stream characters without a line buffer.
- UpCase and short-string operations build case-insensitive word keys.
- An array of records holds the frequency table.
- Real totals avoid the 32767 limit for line/word/character counts.
- A repeated maximum search ranks ten entries without a sorting library.

## Files and native behavior

The folder contains `concord.pas`, standalone `concord.bin`, this README, `demo.in` and `sample.txt`.
Reads `sample.txt` by default and writes no files. Words consist only of ASCII
A..Z; apostrophes, digits and punctuation split words. Characters count bytes
returned by Text input, including line endings (CR/LF counts as two). LF ends a
line, and an unterminated final line counts too. Text input stops at Ctrl-Z.

The first 64 distinct keys are tracked. Counts saturate at 30000 per key;
a warning identifies partial frequency results. Keys use the first 20 letters,
with a warning when longer words share a prefix. Total words still includes
untracked words. Real totals have finite precision on very large files.

These are native Zeal programs in the port's current Turbo Pascal 3 dialect.
They use no CP/M BDOS calls or terminal-specific CRT extensions. They demonstrate
application types familiar from the CP/M era; historical binary or complete
language compatibility is not implied.
