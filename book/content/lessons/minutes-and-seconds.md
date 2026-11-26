---
title: Minutes and Seconds
kicker: '02 / FOUNDATIONS'
duration: '20–35 minutes'
description: 'Build a conversion table, then let the person at the keyboard decide how long it should be.'
toc: true
weight: 20
---
Timers count in seconds, and people tend to think in minutes. A small lookup table bridges the two. In this lesson you will print one with five rows, then change the program so whoever is at the keyboard chooses the length, and anything that is not a sensible answer is politely refused.

Along the way you will meet Pascal’s counted loop and three routines. One calculates, one prints a table, and one checks what a person typed. That last one is the habit a calculator depends on.

You do not need your adventure from The Silent Lab. Both programs here are complete on their own.

{{< workbench >}}

## Start in Pascal

If the Pascal menu is already on screen, go straight to [Print the first table](#print-the-first-table). To launch Pascal, follow these steps.

1. Choose **Open Zeal** and wait for the `H:/>` prompt. Click inside the screen before typing.
2. Enter `zpascal`.
3. At the message prompt, press **Y** for explanatory compiler messages.

These are the keys this lesson uses. [The Silent Lab]({{< relref "the-silent-lab" >}}) walks through each of them the first time.

| Keys | Where | What they do |
| --- | --- | --- |
| **W** | Pascal menu | Choose a Work file: open a saved source, or name a new one |
| **A (Save As)** | Pascal menu | Save the current source under another name |
| **E** | Pascal menu | Open the editor |
| **Ctrl-K**, release, then **D** | Editor | Return to the Pascal menu, text still in memory |
| **S** | Pascal menu | Save the source |
| **O**, **M**, **Q** | Pascal menu | Select Memory mode and return to the menu |
| **R** | Pascal menu | Compile and run |
| **O**, **Q** | After a run | Redraw the menu |

After `Program returned.`, the menu keys work at the `>` prompt.

Your copies for this lesson go in `H:/work/table/`. The files in `H:/book/table/` belong to the book, and **Refresh lesson examples** can replace them. **Save As replaces an existing file without asking**, so if you have done this lesson before, choose fresh names such as `TABLE2.PAS` and `PROMPT2.PAS` and use them wherever the lesson names a personal file.

## Print the first table

Choose how to start. Either way, you end up with your own copy saved as `H:/work/table/TABLE.PAS`:

- **Open the supplied copy:** press **W**, enter `H:/book/table/TABLE.PAS`, and press **Enter**. Press **A (Save As)**, enter `H:/work/table/TABLE.PAS`, and press **Enter**.
- **Type it yourself:** press **W**, enter `H:/work/table/TABLE.PAS`, and press **Enter**. Press **E**, type the listing below, then leave the editor with **Ctrl-K**, release, then **D**.

Press **S** to save. Select Memory mode with **O**, **M**, **Q**, then press **R** to run.

{{< source "table/TABLE.PAS" >}}

You should see a heading and five rows.

```text
Minutes  Seconds
      1       60
      2      120
      3      180
      4      240
      5      300
```

### How the table is built

- **Named constants.** `const` gives fixed values names. `SecondsPerMinute` says what the 60 means wherever it appears. `LastMinute` is the single number to change for a longer or shorter table.
- **A counted loop.** `for Minute := 1 to LastMinute do` sets `Minute` to 1 and runs the statement after `do`, then 2, and so on through `LastMinute`, inclusive. A `for` loop does the counting for you. The next program uses `repeat … until` to keep running until someone asks to stop.
- **A function that calculates.** `InSeconds(Minute)` calls a function. Its parameter `Minutes` receives a copy of the caller’s value. Assigning to the function’s own name, `InSeconds := ...`, sets the result, and the caller uses that result like any other Integer. One multiplication may seem too small to deserve a name, but the name makes the loop read like a sentence, and the next program reuses the function unchanged.
- **Field widths.** In `WriteLn`, `:7` and `:9` print each number right-aligned in a field at least that wide. The heading is 7 + 2 + 7 characters, so each number ends under the last letter of its column’s name.

### Try it

1. Press **E** and change `LastMinute` to 8. Before you run it, predict the last row. Leave the editor, press **S**, then **R**, and check your prediction.
2. Meet the compiler on purpose. Press **E** and delete the semicolon after `SecondsPerMinute = 60`, save, and run. The compiler may report the problem on the following line, because that is where it first notices something missing. Press **Escape** at the message to see the reported location in the editor. Put the semicolon back, leave the editor, save, and run again.

**A good place to pause.** Your table is saved as `H:/work/table/TABLE.PAS`. [Keep what you built](#keep-what-you-built) explains how to pick up again.

## Let the reader choose the length

`TABLE.PAS` decides its length when you compile it. The next program asks while it runs, which means it has to cope with whatever a person actually types: a number, an empty line, a stray letter, a number far too large.

This is a separate file, so your first table keeps working. If `TABLE.PAS` has unsaved changes, press **S** before switching. Then choose your route to `H:/work/table/PROMPT.PAS`:

- **Open the supplied copy:** press **W**, enter `H:/book/table/PROMPT.PAS`, and press **Enter**. Press **A (Save As)**, enter `H:/work/table/PROMPT.PAS`, and press **Enter**.
- **Type it yourself:** press **W**, enter `H:/work/table/PROMPT.PAS`, and press **Enter**. Press **E**, type the listing below, then leave the editor with **Ctrl-K**, release, then **D**.

Press **S** to save. If you’ve reopened Pascal or changed compile modes, select Memory with **O**, **M**, **Q**. Press **R** to run. At the question `How many rows (1..20, or q)?`, enter `3` for a three-row table. The question comes back, ready for another answer.

{{< source "table/PROMPT.PAS" >}}

### Three routines, three jobs

| Routine | Kind | Its job |
| --- | --- | --- |
| `InSeconds` | Function | Calculates one value and returns it. Unchanged from `TABLE.PAS`. |
| `PrintTable` | Procedure | Does a job, printing one table, and returns nothing. |
| `ValidRows` | Function with a `var` parameter | Answers yes or no, and on yes, delivers the number. |

The main program at the bottom asks, decides which routine to call, and asks again.

**`PrintTable`.** Look at where `LastMinute` went. In `TABLE.PAS` it was a constant, fixed when you compiled. Here it is a parameter, so each call can print a different length. That one move is what this program is for. `Minute` is now declared inside the procedure, so it belongs to `PrintTable` alone, and the loop starts it at 1 for every table.

**`ValidRows`.** This function has two things to report. Its Boolean result says whether the answer is usable. Its `var Count` parameter carries the number back. Without `var`, `Count` would be a copy, as `Minutes` is in `InSeconds`. With `var`, `Count` is another name for the caller’s variable, `Rows`, so assigning to `Count` changes `Rows`.

The function begins with `ValidRows := False` and sets True only after every check has passed: the answer is no until proven otherwise. A refused answer never reaches `Count`, so `Rows` stays as it was and the caller prints a correction instead of a table.

`type` gives a kind of value a name. Here, `InputLine` names `string[3]`, a string with room for three characters. The declaration lets `Line` and the parameter `Text` use the same type. `Length(Text)` counts the characters the string holds, and `Text[1]` is its first character.

The checks run in this order.

1. **One or two characters.** `InputLine` holds three, one more than any valid answer needs. If a person types `150`, all three characters arrive and the answer is refused. With room for only two, it would arrive as `15` and quietly print the wrong table. The length check also keeps an empty line away from `Val`, which can report success for empty input without assigning a number.
2. **A digit first.** This keeps signs and Pascal’s `$` hexadecimal notation out of the prompt. The check reads `Text[1]`, so it sits *inside* the length check rather than joining it with `and`: Pascal evaluates both sides of `and`, even when the first is already false.
3. **An integer.** `Val` converts the text and sets `Code` to zero on success. `2x` passes the first two checks and fails here.
4. **In range.** Only a value from 1 to `MaxRows` reaches the table. The largest table ends at 1,200 seconds, well inside Zeal Pascal’s Integer range.

**The main loop.** `repeat ... until Line = 'q'` keeps asking until the answer is `q`. Inside are two `if` statements and one `else`. Pascal attaches an `else` to the nearest `if` that does not have one yet, so the correction belongs to the `ValidRows` test. Entering `q` skips both the table and the correction.

The question uses one `Write` to print text, the value of `MaxRows`, and then more text. The correction message does the same. Change the constant, and both follow.

### Test the prompt

Run the program and work down this table. Check the refusals as carefully as the tables.

| Type | What should happen |
| --- | --- |
| `3` | A three-row table, then the question again. |
| *(just **Enter**)* | `Enter a whole number from 1 to 20.` |
| `0` | The correction. |
| `20` | A twenty-row table ending at 20 and 1200. |
| `21` | The correction. |
| `2x` | The correction: it fails the conversion check. |
| `150` | The correction: it is too long. |
| `-5` | The correction: it does not start with a digit. |
| `2` | A two-row table. Refusals have not broken anything. |
| `q` | The program ends and Pascal shows `Program returned.` |

Only lowercase `q` quits. `Q` is just another answer to refuse.

**One more experiment.** Change `MaxRows` to 30, save, and run. The question, the correction and the limit all change together, and `30` now prints a table. That is a constant earning its name. To accept three-digit requests, widen the length check and give the string room for one extra character.

## Keep what you built

In the Pascal menu, press **S** before closing the workbench. Running a program does not save its source.

| File | What it holds |
| --- | --- |
| `H:/work/table/TABLE.PAS` | The fixed-length table |
| `H:/work/table/PROMPT.PAS` | The table that asks first |

To resume later, open Zeal, enter `zpascal`, press **Y**, then press **W** and enter the file you want. [About the workbench]({{< relref "/about" >}}#your-files) explains how to download a backup outside the browser.

## What comes next

You now have a small tool that answers a real question, and three routines that each do one job. The quietest is `ValidRows`. It checks the text read by the main loop before the program acts on it.

[The Running Total]({{< relref "running-total" >}}) works the same way, with a harder last check. A running total can grow beyond what an Integer holds, so before it adds or multiplies, it has to ask whether the answer will fit.
