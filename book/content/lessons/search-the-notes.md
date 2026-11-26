---
title: Search the Notes
kicker: '05 / TEXT FILES'
duration: '30–45 minutes'
description: 'Find matching lines in a file and save a report for later.'
toc: true
weight: 50
---
Somebody kept a maintenance log while the lab was shut. It has four lines today. Once it has four hundred, finding the last mention of the beacon will take longer than fixing the beacon. This tool reads a text file and tells you which lines mention what you are looking for. It can also save that answer as a report.

**Before you start.** You should be able to open, save, and run a file in Pascal, as in [The Silent Lab]({{< relref "the-silent-lab" >}}). The input checks follow the pattern from [Minutes and Seconds]({{< relref "minutes-and-seconds" >}}). You do not need any earlier experience with data files.

{{< workbench >}}

## Search the supplied file

1. Open Pascal as described in the [workbench reference]({{< relref "workbench-reference" >}}#start-and-save).
2. Press **W**, enter `H:/book/search/SEARCH.PAS`, and press **Enter**.
3. Press **A (Save As)**, enter `H:/work/search/SEARCH.PAS`, and press **Enter**. Saving here also gives the report its folder. Choose a fresh name such as `SEARCH2.PAS` if that file already exists.
4. Select Com-file with **O**, **C**, **Q**, then press **R** to compile and run.
5. At the filename question, press **Enter** to read the supplied `H:/book/search/NOTES.TXT`.
6. Enter `beacon` as the search text, then press **Enter** at the report question.

```text
Line 2
Matching lines: 1
```

Run it again with other search text. Each run asks the same three questions.

| Search for | Matching lines |
| --- | --- |
| `beacon` | 2 |
| `Beacon` | 4. Matching checks case. |
| `fuse` | 3 |
| `e` | 1, 2, 3 and 4 |
| `banana` | None. `Matching lines: 0` |

{{< source "search/SEARCH.PAS" >}}

## Save a report

Run once more, search for `beacon`, and answer `y` at the report question. The program writes the same results to `H:/work/search/REPORT.TXT`, after a first line that names the search.

To read the report, press **W**, enter `H:/work/search/REPORT.TXT`, then press **E**. Leave the editor with **Ctrl-K**, release, then **D**. Before you run the search again, press **W** and reopen your `SEARCH.PAS`, because **R** works on whichever file is open.

Now ask for a second report while the first one is still there. The program refuses and stops before it searches. It creates a report only when that path is unused.

**A good place to pause.** Your tool is saved under `H:/work/search/` and the report sits beside it.

## Follow one character through the program

`Assign` connects a Pascal file variable to a path. `Reset` opens the file for reading. `Read` takes one character at a time, and `Eof` tells the loop when the file has run out. A line feed ends a line. A carriage return is skipped, so files with either style of line ending give the same line numbers. A last line without a line feed still counts.

Reading one character at a time means no line is too long to search. There is no line buffer to overflow and lose its tail.

`Window` holds the most recent characters, never more than the search text needs. Before adding a new character, the program drops the oldest one if the window is already full. When the window equals the search text, the line matches. That finds a word inside another word, such as `lamp` inside `lampstand`. Once a line matches, the program reads on to the end of that line and reports it once.

## Protect the files

`{$I-}` tells the program to handle file errors itself. Read `IOResult` immediately after each operation that can fail. The program checks whether the report already exists before calling `Rewrite`, which would otherwise replace its contents.

A report can be partial after an error or after the 30,000-line limit. Inspect it before retrying. The input file is only ever read.

## Try it

1. **Make room for another report.** Change `ReportName` to `'H:/work/search/REPORT2.TXT'`, save, and request a report again.
2. **Search your own notes.** Press **S** to save your source first. Press **W**, enter `H:/work/search/MINE.TXT`, then press **E** and type a few lines. Keep each line shorter than 127 characters, the editor’s limit. Leave the editor and press **S**. Reopen your `SEARCH.PAS` with **W**, run it, and enter the full path of your notes at the filename question. Keep the book’s `NOTES.TXT` as it is.
3. **Find the edges.** Put a matching word twice on one line. Write a long line with the match near its end. Then enter a path that does not exist. Each case tests a different part of the loop.

## What comes next

This tool reads one file and creates another without touching either afterward. [Keep a Collection]({{< relref "keep-a-collection" >}}) goes further. It opens a file of records, changes one, and leaves the rest alone.
