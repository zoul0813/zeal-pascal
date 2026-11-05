# Queens

Place N queens so none attacks another, count every solution and show the first
board. Recursive search and Pascal sets make the constraints visible in the code.

## Run

Copy this entire folder to Zeal. In ZShell use `cd T:/examples/queens` and
`./queens.bin`, or `cd H:/queens` when HostFS points at the examples directory.
No IDE library is needed. To rebuild, open `queens.pas` in the IDE from this
folder and compile in Memory or Com-file mode. The saved `queens.COM` uses the
same native executable format as the supplied `.bin`.

## Try it

Type `demo.in`: boards of size 4, 6, 3 and 8 have 2, 4, 0 and 92 solutions.
The zero-result board is a valid search result. A blank line or Ctrl-Z quits.
`counts.in` covers the remaining sizes and rejects 0, 9 and nonnumeric input.

| Board size | Solutions |
| --- | --- |
| 1 | 1 |
| 2 | 0 |
| 3 | 0 |
| 4 | 2 |
| 5 | 10 |
| 6 | 4 |
| 7 | 40 |
| 8 | 92 |

Rows are printed highest first, with lettered columns. Q marks a queen.
Only the first arrangement is displayed; every arrangement is counted, including
rotations and reflections. The positions-tried counter counts each candidate
column examined at a search node, including immediately rejected candidates.
The 8-by-8 search does more work than the small boards; wait for its summary.

## Pascal features on show

- `{$A-}` recursive backtracking preserves local column/diagonal indices.
- Three sets track occupied columns and both diagonals. Membership rejects a
  conflict before recursion; union/difference undo each trial after returning.
- Whole-array assignment saves the first solution independently of later trials.
- Integer solution counts stay small for N <= 8; a Real counts search attempts.
- String input plus Val validates the board size before indexing any arrays.

The source, standalone binary and two input transcripts are self-contained.
No files, CRT calls or graphics are used; the board scrolls as plain text.
