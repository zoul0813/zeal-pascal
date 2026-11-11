---
title: The Silent Lab
kicker: '01 / FIRST CONTACT'
duration: '30–60 minutes'
description: 'An abandoned computer lab. Three rooms to explore. A small Pascal program that makes them yours.'
toc: true
---
The lab has been closed for years. Behind its entrance is a room of silent terminals, and beyond that, a workshop. You are going to make a little world you can walk through by typing commands.

By the end, you will have launched Pascal, run a three-room adventure, changed it, and saved your own copy. This is a first encounter, not an exam. Stop whenever you like; the final section explains how to pick up where you left off.

## Before you enter

Use a desktop or laptop with a keyboard, preferably current Chrome or Edge for this pilot. The emulator needs WebGL, WebAssembly, and browser filesystem storage. Touch typing on phones and tablets is not supported.

You only need to recognize a variable as a named value, and a loop as something that repeats. We will introduce the Pascal spelling as we go. No assembly or hardware knowledge is needed.

The computer is available from **Open Zeal** at the top of every page. Opening the workbench starts the emulator automatically. You can read ahead first, open it now, or hide it while reading. Hiding it keeps the computer running.

{{< workbench >}}

## 1. Start the computer and launch Pascal

Choose **Open Zeal**. The emulator starts automatically. After the startup messages, you should see a ZShell prompt. Click inside the screen before typing.

Enter this command and press **Enter**:

```text
zpascal
```

The welcome screen comes from ZShell's startup files on B:, the EEPROM. The startup script adds Pascal and the community tools to PATH and starts the shell in H:/. From there, `zpascal` finds its compiler library in `lib/`. T: contains the community tools; you do not need to install anything. If you change directories before launching Pascal, enter `cd H:/` first.

The first launch can take a moment. At the compiler-message prompt, press **Y** for explanatory messages. You should then see the Pascal main menu.

> Keys go to the Zeal while the screen has focus. Press **F8** to return focus to the book. Click the screen to type again. **Expand** gives the computer more room.

## 2. Make a tiny program first

In the Pascal menu, press **W** for Work file. Type `H:/work/LAB.PAS` and press **Enter**. A missing file gives you an empty work buffer. Then press **E** to enter the editor.

Type this program. If you prefer to start from supplied code, open `H:/book/lab/HELLO.PAS` with **W** instead, then press **E**.

{{< source "HELLO.PAS" >}}

A few landmarks:

- `program SilentLab;` names the program.
- `begin` and `end.` enclose its main instructions. The final period ends the program.
- `WriteLn` writes a line of text. Strings use single quotes.
- Semicolons separate statements. There is no need for one immediately before the final `end`.

Press **Ctrl-K**, release both keys, then press **D** to return to the menu. This is a two-step editor command, not three keys held together. The text remains in RAM.

Press **S** to save the source. Then press **R** to compile and run in Memory mode. The screen should display:

```text
THE SILENT LAB
You stand outside an abandoned computer lab.
```

Follow the on-screen return prompt to get back to the menu. You have just written and run a native Zeal program.

### If it does not compile

Read the diagnostic, then return to the editor as prompted. Check quotes, parentheses, the semicolon between the two `WriteLn` calls, and the period after `end`. Correct the source, save, and try **R** again.

Making a mistake and finding its location is part of programming. No files are harmed by a syntax error.

## 3. Give the lab three rooms

A greeting always does the same thing. An adventure needs to remember where you are and react to input.

Here is our map:

```text
       TERMINAL ROOM ---- WORKSHOP
             |
          ENTRANCE
```

Start with the prepared adventure: from the Pascal menu, press **W**, enter `H:/book/lab/START.PAS`, then **E**. If your workbench shows a newer lesson folder after a fresh start, use that folder instead.

Before changing it, make your own copy. Return to the menu with **Ctrl-K**, then **D**. Press **A** for Save As, enter `H:/work/LAB.PAS`, and confirm any overwrite only if you want to replace your earlier greeting. To keep both, choose `H:/work/ADVENT.PAS` instead. Press **E** to inspect your copy.

Readers who prefer a blank start can type the following listing into a new Work file instead. It is the same program as `START.PAS`.

{{< reveal "Show the complete starting program" >}}
{{< source "START.PAS" >}}
{{< /reveal >}}

### Two values carry the world

```pascal
var
  Room: Integer;
  Command: string[20];
```

`var` introduces variables. `Room` is an integer: a whole number. We use 1 for the entrance, 2 for the terminal room, and 3 for the workshop. A future lesson can give these values more descriptive names.

