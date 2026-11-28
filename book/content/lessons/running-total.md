---
title: The Running Total
kicker: '03 / CALCULATOR'
duration: '30–45 minutes'
description: 'Build a desk calculator that keeps a total, remembers a number, and shows recent results.'
toc: true
weight: 30
---
A price tag is easy to read. A parts order for the lab is harder. There are bulbs, a box of fuses, a discount, and a number you meant to remember. In this project you build a desk calculator that shows the total after every step and refuses anything that would make that total wrong.

There are two programs. `TOTAL.PAS` works in whole numbers and checks each result before it calculates. `CALC.PAS` adds decimals, a memory, and a short history, then leaves the IDE as a program of its own. Each file is complete, so you can start with either one.

**Before you start.** You should be able to open, save, and run a file in Pascal, as in [The Silent Lab]({{< relref "the-silent-lab" >}}). The input checks here grow out of `ValidRows`, so look back at [Minutes and Seconds]({{< relref "minutes-and-seconds" >}}) if `Val` or a `var` parameter is new to you.

{{< workbench >}}

## Whole numbers first

1. Open Pascal as described in the [workbench reference]({{< relref "workbench-reference" >}}#start-and-save).
2. Press **W**, enter `H:/book/calc/TOTAL.PAS`, and press **Enter**.
3. Press **A (Save As)**, enter `H:/work/calc/total.pas`, and press **Enter**. Save As replaces an existing file without asking, so choose a fresh name such as `total2.pas` if you have done this lesson before.
4. Select Bin-file with **O**, **C**, **Q**, then press **R** to compile and run.

{{< source "calc/TOTAL.PAS" >}}

The prompt shows the current total. Type an operation followed by a number, with no space between them, and press **Enter**. Work down the table and check each refusal as carefully as each answer.

| Type | Next total |
| --- | --- |
| `+24` | 24 |
| `-4` | 20 |
| `*3` | 60 |
| `*99` | Still 60. The product would pass the limit. |
| `+990` | Still 60. The sum would pass 999. |
| `/2` | Still 60. This version has no division yet. |
| `q` | Return to Pascal. After `Program returned.`, the menu keys work at the prompt. |

### Check first, then calculate

The first character picks the operation. The rest of the line goes through `Val`, and the total changes only after the conversion and the range checks pass. A refused line prints a reminder and leaves `Total` exactly as it was. The check on `Line[2]` keeps out `$`, Pascal’s hexadecimal prefix.

Every number is capped at 999 in either direction. That is far smaller than an Integer allows, and it is deliberate. Two numbers of at most 999 add up to at most 1,998, so `Next` always fits, and the program can compare it with the limit afterward.

Multiplication breaks that guarantee. 999 times 999 is too large for an Integer, and the result can wrap around to a wrong number without any warning. So the program asks before it multiplies. If the size of the total is no more than `Limit div Abs(Value)`, the product fits. Multiplying by zero gets its own branch, because asking the question would mean dividing by zero.

### Try it

1. Enter `+999`, then `*999`. Predict the total before you press **Enter**.
2. Press **E** and change `Limit` to 99. Save, run, and enter `+150`. The refusal still says `-999..999`, because that message is typed out by hand. Change the last `WriteLn` so it prints `-Limit` and `Limit`, the way Minutes and Seconds printed `MaxRows`.
3. Give the calculator whole-number division. Add this line directly after the `'-'` line in the `case`.

```pascal
          '/': if Value = 0 then Ready := False else Next := Total div Value;
```

Put it there rather than after the `'*'` branch. That branch has no semicolon, because the `case` statement’s own `else` follows it. Run the program, make the total 60, and enter `/7`. `div` keeps the whole part and drops the rest, so the answer is 8. The next program divides properly.

**A good place to pause.** Press **S**. Your whole-number calculator is saved as `H:/work/calc/total.pas`.

## Decimals, memory, and history

`CALC.PAS` is a separate program, so your first calculator keeps working.

1. If `total.pas` has unsaved changes, press **S**.
2. Press **W**, enter `H:/book/calc/CALC.PAS`, and press **Enter**.
3. Press **A (Save As)**, enter `H:/work/calc/calc.pas` or another unused name, and press **Enter**.
4. Press **R** to compile and run. If you reopened Pascal, select Bin-file with **O**, **C**, **Q** first.

{{< source "calc/CALC.PAS" >}}

| Type | What happens |
| --- | --- |
| `+12.50` | The total becomes 12.50. |
| `m` | `Stored.` The memory now holds 12.50. |
| `/4` | The total becomes 3.13. |
| `/0` | Refused. The total stays 3.13. |
| `r` | The total returns to 12.50 from memory. |
| `h` | Three results, oldest first. 12.50, 3.13, then 12.50. |

### Reading a decimal

`ReadOperand` checks the shape of the number before `Val` sees it. It accepts digits, one optional minus sign right after the operation, and at most one dot. Like `ValidRows`, it returns a Boolean and hands the number back through a `var` parameter.

The total is a Real now, so `/` can give a fraction. Each display rounds to two decimal places with `:0:2`, but the stored value keeps more detail. After `r`, enter `/4` again, then `*2`. The underlying 3.125 becomes 6.25 because the multiplication uses the stored value.

Division by zero is refused before it happens, just as `TOTAL.PAS` refuses an oversized product. The limit is now 9999 in either direction.

### A history that scrolls

`History` has five numbered slots. `Used` counts how many of them hold a result. Once all five are full, `RememberResult` copies each entry one slot toward the start and writes the newest result in the last slot. `m` does not add a line, but `r` does, because it changes the total.

### Try it

1. Enter `+1` six times, then `h`. Which result scrolled away?
2. Change `Slots` to 8 and repeat. The array, the loops, and the full-history check all follow the constant.
3. Add a `c` command that clears the total. Put this branch after the `end` of the `r` branch, just before `else if Line = 'h'`. Then add `c` to the help line at the top.

```pascal
    else if Line = 'c' then
    begin
      Total := 0;
      RememberResult
    end
```

**A good place to pause.** Press **S**. Your decimal calculator is saved as `H:/work/calc/calc.pas`.

## Take it out of the IDE

So far the calculator has run inside Pascal. Now compile it into a program you can start from the shell.

1. Quit the calculator with `q`, then press **S** to save the source.
2. Select Bin-file mode with **O**, **C**, **Q**, then press **C** to compile. Pascal writes `H:/work/calc/calc.bin` next to your source. If you chose another source name, use its lowercase stem followed by `.bin`.
3. Press **Q** to leave Pascal for ZShell.
4. Enter `H:/work/calc/calc.bin`. When you are done, enter `q` to return to the shell.

The executable starts with a zero total and an empty history every time. Changes to the calculator itself live in the source. To keep working on it, enter `zpascal`, press **Y**, and open your source with **W**. Choose the lesson’s compile mode again before running it.

## What comes next

You now have a calculator that checks before it acts and an array that keeps a short memory. [The Lab, Rewired]({{< relref "the-lab-rewired" >}}) indexes arrays by room names instead of numbers and turns a whole map into data.
