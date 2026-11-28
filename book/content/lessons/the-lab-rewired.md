---
title: The Lab, Rewired
kicker: '04 / ADVENTURE'
duration: '30–45 minutes'
description: 'Move the lab map into data, give the player an inventory, and repair the roof beacon.'
toc: true
weight: 40
---
The amber light from the Silent Lab is still on. The beacon on the roof is a different matter. It is dark, the roof hatch is locked, and the spare fuse is somewhere downstairs. This adventure has five rooms, two objects, and one light to get working.

The first lab spelled out its moves in `if` statements. That works for three rooms and gets tiresome at thirty. Here the map is a table of data, and one short procedure handles every move.

**Before you start.** This lesson builds on the command loop from [The Silent Lab]({{< relref "the-silent-lab" >}}) and the routines from [Minutes and Seconds]({{< relref "minutes-and-seconds" >}}). It indexes arrays in a new way, so the history array in [The Running Total]({{< relref "running-total" >}}) is a useful warm-up. You do not need your own adventure. `REWIRE.PAS` is complete and reads no files from the first lab.

{{< workbench >}}

## Restore the beacon

1. Open Pascal as described in the [workbench reference]({{< relref "workbench-reference" >}}#start-and-save).
2. Press **W**, enter `H:/book/rewired/REWIRE.PAS`, and press **Enter**.
3. Press **A (Save As)**, enter an unused name such as `H:/work/rewired/REWIRE.PAS`, and press **Enter**.
4. Select Com-file with **O**, **C**, **Q**, then press **R** to compile and run.

Play it before you read it. Commands are lowercase, as in the first lab. If you want a route, here is one.

| Enter | Look for |
| --- | --- |
| `n`, `e`, `n` | Reach the workshop. The roof hatch refuses you. |
| `w`, `take key`, `e` | Pick up the key in the terminal room and return. |
| `s`, `take fuse`, `n` | Collect the fuse in the store room and return. |
| `n`, `repair` | Climb to the roof and light the beacon. |
| `q` | Ends a run early, before the beacon lights. |

On a second run, try `take fuse` at the entrance and `repair` in the workshop. Both are refused. The inventory starts empty every run.

{{< source "rewired/REWIRE.PAS" >}}

## Read a map entry

`Room`, `Direction`, and `Item` are enumerations. Each one names a small group of related values, and Pascal can use those names as array indexes. `Map[Workshop]` is a record holding the workshop’s title and its exits. `Map[Workshop].Doors[South]` is the room you reach by going south from there.

The program never prints a `Room` value itself. Each room carries a `Title` string for the player to read.

`BuildMap` starts by putting `NoRoom` behind every door. Each assignment after that opens one door in one direction, so a two-way passage takes two lines. `Move` looks up the destination, applies the one special rule for the roof hatch, and changes `Here` only when the move is allowed.

`Look` prints the title, then adds a line for rooms with something to notice. The entrance has nothing extra, so it shows its title alone.

## Keep track of the objects

`Holding` is a set of `Item` values. An empty set is written `[]`. `Key in Holding` asks whether the key is in the set, and `Holding + [Key]` adds it. The `repair` branch reads almost like the rule it enforces. The player must be on the roof and holding the fuse.

## Try it

1. **A shortcut.** Add an east door at the entrance that leads to the store room, and a west door back. Both lines go in `BuildMap`. Test both directions, then finish the game through the new route. The movement code does not change.
2. **A new room.** Add `Cellar` to the `Room` list between `Store` and `Roof`. That position keeps it inside the `Entry to Roof` loop that closes every door. Give it a title, connect it east of the store room, and walk there and back.
3. **A harder one.** The workshop describes its exits in a sentence written by hand. Write a procedure that prints the open exits from the map instead, with a `for` loop over `Direction`. You will need a way to turn each direction into a word.

**A good place to pause.** Press **S**. Your adventure is saved under `H:/work/rewired/`.

## What comes next

The lab now runs on data. [Search the Notes]({{< relref "search-the-notes" >}}) leaves the map behind and reads a file one character at a time. [Keep a Collection]({{< relref "keep-a-collection" >}}) brings records back and stores them on disk.
