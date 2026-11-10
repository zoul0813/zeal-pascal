# Pascal applications for a small machine

The nine self-contained applications below (ten programs) demonstrate the current native Zeal Pascal port.
Each application folder contains a standalone native `.bin`, its complete Pascal source,
run instructions and exact
input lines for a short demo. No shared example libraries, includes or compiler extensions are needed.

The [ZVB examples](zvb/README.md) include Snake, a sprite demo, a file CRC32
utility, and a sound/DMA/CRC hardware check. They require a Zeal Video Board;
see their guide for compiling with the stock Units, required asset includes,
controls and redistribution credits. The application instructions below apply
to the nine prebuilt applications in this table.

| Example | Application | Pascal strengths |
| --- | --- | --- |
| [Lighthouse](lighthouse/README.md) | Text adventure | Enumerations, sets, cached screen updates, color and CP437 graphics |
| [Parts Bin](partsbin/README.md) | Persistent stock database | Records, typed files, random access, recoverable I/O |
| [Loan](loan/README.md) | Financial calculation | Real6, Exp/Ln, cent rounding, formatted tables |
| [Calculator](calc/README.md) | Expression interpreter | Recursive descent, A- locals, Real arithmetic, error positions |
| [Queens](queens/README.md) | Puzzle solver | Recursive backtracking, sets, whole-array snapshots |
| [Measure Lab](measure/README.md) | Data collection and statistical report | Execute handoff, persistent Real files, streaming variance |
| [Concordance](concord/README.md) | Text processing | Text streams, strings, frequency tables |
| [Life](life/README.md) | Simulation | Two-dimensional arrays, simultaneous updates, wrapping grids |
| [Errands](tasks/README.md) | In-memory task manager | Typed pointers, linked records, heap allocation and reclamation |

Start with Lighthouse for a short story, or Life for an immediately visible
simulation. Parts Bin shows that the generated program can retain and update
real data across launches. Loan and Concordance do useful numerical and text
work. Errands makes allocation and reclamation visible. Calculator explains
where an expression goes wrong; Queens counts every solution to a classic puzzle.
Measure Lab shows two programs working as one application through persistent data.

Keep each example folder's files together. From the directory containing
`examples/`, change into the example you want to run:

```text
cd examples/life
./life.bin
```

Use the corresponding folder and executable name for the other applications.
The explicit `./` runs the executable in the current folder without depending
on the shell's search path.
No IDE or library installation is needed to run these executables. Each local
README also explains editing, Memory execution and rebuilding a COM.
The examples use the existing compiler and runtime. COM files run on native
Zeal OS and are not CP/M executables. Lighthouse uses the native 80x40 text console and TP3 screen routines; the other
examples use scrolling text output.
The sample input files are transcripts, not shell scripts.

Every disk file is explicitly closed before normal termination or Execute.
The sources enable bounds checks. Input is read as text before Val conversion,
and loops never rely on Boolean short circuiting. Capacities and persistence
limits are explained with each application. Back up generated data before
experimenting with disk updates.

For repository build, verification and executable-refresh instructions, see
[example maintenance](../tests/README.md#example-applications).
