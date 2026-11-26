---
title: The Silent Lab
kicker: '01 / FIRST CONTACT'
duration: '30–60 minutes'
description: 'An abandoned computer lab. Three rooms to explore. A small Pascal program that makes them yours.'
toc: true
weight: 10
---
The lab has been closed for years. Behind its entrance is a room of silent terminals, and beyond that, a workshop. You are going to make a little world you can walk through by typing commands.

By the end, you will have launched Pascal, run a three-room adventure, changed it, and saved your own copy. This is a first encounter, not an exam. There are two marked places to pause along the way, and [Save and resume](#6-save-and-resume) explains how to pick up where you left off.

## Before you enter

Use a desktop or laptop with a keyboard, preferably current Chrome or Edge for this pilot. The emulator needs WebGL, WebAssembly, and browser filesystem storage. Touch typing on phones and tablets is not supported.

You only need to recognize a variable as a named value, and a loop as something that repeats. We will introduce the Pascal spelling as we go. No assembly or hardware knowledge is needed.

The computer is available from **Open Zeal** at the top of every page. Opening the workbench starts the emulator automatically. You can read ahead first, open it now, or hide it while reading. Hiding it keeps the computer running.

{{< workbench >}}

### Where your work lives

Save your own programs under **`H:/work/`**. H: is persistent storage: saved files remain after you reload the page and restart Zeal in this browser. Text you have typed but not saved does not.

The supplied files in `H:/book/lab/` belong to the book, and **Refresh lesson examples** can replace them. Copy an example to `H:/work/` before you change it.

**Save As replaces an existing file without asking.** If you have done this lesson before, pick new names for this visit, such as `HELLO2.PAS` and `LAB2.PAS`, and use them wherever the lesson says `HELLO.PAS` or `LAB.PAS`.

H: lives in this browser’s storage, which is not a backup: clearing site data can remove it. Section 6 shows how to download a copy you can keep elsewhere.

## 1. Start the computer and launch Pascal

Choose **Open Zeal**. The emulator starts automatically. After the startup messages, you should see a ZShell prompt on **H:**, your persistent drive. Click inside the screen before typing.

Enter this command and press **Enter**:

```text
zpascal
```

The first launch can take a moment. At the compiler-message prompt, press **Y** for explanatory messages. You should then see the Pascal main menu.

> Keys go to Zeal while the screen has focus. Press **Ctrl+Shift+Space** to switch focus to the book; press it again to return to Zeal. The screen overlay shows when Zeal is not receiving keyboard input. **Expand** gives the computer more room.

## 2. Make a tiny program first

Before entering the lab, make something small that works. Here is the whole program:

{{< source "lab/HELLO.PAS" >}}

Choose how you want to start. Either way, you end up with your own greeting saved as `H:/work/HELLO.PAS` (or your new name for this visit):

- **Type it yourself:** in the Pascal menu, press **W** for Work file, enter `H:/work/HELLO.PAS`, and press **Enter**. If the file does not exist yet, you get an empty editor buffer; nothing is saved until you press **S** later. Press **E** for the editor and type the listing.
- **Open the supplied copy:** press **W**, enter `H:/book/lab/HELLO.PAS`, and press **Enter**. Press **A (Save As)** and enter `H:/work/HELLO.PAS`, and press **Enter**. You are now working on your own copy. Press **E** to look at it in the editor.

While it is on screen, a few landmarks:

- `program SilentLab;` names the program.
- `begin` and `end.` enclose its main instructions. The final period ends the program.
- `WriteLn` writes a line of text. Text inside single quotes is a string, printed exactly as written.
- Semicolons separate statements. The second `WriteLn` is followed directly by `end`, so it needs no semicolon.

To leave the editor, press **Ctrl-K**, release both keys, then press **D**. This is a two-step command, not three keys held together. You return to the Pascal menu with your text still in memory, but not yet saved.

Now save, choose how to run, and run:

1. In the Pascal menu, press **S** to save the source.
2. Select Memory mode: press **O** for Options, **M** for Memory, then **Q** to return to the main menu.
3. Press **R** to compile and run.

The screen should display:

```text
THE SILENT LAB
You stand outside an abandoned computer lab.
```

When Pascal shows `Program returned.`, you can use the main-menu keys at the `>` prompt. To see the menu again, press **O** for Options, then **Q**. You have just written and run a native Zeal program.

### If it does not compile

Read the message, then press **Escape** to return to the editor at the place the compiler stopped. Check the quotes, the parentheses, the semicolon between the two `WriteLn` lines, and the period after `end`. Correct the source, leave the editor with **Ctrl-K** then **D**, press **S** to save, and try **R** again.

Making a mistake and finding its location is part of programming. No files are harmed by a syntax error.

**A good place to pause:** your greeting is saved as `H:/work/HELLO.PAS` (or the name you chose). You can close the page now and use [Save and resume](#6-save-and-resume) when you return.

## 3. Walk through the lab

A greeting says the same thing forever, which is reassuring in a lab and dull in a game. An adventure needs to remember where you are and react to input.

Here is our map:

```text
       TERMINAL ROOM ---- WORKSHOP
             |
          ENTRANCE
```

First, take a walk through the lab. Then we’ll look at how the program remembers where you are.

Choose your route again. This time your personal file is **`H:/work/LAB.PAS`** (or your new name for this visit), so your greeting stays as it is. The adventure is about forty lines, so opening the starter is the quicker route; typing it is a fine choice if you like to feel each line go in. Both produce the same program.

- **Open the starter:** press **W**, enter `H:/book/lab/START.PAS`, and press **Enter**. Press **A (Save As)** and enter `H:/work/LAB.PAS`, and press **Enter** before changing anything.
- **Type it yourself:** press **W**, enter `H:/work/LAB.PAS`, press **Enter**, then **E**. Type the complete listing shown at the start of [How the program works](#4-how-the-program-works), then come back here. Leave the editor with **Ctrl-K**, release, then **D**.

In the Pascal menu, press **S** to save your adventure, then **R** to run it. (If you have reloaded the page since the greeting, select Memory mode again with **O**, **M**, **Q** first.)

The program prints its title and the list of commands, then describes the entrance and waits at a `>` prompt. Commands are lowercase; type each one and press **Enter**. Try this sequence:

| Type | What should happen |
| --- | --- |
| `n` | Move from the entrance into the terminal room. |
| `e` | Enter the workshop. |
| `look` | See the workshop description again. |
| `n` | See “There is no door north.” and remain in the workshop. |
| `w` | Return to the terminal room. |
| `s` | Return to the entrance. |
| `xyz` | See a reminder of the available commands. |
| `quit` | See “You leave the lab. For now.” and end the program. |

Notice that every turn begins by describing the room you are in. Keep that in mind; it explains why `look` turns out to be the easiest command in the program.

Check the refusals as carefully as the moves: you should change rooms only where the map shows a door. There is no automatic grader. The map and the table are your test plan.

After `Program returned.`, you can use the main-menu keys at the `>` prompt. Press **O**, then **Q**, if you want to see the menu again.

**Another good place to pause:** your adventure is saved as `H:/work/LAB.PAS` (or your chosen name). You have walked through it; next you will take it apart. Use [Save and resume](#6-save-and-resume) if you stop here.

## 4. How the program works

Here is the complete starting program. It is the same as `H:/book/lab/START.PAS` and your `LAB.PAS`. Press **E** if you would like your own copy on the Zeal screen while you read; the tour below follows it from top to bottom.

{{< source "lab/START.PAS" >}}

### The top: a name, a safety switch and two variables

```pascal
program SilentLab;
{$R+}
var
  Room: Integer;
  Command: string[20];
```

The first line names the program, as in the greeting. `{$R+}` is an instruction to the compiler rather than to the player: it turns on range checking, so certain out-of-range mistakes are reported instead of passing silently. Leave it in place as you experiment.

`var` introduces variables. `Room` is an `Integer`, a whole number. We use 1 for the entrance, 2 for the terminal room, and 3 for the workshop. A later lesson can give these values more descriptive names.

`Command` will hold what the player types. Zeal Pascal strings have an explicit capacity, and `string[20]` holds up to 20 characters. Commands here are much shorter than that.

Each declaration is a name, a colon, a type and a semicolon.

### Setting the scene

```pascal
begin
  Room := 1;
  WriteLn('THE SILENT LAB');
  WriteLn('Commands: n, s, e, w, look, quit');
```

`:=` assigns a value: `Room := 1` puts the player at the entrance. A plain `=` compares two values instead, so `Room = 1` asks whether the player is at the entrance. Mixing the two up is one of the most common Pascal slips, so it is worth a second glance whenever you write either.

The two `WriteLn` lines run once, before the game begins.

### One turn, repeated

Everything between `repeat` and `until` is one turn of the game:

```pascal
repeat
  WriteLn;
  case Room of
    1: WriteLn('Entrance. A door leads north.');
    2: WriteLn('Terminal room. Doors lead south and east.');
    3: WriteLn('Workshop. The terminal room is west.')
  end;
  Write('> ');
  ReadLn(Command);
```

`WriteLn;` on its own prints an empty line, which gives each turn a little breathing room.

`case Room of` picks one line according to the value of `Room`. Each choice starts with a value and a colon. The `case` has its own closing `end;`. As in the greeting, the last choice needs no semicolon before that `end`.

`Write('> ')` prints the prompt *without* moving to a new line, so the player types beside it. `WriteLn` would move to the next line first. `ReadLn(Command)` waits for the player to type a line and press Enter, then stores it in `Command`.

### Choosing what to do

Next comes a chain of tests:

```pascal
if Command = 'n' then
begin
  if Room = 1 then Room := 2
  else WriteLn('There is no door north.')
end
else if Command = 's' then
```

Pascal checks `if Command = 'n'` first. If it is false, it moves to the `else if` for `'s'`, then `'e'`, and so on, and takes the first branch that matches. The rest are skipped.

`begin ... end` groups the instructions for one branch. Inside, a second `if` asks whether this room has a door north. If it does, `Room` changes. If not, the program prints a refusal and `Room` stays as it was. The comparison is exact, so `N` in capitals is not the same as `n` and is treated as an unknown command.

The last test catches everything else:

```pascal
else if (Command <> 'look') and (Command <> 'quit') then
  WriteLn('Try n, s, e, w, look, or quit.')
```

`<>` means “not equal.” Each comparison in parentheses is either true or false, and `and` is true only when both are. So the help message appears only for a command that is neither `look` nor `quit`. Keep the parentheses: in Pascal, `and` is applied before comparisons such as `<>`, so each comparison needs its own pair.

Here is the payoff for noticing that each turn begins with a description: `look` needs no code of its own. It changes nothing, so the next turn describes the same room again.

### The end of the loop

```pascal
until Command = 'quit';
WriteLn('You leave the lab. For now.')
end.
```

After each turn, `until` checks its condition. If the player typed `quit`, the loop stops; otherwise another turn begins. A `repeat` loop always runs at least once, which suits a game that should describe the first room before asking anything. The final `WriteLn` says goodbye, and `end.` closes the program.

## 5. Leave something running

The workshop is quiet. Give it one small mystery.

Make sure you are editing your own `H:/work/LAB.PAS`, not the book’s starter. If you are at the Pascal menu, press **E** to open the editor.

**First, a small change.** Find the workshop’s line in the `case` statement and change its description to mention an amber light. Change only the words inside the quotes, and avoid apostrophes for now so the surrounding quotes stay balanced. Leave the editor, press **S** to save, then **R** to run, and walk to the workshop to check that you changed the right room.

**Then, a new command.** Add a command named `inspect`:

- In the workshop, it should print `A label reads: DO NOT SWITCH OFF.`
- In either other room, it should print `Nothing here needs a closer look.`
- It should leave the player in the same room.

Add `inspect` to both command lists, the one printed at the start and the help message for unknown commands, so a player can discover it.

**One rule to watch while you add it:** do not put a semicolon immediately before `else`. In the chain of tests, each branch’s `end` is followed directly by the next `else`. A semicolon there ends the `if` statement early, leaving the `else` with nothing to belong to, and the compiler will stop.

Save and run again. Test `inspect` in all three rooms, then make sure the moves, `look`, an unknown command and `quit` still behave as they did in your walkthrough.

{{< reveal "Hint: where does the new command go?" >}}
Add another `else if Command = 'inspect' then` branch after the movement branches and before the unknown-command check. Use `begin ... end` for the branch, with an `if Room = 3 then ... else ...` inside it. Do not put a semicolon between the branch's closing `end` and the next `else`.
{{< /reveal >}}

{{< reveal "Reveal the worked solution" >}}
This version adds the light, the command, and both help-message changes. Compare the relevant parts with your program; your room descriptions can be different.

{{< source "lab/SOLUTION.PAS" >}}

The same source is in `H:/book/lab/SOLUTION.PAS`. Opening it is optional. If you do, save your own work with **S** first, so nothing unsaved is lost when you switch files. Before changing the solution, use **A (Save As)** to give it an unused personal name such as `H:/work/SOLVED.PAS`.
{{< /reveal >}}

## 6. Save and resume

Whenever you stop, return to the Pascal menu and press **S**. Saving is separate from compiling or running: a program that runs perfectly can still have unsaved edits, and reloading the page discards them.

Your files:

| File | What it holds |
| --- | --- |
| `H:/work/HELLO.PAS` | Your greeting |
| `H:/work/LAB.PAS` | Your adventure |
| `H:/work/SOLVED.PAS` | Your copy of the solution, if you made one |

If you chose new names for this visit, use those instead. If you still have a book starter open, use **A (Save As)** with an unused `H:/work/` name; Save As replaces an existing file without asking. Avoid the IDE’s default T: source path for personal work, because **Update Zeal** replaces T:.

**To keep a backup outside the browser:**

1. In the Pascal menu, press **S**.
2. Find **Your files** below the emulator.
3. Choose **Download backup** and keep the JSON file somewhere safe.

H: is stored by this browser for this site address. It does not follow you to another device or browser, and clearing site data can remove it. The downloaded file is your backup. Use one workbench tab at a time.

**To resume after a reload:**

1. Open Zeal and wait for the ZShell prompt.
2. Enter `zpascal`, then press **Y** at the message prompt.
3. Press **W**, enter `H:/work/LAB.PAS` (or the file you were working on), and press **Enter**.
4. Press **E** to carry on editing.
5. When ready to run, leave the editor with **Ctrl-K**, release, then **D**, and press **S** to save. Select Memory mode with **O**, **M**, **Q**, then press **R**.

You are reopening saved source, not a snapshot of the running computer.

For **Restore backup**, **Update Zeal**, **Refresh lesson examples**, or moving from an older workbench, see [Help with files and updates]({{< relref "/about" >}}#your-files). Save personal work in `H:/work/` before updating Zeal.

## What you have now

You have a small adventure that remembers where you are, refuses doors that do not exist, and keeps one secret in the workshop. Along the way you used Pascal variables, a bounded string, input and output, assignment, comparisons, an `if ... else if` chain, a `case` statement and a `repeat` loop. You also went round the real edit–save–compile–run cycle on Zeal, and probably met the compiler at least once.

If you want one more experiment, add a fourth room or a command that reads a notice in the terminal room. Draw the new door on the map before you change the program, and Save As an unused `H:/work/` name first so your working version stays intact.

The amber light is still blinking. What it is keeping running can wait for another day.

Continue with [Minutes and Seconds]({{< relref "minutes-and-seconds" >}}) to build a conversion table. For reference as you experiment: [IDE commands](https://github.com/zoul0813/zeal-pascal/blob/main/docs/ide.md), [Pascal language guide](https://github.com/zoul0813/zeal-pascal/blob/main/docs/language.md), and [compiler errors](https://github.com/zoul0813/zeal-pascal/blob/main/docs/errors.md).