`Command` holds up to 20 characters. Zeal Pascal strings have an explicit capacity: `string[20]`. `ReadLn(Command)` reads a line of input into it.

```pascal
Room := 1;
```

`:=` assigns a value. `=` compares values. That distinction matters: `Room := 1` changes the room; `Room = 1` asks whether we are at the entrance.

### Repeat a turn

The `repeat ... until` loop displays the room, asks for a command, and handles it. It runs at least once. Its final line is:

```pascal
until Command = 'quit';
```

The loop stops when the reader types `quit`. Commands in this first version are lowercase. Use `n`, `s`, `e`, `w`, `look`, or `quit`, followed by Enter.

The `case Room of` statement selects the appropriate description. An `if ... then ... else` decides whether a direction leads somewhere. `begin ... end` groups several instructions into one branch.

The `{$R+}` directive near the top enables range checks. Leave it in place as you experiment.

## 4. Explore and check the rules

Return to the menu, save, and press **R**. Try this sequence:

| Type | What should happen |
| --- | --- |
| `n` | Move from the entrance into the terminal room. |
| `e` | Enter the workshop. |
| `look` | See the workshop description again. |
| `n` | See “There is no door north.” and remain in the workshop. |
| `w` | Return to the terminal room. |
| `s` | Return to the entrance. |
| `xyz` | See a reminder of the available commands. |
| `quit` | Leave the adventure. |

The program displays the current room at the start of each turn. That is why `look` needs no special action: leaving `Room` unchanged makes the next turn describe the same room.

Check both successful and unsuccessful moves. A room number should change only when there is a door in that direction. There is no automatic grader; use the map and the table to decide whether your version behaves correctly.

## 5. Leave something running

The workshop is quiet. Give it one small mystery.

First, change its description to mention an amber light. Compile and run again to check that you changed the right room.

Then add a command named `inspect`:

- In the workshop, it should print `A label reads: DO NOT SWITCH OFF.`
- In either other room, it should print `Nothing here needs a closer look.`
- It should leave the player in the same room.

Add `inspect` to both command-help messages so a player can discover it. Test it in all three rooms, and make sure movement and `quit` still work.

{{< reveal "Hint: where does the new command go?" >}}
Add another `else if Command = 'inspect' then` branch after the movement branches and before the unknown-command check. Use `begin ... end` for the branch, with an `if Room = 3 then ... else ...` inside it. Do not put a semicolon between the branch's closing `end` and the next `else`.
{{< /reveal >}}

{{< reveal "Reveal the worked solution" >}}
This version adds the light, the command, and both help-message changes. Compare the relevant parts with your program; your room descriptions can be different.

{{< source "SOLUTION.PAS" >}}

The same source is in `SOLUTION.PAS` in your lesson folder. Opening it is optional; save your own work first.
{{< /reveal >}}

## 6. Save it for tomorrow

Return to the Pascal menu and press **S**. Saving source is separate from compiling or running it. An adventure that runs successfully may still have unsaved edits.

For this pilot, save personal files under `H:/work/`. Files saved on H: persist in this browser. Completed writes to the TF card (T:) and EEPROM (B:) persist too, directly in OPFS, without a separate Save Data step. The editor's RAM does not. A workspace download backs up personal H: files; it does not include the TF/EEPROM images.

To make a backup outside the browser:

1. Press **S in Pascal**.
2. Open **Files, backups & fresh starts** below the emulator.
3. Choose **Download H: files** and keep the JSON file.

To resume after a page reload, start the Zeal, launch Pascal with the same command, press **Y**, and open `H:/work/LAB.PAS` (or your chosen filename) using **W**, then **E**. You are restoring saved source, not a snapshot of the running computer.

**Fresh lesson copy** prepares a new folder of original examples and shows its path. It preserves previous lesson files and your editor buffer. **Load all book code** adds the complete pilot collection without overwriting existing files; there is currently one lesson. **Import backup** restores into a new directory under `H:/work/`, leaving existing files alone. Use its displayed path when opening imported source.

Browser storage belongs to this browser and site address. It does not follow you to another device or between a localhost preview and GitHub Pages. Clearing site data can remove it, so keep downloaded backups. Use one workbench tab at a time.

## What you have now

You have a working little adventure and a place to extend it. You have used Pascal variables, bounded strings, input/output, assignment, conditions, and a repeat loop. You have also followed the real edit–compile–run cycle on the Zeal.

For another experiment, add a fourth room or a command to read a notice in the terminal room. Draw the new connection on the map before changing the program. Keep a working copy before you begin.

This is the end of the pilot. The full book will begin with short foundations and grow through a calculator, a larger adventure, a text-file utility, and a collection database, with optional graphics and sound side trips.
