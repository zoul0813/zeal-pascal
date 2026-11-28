---
title: A Panel on the Screen
kicker: '07 / OPTIONAL SCREEN AND INPUT'
description: 'Build a small status panel that reacts to a key without waiting for Enter.'
duration: '15–25 minutes'
toc: true
weight: 70
---
The calculator, the adventures, and the search tool all wait for a whole line and the **Enter** key. A control panel should not. Press a key and the light moves. This side trip builds a small beacon status board that answers single keys and keeps the rest of the screen where it is.

**Before you start.** You need the routines and `repeat` loops from [Minutes and Seconds]({{< relref "minutes-and-seconds" >}}). No other lesson’s files are needed.

{{< workbench >}}

## Light up the panel

1. Open Pascal as described in the [workbench reference]({{< relref "workbench-reference" >}}#start-and-save).
2. Press **W**, enter `H:/book/screen/PANEL.PAS`, and press **Enter**.
3. Press **A (Save As)**, enter an unused name such as `H:/work/screen/panel.pas`, and press **Enter**.
4. Select Bin-file with **O**, **C**, **Q**, then press **R** to compile and run.

A blue screen shows `BEACON STATUS` with two choices. `Workshop` is highlighted in yellow. Press **2** and the highlight moves to `Roof`. Press **1** to move it back. No **Enter** is needed. Press **q** to finish. The program prints `Panel closed.` and returns to Pascal.

{{< source "screen/PANEL.PAS" >}}

## Three jobs, kept apart

The program splits its work three ways, and each part has one job.

- **State.** `Choice` holds the current selection. It is the only thing the panel remembers from one key to the next.
- **Drawing.** `DrawPanel` turns `Choice` into colors at fixed positions. `GotoXY` places the cursor, counting from column 1 and row 1. `Write(Con, ...)` writes to the Zeal console at that spot.
- **Input.** The loop asks `KeyPressed` whether a key is waiting. Only then does it call `ReadKey`, so the program never stops to wait. `Delay(20)` gives the loop a short pause between checks.

A key changes the state, and the drawing follows. The panel is redrawn only when the choice changes.

`CrtInit` saves the console settings and `CrtExit` restores them when you quit normally. The picture you drew stays on screen afterward. Press **O**, then **Q** at the returned prompt to redraw the Pascal menu. If the program stops before `CrtExit` runs, the colors can stay behind. Reload the browser page and choose **Open Zeal** to start clean. Reloading discards unsaved editor text, so press **S** before each run.

## Try it

1. **A new highlight.** Change `Yellow` in both places to another CRT color, such as `LightCyan`.
2. **A third choice.** In `DrawPanel`, copy the two lines for choice 2 and the `Write` that follows. Change the copy to row 9, choice 3, and `'3  Store room'`. In the main loop, add `if Key = '3' then Choice := 3;` and add `(Key = '3')` to the redraw test. Update the help line to say `1, 2 or 3`.

**A good place to pause.** Press **S**. Your panel is saved under `H:/work/screen/`.

## What comes next

[A Map in Tiles]({{< relref "a-map-in-tiles" >}}) trades characters for colored tiles. [Catch the Beacon]({{< relref "catch-the-beacon" >}}) uses this same polling loop to keep a game moving while it listens for keys.
