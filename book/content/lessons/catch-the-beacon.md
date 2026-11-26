---
title: Catch the Beacon
kicker: '10 / OPTIONAL REAL-TIME GAME'
description: 'Move a basket in real time and catch a string of falling beacons.'
duration: '20–30 minutes'
toc: true
weight: 100
---
Someone on the roof is lowering beacon bulbs without looking. You have a basket. The calculator and the adventures wait for you to type. This game keeps moving while you decide what to do.

**Before you start.** This game uses the key polling, `GotoXY`, and console cleanup from [A Panel on the Screen]({{< relref "on-the-screen" >}}). Read that side trip first if `KeyPressed` is new to you. No other lesson’s files are needed.

{{< workbench >}}

## Play a round

1. Open Pascal as described in the [workbench reference]({{< relref "workbench-reference" >}}#start-and-save).
2. Press **W**, enter `H:/book/game/CATCH.PAS`, and press **Enter**.
3. Press **A (Save As)**, enter an unused name such as `H:/work/game/CATCH.PAS`, and press **Enter**.
4. Select Com-file with **O**, **C**, **Q**, then press **R** to compile and run.

The basket is the `=` near the bottom and the falling bulb is a `*`. Press **a** twice straight away to get under the first one. After that, **a** moves left and **d** moves right. Each catch raises the score and sends the next bulb two columns to the right, so keep tapping **d**. Catch all nine to win.

Let one fall past you to see the result screen. Press **r** for a fresh round. Press **q** during play or at the result screen to quit. The game prints `Game closed.` and returns to Pascal.

{{< source "game/CATCH.PAS" >}}

## Give each frame a job

Each trip around the loop is one frame, and every frame does the same jobs in the same order.

1. Draw the bulb, wait a moment, and erase it.
2. If a key is waiting, read it and move the basket.
3. If the bulb is on the row just above the basket, compare their columns. Otherwise move it down a row.

A catch adds to the score and starts a new drop at the top. A miss sets `Playing` to False, and the next frame shows the result screen instead.

A round owns its basket position, its drop, and its score. `StartRound` sets all of them, so restarting after a miss is exactly the same operation as starting for the first time. Nothing from the last round leaks into the next one.

`Delay(130)` asks for a pause of about 130 milliseconds. The emulator’s speed depends on the computer running it, so the game can feel faster or slower in another browser, and timing on a physical Zeal may differ again.

## Try it

1. **Slow it down.** Raise the `Delay` value and play again.
2. **Show the score from the start.** The score appears only after the first catch. Copy the two lines that print it into `StartRound`, after `Score := 0`.
3. **Change the drops.** `DropCol := DropCol + 2` decides where the next bulb falls. Try `+ 1` for an easier round. A bigger step soon sends bulbs past column 40, where the basket cannot follow, so change `RightEdge` or the win score to keep the game fair.

**A good place to pause.** Press **S**. Your game is saved under `H:/work/game/`.

## What comes next

You have now met every side trip. A catch sound from [A Tune for the Beacon]({{< relref "a-beacon-tune" >}}) or a tiled backdrop from [A Map in Tiles]({{< relref "a-map-in-tiles" >}}) would make good additions. The [workbench reference]({{< relref "workbench-reference" >}}#pattern-lookup) points back to each technique when you need it.
