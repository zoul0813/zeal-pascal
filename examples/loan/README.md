# Loan

A loan amortization calculator with Real arithmetic and a complete monthly table.

## Run

Copy this entire folder to a writable native Zeal drive. In ZShell, change into
it (`cd T:/examples/loan` if installed there), then run `./loan.bin`.
The included executable is standalone: it needs neither the IDE nor its ZPL library.

To edit and rebuild, start the installed IDE from this same directory (for example
`T:/bin/zpascal.bin`) and use W to open `loan.pas`. Choose O, M, Q for
Memory output, then R. After the run, Escape returns to the IDE. For a saved
executable choose O, C, Q and C; the compiler writes `loan.COM` and a source
map. That COM is the same native executable format as the supplied `.bin`.
The normal automatic Start and default End settings suffice.

Keep the current directory set to this folder: opening a source file does not
change the current directory. The IDE's L option selects drive roots, so using
it changes where relative data filenames resolve.

## Two-minute demo

Press Enter three times (`demo.in`) for 10000 at 6% over three years.
The monthly payment is 304.22, followed by 36 rows and a final balance of 0.00.
Try `zero.in`: 1200 at zero interest over one year pays 100.00 per month.

Inputs accept defaults or decimal numbers. Principal: 1..1000000; annual rate:
0..25%; term: 1..30 whole years. A fractional term ends with an explanation.
The full schedule scrolls; a 30-year loan prints up to 360 rows.

`demo.in` contains exact input lines, including blank lines where needed.
It is a human replay transcript and an automated-test fixture; no shell input
redirection support is assumed. End of input (Ctrl-Z) exits cleanly.

## Pascal features on show

- Real6 arithmetic and Exp/Ln calculate the annuity payment.
- A zero-interest branch avoids division by zero; tiny rates use a discounted
  payment sum to avoid subtracting nearly equal Real values.
- `cents` uses Int on Real values, avoiding 16-bit Integer cents overflow.
- Formatted WriteLn fields produce the payment/interest/balance table.
- Interest is rounded each month; the last payment absorbs the residual.

## Files and native behavior

The folder contains `loan.pas`, standalone `loan.bin`, this README, `demo.in` and `zero.in`.
Writes no files. Amounts are illustrative, using positive half-up cent rounding;
fees, lender rules and legal disclosures are outside this example. Very small
loans over long terms can have a rounded regular payment of zero; the final
payment still clears the balance. This is a calculation demo, not financial advice.

These are native Zeal programs in the port's current Turbo Pascal 3 dialect.
They use no CP/M BDOS calls or terminal-specific CRT extensions. They demonstrate
application types familiar from the CP/M era; historical binary or complete
language compatibility is not implied.
