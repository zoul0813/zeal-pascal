# Pascal applications for a small machine

Nine self-contained applications (ten programs) demonstrating the current native Zeal Pascal port.
Each folder contains a standalone native `.bin`, its complete Pascal source,
run instructions and exact
input lines for a short demo. No shared example libraries, includes or compiler extensions are needed.

| Example | Application | Pascal strengths |
| --- | --- | --- |
| [Lighthouse](lighthouse/README.md) | Text adventure | Enumerations, sets, command parsing, full-screen TP3 terminal dashboard |
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

Copy each entire folder to Zeal, change into it in ZShell, and run `./name.bin`
(for example `./life.bin`). The explicit `./` avoids depending on shell PATH.
No IDE or library installation is needed to run these executables. Each local
README also explains editing, Memory execution and rebuilding a COM.
The examples use the existing compiler and runtime. COM files run on native
Zeal OS and are not CP/M executables. Lighthouse uses the native 80x40 text console and TP3 screen routines; the other
examples use scrolling text output.
The sample input files are transcripts, not shell scripts. The entire examples
tree, including its ten `.bin` files and `binaries.json` provenance, is intended
for source control. Generated `.COM`/`.ZMP`/backup files and the working
`parts.dat` database are ignored.

For a direct emulator setup, set the emulator's HostFS root (`--hostfs`) to
this repository's `examples` directory. The folders then appear under `H:/`:

```text
cd H:/life
./life.bin
```

For a TF installation, copy the complete folders into `T:/examples`, then use
`cd T:/examples/life` and `./life.bin`. The project's `tools/package-tf.py`
installs the IDE and its library; it does not install these examples. Use TF or HostFS for the collection; the
compiler/runtime and ten executable images need more than the 64 KiB EEPROM
volume. Exact executable sizes are recorded in `binaries.json`.

Every disk file is explicitly closed before normal termination or Execute.
The sources enable bounds checks. Input is read as text before Val conversion,
and loops never rely on Boolean short circuiting. Capacities and persistence
limits are explained with each application. Back up generated data before
experimenting with disk updates.

From the repository, build and run the native acceptance check:

```sh
zde --no-tty cmake --target zexampletest_bin
python3 tools/test-examples.py
```

The check compiles these exact sources on the pinned, unmodified Zeal emulator,
runs their standalone `.bin` programs, and checks puzzle completion, database
persistence, zero-rate and ordinary loan schedules, word counts, Life evolution,
heap reclamation, recursive expressions and error recovery, every queen count,
and the Measure Lab handoff/return, EOF finalization and missing-program recovery. Its temporary executables, media, output captures and
hash-bound report stay under `analysis/native-slice/`. It does not install the
examples on a user's card or establish physical-hardware/full TP3 conformance.

To refresh the shipped executables after changing sources, run the same native
check with `--package`. Only after the behavior checks pass does it copy the
native-tested images into these folders and update `binaries.json`. A normal
run verifies that the shipped binaries and source hashes match fresh compiler
output. The `.bin` bytes are unchanged from the compiler's published native
COM format; the test launches the `.bin` filenames themselves.

The shipped executables record their compiler base revision, compiler-source
digest and runtime digest in `binaries.json`.
After changing the compiler or runtime, rebuild them all with `--package`
rather than mixing builds. The manifest records the compiler base revision and
a digest of its source contents alongside the source/binary hashes.
