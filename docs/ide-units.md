# IDE compiled units

> Historical checkpoint/proposal. Its limits, feature status and measurements
> describe that stage. Use [Pascal units and ZPU libraries](units.md) for current
> support, authoring instructions and resource limits.

The normal Zeal Pascal IDE can create and consume compiled `.ZPU` units.
Select **Options → U Unit-file**, return to the main menu, and press **C**.
The declared unit name determines the output: `MATHSRC.PAS` containing
`unit Calc;` produces `CALC.ZPU` beside the source. R explains that units must
be compiled and used by a program. Dirty Work is saved first through the
existing source-save/backup path. Work is then refreshed from saved text,
including the existing trailing-space trimming, so imported error offsets match
the displayed source. Missing or unreadable source leaves Work intact; a
successful refresh preserves its cursor, clamped to the saved length.

For example, compile this as a Unit-file:

```pascal
unit Calc;
interface
function Twice(X: Integer): Integer;
implementation
function Twice(X: Integer): Integer;
begin Twice := X * 2 end;
end.
```

Switch to Memory or Com-file mode to compile its client:

```pascal
uses Calc;
begin WriteLn(Twice(21)) end.
```

A client imports the unit with `uses Calc;`. Compiled code and signatures come from the `.ZPU` artifact. Clients can compile,
run and save programs after the unit source is removed. Editing a unit source
requires explicitly rebuilding its `.ZPU` to update client code.

Unit source paths, lengths and fingerprints remain optional debug metadata.
Runtime errors retain their code and PC when source is unavailable. Navigation
requires matching source text, including when an open editor buffer shares its
path. Missing or changed text leaves Work intact. Separate units may record the
same authoring path, and parser includes have their own source identities. Only reachable routine fragments enter the program. Saved `.COM`
programs contain their selected code and runtime and run after compiler files,
unit artifacts, and source files have been deleted.

After error navigation into a unit, Main still selects the client program.
Before rebuilding the edited unit, use **M Main**, submit an empty filename,
then select **Options → U Unit-file** and press **C**. Clear Main so the unit
source in Work becomes the compilation root.

## Memory and format

`.ZPL` remains the IDE/compiler bank container. `.ZPU` is the separately compiled
Pascal unit artifact. Neither is required by a saved executable.

The IDE still loads **10 code pages at startup**, compared with the earlier
13-page baseline (48 KiB saved). Its warm compiler cache uses 15 pages, one more
than the preceding optional-Real IDE. Logical bank 16 contains unit parsing and
linking; logical bank 17 handles unit artifact I/O and publication transiently.
The optional Real page and unit I/O page are released after their phases.
Startup and generated-program execution do not keep these pages resident.

The source-independent import checkpoint uses 238600 bytes in its bank container,
198 bytes below the preceding IDE checkpoint. Generated programs retain the
existing fixed 16 KiB runtime prefix. Omitting the Real engine changes included
bytes. Requirement-based execution-page allocation is described in
`program-pages.md`.

The unreleased format is ABI1 under the `ZPU1` magic. There are no compatibility
readers for earlier experimental layouts; the first release will establish the
ABI1 contract. A validated byte for each fragment records Real-engine and
independent-memory demand. Raw Real storage/copy alone does not request the
engine. Only referenced fragments propagate either requirement. Small programs
using bounded leaf routines can use two execution pages. Format, bounds, flag
and private runtime-fingerprint checks remain mandatory. Rebuild experimental
unit artifacts after compiler/runtime changes.
The allocation contract and measurements are in `program-pages.md`.

## Supported scope and failure behavior

Exports support the existing primitive scalar/untyped VAR subset, including
Real arguments and results. Opaque External declarations are rejected in units. Private helper routines and their reachable Real
requirements are linked transitively. Dependencies between units, initialization,
exported aggregate types/variables, and bodies requiring the existing generated
console/Text/CRT/Inline helpers remain outside this increment. Unsupported unit
features receive a diagnostic; this is not a complete TP4–TP7 unit system or a
complete recovered Turbo Pascal specification.

Compilation writes an owned `ROOT.$PU` scratch file. An unrelated pre-existing
scratch file is preserved and blocks the operation. Compilation errors and
cancellation before publication preserve an existing `.ZPU`; cleanup is retried
when removal fails. Publication uses the existing checked sequential-copy policy
for executable output. A failure during replacement can leave a partial target;
publication is not an atomic filesystem transaction.

Source-independent import qualification is recorded in
`sourcefree-units-evidence.json` and `../reviews/sourcefree-units-v1.md`.
The preceding IDE milestone is recorded in `ide-units-evidence.json` and
`../reviews/ide-units-v1.md`. Qualification uses the native emulator.

Run the registered checks through `tools/run-tests.py`; it builds selected native
prerequisites with ZDE before freezing artifacts and starting parallel readers.
For a standalone driver invocation, build its native target first with ZDE.
