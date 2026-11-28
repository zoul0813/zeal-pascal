---
title: A Map in Tiles
kicker: '08 / OPTIONAL GRAPHICS'
description: 'Draw a small map using colored tiles on the Zeal Video Board.'
duration: '20–30 minutes'
toc: true
weight: 80
---
Text tells you where the beacon is. A map shows you. This side trip draws the roof on the Zeal Video Board in three colors. Dark blue is the night, teal is the roof, and an amber square marks the beacon.

**Before you start.** You need `for` loops and arrays, as in [Minutes and Seconds]({{< relref "minutes-and-seconds" >}}) and [The Running Total]({{< relref "running-total" >}}). This lesson compiles an executable, which the end of The Running Total walks through. The [workbench reference]({{< relref "workbench-reference" >}}#compile-an-executable) has the short version.

**Workbench preview.** The ZVB compile for this example is still being verified.

{{< workbench >}}

## Draw the map

1. Open Pascal as described in the [workbench reference]({{< relref "workbench-reference" >}}#start-and-save).
2. Press **W**, enter `H:/book/graphics/MAPVIEW.PAS`, and press **Enter**.
3. Press **A (Save As)**, enter an unused name such as `H:/work/graphics/mapview.pas`, and press **Enter**.
4. Press **S**. Select Bin-file mode with **O**, **C**, **Q**, then press **C** to compile. Wait for the compiler’s next prompt before pressing other keys.
5. Press **Q** to leave Pascal. At the shell, enter `H:/work/graphics/mapview.bin`.
6. Look at the map, then press any key. The text screen returns with `Map closed.`

The program borrows a helper, `H:/book/graphics/VIDEO.INC`, while it compiles. Your copy names that path, so keep the helper where it is. To change the map, enter `zpascal`, press **Y**, and reopen your copy with **W**.

{{< source "graphics/MAPVIEW.PAS" >}}

## Build a row

In this mode the screen is a grid of tiles, twenty across and fifteen down. A tile number picks a pattern, and each solid tile takes its color from the palette. Tile 0 is dark blue, tile 1 is teal, and tile 2 is amber.

`Row` is an array holding one strip of twenty tile numbers. For each of the fifteen rows, `DrawMap` fills `Row` with zeros and sends it to the foreground layer, which clears it. Then it fills in the roof, adds two dark squares and the beacon at column 10, and sends the strip to the background layer. Changing one number in `Row` changes one square on screen without drawing a single pixel yourself.

## Leave the screen as you found it

Before it changes anything, the program uses the helper to save the current video settings. On a normal exit it restores them and redraws a clean text screen. If you interrupt the program before that cleanup and cannot see the prompt, reload the browser page and choose **Open Zeal** to boot back into text mode. Your source is safe if you saved it with **S** before compiling.

## Try it

After each change, press **S**, then **C**, then **Q**, and run the executable again.

1. **Move the beacon.** In the line `if Y = 7 then Row[10] := 2`, change the row or the column.
2. **Reshape the roof.** Change the loop `for X := 3 to 16` to make the roof narrower or wider.
3. **Add a stairwell.** Just after the line that places the beacon, add `if (Y >= 12) and (Y <= 13) then Row[10] := 1;` for a teal strip leading off the roof.

**A good place to pause.** Your map is saved under `H:/work/graphics/`.

## What comes next

[A Tune for the Beacon]({{< relref "a-beacon-tune" >}}) uses the same video board for sound. [Catch the Beacon]({{< relref "catch-the-beacon" >}}) puts something on screen that moves.
