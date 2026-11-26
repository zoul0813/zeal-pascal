---
title: Workbench Reference
kicker: 'REFERENCE'
description: 'Find a key, a file location, or a way back to a working program.'
toc: true
weight: 110
---
## Start and save

Choose **Open Zeal** and wait for `H:/>`. Click the emulator screen before you type. **Ctrl+Shift+Space** switches the keyboard between the emulator and the book. Enter `zpascal`, then press **Y** for explanatory compiler messages.

| To do this | Use these keys |
| --- | --- |
| Choose or open a Work file | **W**, type the path, **Enter** |
| Make a personal copy of supplied source | **A (Save As)**, type an unused `H:/work/` path, **Enter** |
| Edit the Work file | **E** |
| Return to the menu from the editor | **Ctrl-K**, release both keys, then **D** |
| Save your source | **S** |
| Compile and run, the lessons’ usual route | **O**, **C**, **Q**, then **R** |
| Run in Memory, as in the first two lessons | **O**, **M**, **Q**, then **R** |
| Redraw the menu after a program returns | **O**, then **Q** |
| Go to a reported source location | **Escape** at the compiler diagnostic |

Save As replaces an existing filename without asking. Use the book’s paths under `H:/book/` for supplied copies and project folders under `H:/work/` for your changes. Run and Compile do not save your source. [About the workbench]({{< relref "/about" >}}#your-files) explains backups and updates.

## Compile an executable

Save the personal source with **S**, then choose **O**, **C**, **Q** to select Com-file mode. Press **C** to compile a `.COM` next to the source. A larger program can take a few minutes in the browser. Wait for the compiler to finish before sending another command. To compile and run from inside Pascal, use **R** in Com-file mode. Return to ZShell with **Q** in the main menu and enter the executable's full `H:/work/` path. For example, `H:/work/graphics/MAPVIEW.COM`.

To edit again, enter `zpascal`, press **Y**, and reopen your source with **W**. Select the compile mode your lesson uses before running again.

## Projects and folders

Every lesson supplies complete files, so you can start anywhere. The last column names the lessons whose ideas each one assumes.

| Lesson | Book files | Your folder | Builds on |
| --- | --- | --- | --- |
| [The Silent Lab]({{< relref "the-silent-lab" >}}) | Named in the lesson | `H:/work/` | Nothing |
| [Minutes and Seconds]({{< relref "minutes-and-seconds" >}}) | `H:/book/table/` | `H:/work/table/` | The Silent Lab |
| [The Running Total]({{< relref "running-total" >}}) | `H:/book/calc/` | `H:/work/calc/` | Minutes and Seconds |
| [The Lab, Rewired]({{< relref "the-lab-rewired" >}}) | `H:/book/rewired/` | `H:/work/rewired/` | The Silent Lab, The Running Total |
| [Search the Notes]({{< relref "search-the-notes" >}}) | `H:/book/search/` | `H:/work/search/` | Minutes and Seconds |
| [Keep a Collection]({{< relref "keep-a-collection" >}}) | `H:/book/catalog/` | `H:/work/catalog/` | The Lab, Rewired, Search the Notes |
| [A Panel on the Screen]({{< relref "on-the-screen" >}}) | `H:/book/screen/` | `H:/work/screen/` | Minutes and Seconds |
| [A Map in Tiles]({{< relref "a-map-in-tiles" >}}) | `H:/book/graphics/` | `H:/work/graphics/` | The Running Total |
| [A Tune for the Beacon]({{< relref "a-beacon-tune" >}}) | `H:/book/sound/` | `H:/work/sound/` | Minutes and Seconds |
| [Catch the Beacon]({{< relref "catch-the-beacon" >}}) | `H:/book/game/` | `H:/work/game/` | A Panel on the Screen |

## Get back to a working program

- **The compiler reports an error.** Press **Escape** to jump to the reported location. The real mistake is sometimes on the line before. Fix it, press **S**, and run again.
- **A program is waiting.** Most book programs quit with **q**. Programs with a `>` prompt need **Enter** after it. The tile map closes with any key.
- **The screen still shows a program’s picture.** At the returned prompt, press **O**, then **Q** to redraw the Pascal menu.
- **R runs the wrong file.** **R** and **C** work on the open Work file. After viewing a report or export, reopen your source with **W**.
- **You want a clean example.** Open the `H:/book/` path again with **W**. If you edited a book file, [Refresh lesson examples]({{< relref "/about" >}}#refresh-lesson-examples) restores it.
- **A program stops responding or leaves the screen unreadable.** If you can, save with **S**. Reload the browser page, then choose **Open Zeal** again. Reloading discards unsaved editor text.

## Pattern lookup

| Pattern | See it in use |
| --- | --- |
| Program structure, decisions, and a command loop | [The Silent Lab]({{< relref "the-silent-lab" >}}) |
| A counted loop, function result, or `var` parameter | [Minutes and Seconds]({{< relref "minutes-and-seconds" >}}) |
| Converting text to numbers and checking bounds | [The Running Total]({{< relref "running-total" >}}) |
| Checking for overflow before multiplying | [The Running Total]({{< relref "running-total" >}}) |
| Enum indexes, records, and sets | [The Lab, Rewired]({{< relref "the-lab-rewired" >}}) |
| Sequential text reads and `IOResult` | [Search the Notes]({{< relref "search-the-notes" >}}) |
| Refusing to overwrite an existing file | [Search the Notes]({{< relref "search-the-notes" >}}) |
| Typed records, `Seek`, and `Flush` | [Keep a Collection]({{< relref "keep-a-collection" >}}) |
| Key polling and console cleanup | [A Panel on the Screen]({{< relref "on-the-screen" >}}) |
| Tiles, palettes, and restoring the video mode | [A Map in Tiles]({{< relref "a-map-in-tiles" >}}) |
| Sound voices and a quiet exit | [A Tune for the Beacon]({{< relref "a-beacon-tune" >}}) |
| A frame loop and restartable game state | [Catch the Beacon]({{< relref "catch-the-beacon" >}}) |

For precise declarations, see the [language guide](https://github.com/zoul0813/zeal-pascal/blob/main/docs/language.md), [IDE guide](https://github.com/zoul0813/zeal-pascal/blob/main/docs/ide.md), or [runtime reference](https://github.com/zoul0813/zeal-pascal/blob/main/docs/runtime.md).
