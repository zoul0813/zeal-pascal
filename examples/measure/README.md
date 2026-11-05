# Measure Lab

Two separate Pascal programs collect a batch of measurements and calculate its
statistics. They hand control to each other with Execute, exchanging data on
disk while starting each program with fresh globals.

## Run

Copy this entire folder to Zeal. In ZShell use `cd T:/examples/measure` and
`./measure.bin`, or `cd H:/measure` when HostFS points at the examples tree.
Keep `stats.bin` alongside it. Both executables are standalone and need no IDE
library. To rebuild, compile `stats.pas` and `measure.pas` separately in saved
Com-file mode, then copy `stats.COM` to `stats.bin` and `measure.COM` to `measure.bin`.
Each program executes the other by its `.bin` name. Rebuild both after changing the compiler/runtime. Normal Start/End
settings suffice. Memory mode cannot perform Execute: it reports I/O21 and
returns to the collector's menu with the saved dataset retained.

## Try it

Type `demo.in`: the first R finds no saved batch and returns to the collector.
N starts a new batch; Y confirms replacing any saved `values.dat`. Both `bad`
(not a number) and `9e38` (out of range) are refused before 10, 20 and 30 are accepted. A blank line finishes
the batch. The collector explicitly closes it and executes `stats.bin`.

The report displays 3 samples, mean 20.00, minimum 10.00, maximum 30.00 and sample
standard deviation 10.00. It closes its input and executes `measure.bin` again.
R reads the same saved batch in another fresh report process; Q quits after the
report hands control back. **Neither Execute returns to its Pascal caller.**
This is application composition with independent program memory and persistent
data, rather than a returning function call or a shared-global overlay.

## Pascal features on show

- Execute handoff in both directions, with I-/IOResult recovery on load failure.
- A typed `file of real` shared across independently compiled programs.
- Explicit Close before transfer, quit and Ctrl-Z; no implicit finalization.
- Welford's one-pass mean/variance algorithm and Sqrt on Real6 values.
- Bounded input conversion and validation before changing the dataset.

The collector accepts up to 100 measurements in -1000000..1000000. The report
uses sample standard deviation (dividing by N-1), displayed as zero for a single
sample. It clamps a nonpositive accumulated variance to zero before Sqrt. Real6
rounding applies; results display two decimal places. The file is this port's
native typed-file format, not a portable exchange format.

N asks before replacing an existing file; R only reads it. An empty confirmed
batch is saved but does not automatically start the report. Disk failures stop
with a status and close any owned file. If a report cannot start, the saved batch
remains available and the collector keeps running. If the report cannot return
to the collector, it prints the status and exits. Run `./measure.bin` again.
A missing or unreadable input file never causes the report to create or rewrite
it. It returns to the collector, where N can create a batch. Invalid count or
record contents stop the report with an explanation.

## Edge-case checks

`missing.in` exercises R then Q with `stats.bin` temporarily moved aside; restore
it afterwards. `eof.in` starts a new batch containing 40; send Ctrl-Z at the next
prompt. Run `./stats.bin` to verify the single record was finalized, then use
`reopen.in` to quit the fresh collector. The automated check does this only on
disposable media. All sources, executables and four transcripts are local.
