---
title: A Tune for the Beacon
kicker: '09 / OPTIONAL SOUND'
description: 'Play four short notes, then leave the sound hardware quiet.'
duration: '15–25 minutes'
toc: true
weight: 90
---
A beacon has a light. Yours can have a call sign too, four rising notes short enough to learn before they finish. This side trip plays them on the Zeal Video Board’s sound voices and then leaves the machine quiet.

**Before you start.** You need arrays and `for` loops, as in [Minutes and Seconds]({{< relref "minutes-and-seconds" >}}) and [The Running Total]({{< relref "running-total" >}}). No other lesson’s files are needed.

**Workbench preview.** The ZVB compile for this example is still being verified.

{{< workbench >}}

## Play the call sign

1. Open Pascal as described in the [workbench reference]({{< relref "workbench-reference" >}}#start-and-save).
2. Press **W**, enter `H:/book/sound/TONES.PAS`, and press **Enter**.
3. Press **A (Save As)**, enter an unused name such as `H:/work/sound/tones.pas`, and press **Enter**.
4. Choose **Enable sound** in the workbench. Then click the screen or press **Ctrl+Shift+Space** so Zeal has the keyboard again.
5. Press **S**. Select Bin-file with **O**, **C**, **Q**, then press **R** to compile and run. Wait for the compiler’s next prompt before pressing other keys.
6. Press **p**. The program prints `Note 1` through `Note 4` as it plays each one. Press **p** again to repeat, or **q** to return to Pascal.

With browser audio off, the note numbers still appear, so you can follow along in silence.

{{< source "sound/TONES.PAS" >}}

## One note at a time

`Tune` is an array of four frequencies in hertz. 330, 440, 523, and 660 climb from E up through A and C to the next E.

Each call to `Note` turns one frequency into a divider the sound hardware understands and sets a triangle wave. Releasing the hold starts the note, and setting it again stops it. The first `Delay` is the length of the note, and the second is the gap before the next one.

Before the loop, the program sends one voice to both channels and sets a low master volume. When you press **q**, it holds the voice and turns the master volume down to zero. If you interrupt the program while a note is playing, that cleanup never runs. Reload the browser page and choose **Open Zeal** for a quiet machine. Reloading discards unsaved editor text, so press **S** before each run.

## Try it

1. **Change the tune.** Replace one frequency, or swap two of them, and play again.
2. **Change the rhythm.** Make the second `Delay` much longer for a slow, spaced-out call, or `0` for notes that run together.
3. **Add a fifth note.** Declare `const TuneLength = 5;` before the `type` line. Use `TuneLength` in place of the 4 in the array type and in the `for` loop, then set `Tune[5] := 880`. The constant keeps the array and the loop in step, just as `LastMinute` did in Minutes and Seconds.

If you have finished [Search the Notes]({{< relref "search-the-notes" >}}), here is a larger idea. Read the frequencies from a text file, and the beacon can learn a new tune without a recompile.

**A good place to pause.** Press **S**. Your tune is saved under `H:/work/sound/`.

## What comes next

[Catch the Beacon]({{< relref "catch-the-beacon" >}}) is a small real-time game. A catch sound would suit it.
