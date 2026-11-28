---
title: Keep a Collection
kicker: '06 / TYPED FILES'
duration: '35–55 minutes'
description: 'Keep a small catalog whose records survive the end of a run.'
toc: true
weight: 60
---
The lab’s parts shelf is a mess of bulbs, keys, and unlabelled fuses. A list on screen helps until the program ends. This catalog keeps its list in a file, so it is still there tomorrow. Each item gets a number you can use to change or remove it later.

**Before you start.** The catalog stores records like the ones in [The Lab, Rewired]({{< relref "the-lab-rewired" >}}). It checks file operations the way [Search the Notes]({{< relref "search-the-notes" >}}) does. You can start here without either program, but skim their explanations if records or `IOResult` are new.

{{< workbench >}}

## Make a catalog

1. Open Pascal as described in the [workbench reference]({{< relref "workbench-reference" >}}#start-and-save).
2. Press **W**, enter `H:/book/catalog/CATALOG.PAS`, and press **Enter**.
3. Press **A (Save As)**, enter `H:/work/catalog/catalog.pas`, and press **Enter**. This also gives the data file its folder. Choose a fresh name if that file already exists.
4. Select Bin-file with **O**, **C**, **Q**, then press **R** to compile and run.
5. On a fresh workspace the catalog asks whether to create `ITEMS.DAT`. Enter `y` to create it. Any other answer ends the program without creating anything.

Whatever you named your source, the data always lives at `H:/work/catalog/ITEMS.DAT`. If you have done this lesson before, the catalog opens your earlier items instead of asking.

Work down this table. Each line is one command followed by the answers it asks for.

| Enter | What you should see |
| --- | --- |
| `a`, then `Spare bulb` | `Added as item 1` |
| `a`, then `Brass key` | `Added as item 2` |
| `l` | Both items with their numbers |
| `f`, then `key` | `2 Brass key`. Searches compare case, so `Key` finds nothing. |
| `u`, `1`, then `Beacon bulb` | `Updated.` List again to see the new title. |
| `d`, `2`, then `y` | `Removed.` Item 2 disappears from the list. |
| `e` | `Wrote LIST.TXT.` |
| `q` | The catalog closes its files and ends. |

Now run the program again. It does not ask to create anything this time. Enter `l` and only `1 Beacon bulb` appears. Add another item and it becomes item 3. Number 2 stays retired.

The export at `H:/work/catalog/LIST.TXT` is a plain text copy. Open it with **W** and **E**, then reopen your source with **W** before running again. `ITEMS.DAT` is the catalog’s binary data, so do not edit it. To keep a copy outside the browser, use **Download backup**, which includes it. [About the workbench]({{< relref "/about" >}}#your-files) explains backups.

{{< source "catalog/CATALOG.PAS" >}}

## Read and write one slot

An `Entry` is a record with a title and an active flag. A `file of Entry` stores many records of exactly that shape, one after another. Item numbers start at one for people. The positions passed to `Seek` start at zero, so the program subtracts one. `Fetch` seeks to one slot and reads it. `Put` seeks to that same slot, writes, and flushes. Every other slot keeps its contents.

Removing an item only clears its active flag. The slot stays in the file, so the numbers of the other items never shift. There is room for fifty slots, and removed ones still count. The export includes only active items and refuses to replace an earlier `LIST.TXT`.

## Treat the file layout as part of the program

The program offers to create a catalog only when opening it reports a missing file. Any other error stops the program, so you can inspect what happened. It never recreates a file it could not open.

The data lives at a fixed path, and the record layout is part of its meaning. An existing file may not match the program after you change `Entry`. Even a record of the same size can mean something different. Back up the data before experimenting with the record layout. A write error does not undo bytes already written, so inspect the file before retrying.

**A good place to pause.** Your source is in `H:/work/catalog/` and your items are in `ITEMS.DAT` beside it.

## Try it

1. **Count without writing.** Add a read-only `n` command that reports how many items are active. Put this procedure after `ShowCards`.

```pascal
procedure CountActive;
var Active: Integer;
begin
  Active := 0;
  for Index := 0 to Count - 1 do
  begin
    Fetch;
    if Card.Active then Active := Active + 1
  end;
  WriteLn(Active, ' active items')
end;
```

Then add `else if Command = 'n' then CountActive` to the command chain, just before `else if Command <> 'q'`. Add `n` to the help line and to the list of choices in the last message.

2. **Give a new layout its own file.** Before you change `Entry`, change `DataName` to `'H:/work/catalog/ITEMS2.DAT'`. The new program then starts a new catalog and your first one stays readable by the first program. Now try adding a `Shelf` field and setting it when an item is added.

## What comes next

That completes the four core projects. The side trips leave the prompt behind. [A Panel on the Screen]({{< relref "on-the-screen" >}}) responds to single keys, [A Map in Tiles]({{< relref "a-map-in-tiles" >}}) draws with the video board, and [A Tune for the Beacon]({{< relref "a-beacon-tune" >}}) gives the beacon a voice. [Catch the Beacon]({{< relref "catch-the-beacon" >}}) puts keys and timing together in a game.
