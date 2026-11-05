# IDE compiled units

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

A client imports the unit with `uses Calc;`. The current implementation requires
the matching unit source at compilation time for source identity and error
locations. Only reachable routine fragments enter the program. Saved `.COM`
programs contain their selected code and runtime and run after compiler files,
unit artifacts, and source files have been deleted.

## Memory and format

`.ZPL` remains the IDE/compiler bank container. `.ZPU` is the separately compiled
Pascal unit artifact. Neither is required by a saved executable.

The IDE still loads **10 code pages at startup**, compared with the earlier
13-page baseline (48 KiB saved). Its warm compiler cache uses15 pages, one more
than the preceding optional-Real IDE. Logical bank16 contains unit parsing and
linking; logical bank17 handles unit artifact I/O and publication transiently.
The optional Real page and unit I/O page are released after their phases.
Startup and generated-program execution do not keep these pages resident.

The combined IDE bank container grows to238798 bytes on disk; this integration
does not claim a smaller `.ZPL`. Generated programs retain the existing fixed
16 KiB runtime prefix. Omitting the Real engine changes included bytes, but does
not yet reduce generated-program execution-page allocation.

New artifacts use ABI2 under the `ZPU1` magic. A validated requirement byte for
each fragment records Real-engine demand. Raw Real storage/copy alone does not
request the engine. Referenced fragments with arithmetic, conversion, or other
Real operations request it before runtime finalization. ABI1 remains readable;
selected ABI1 fragments conservatively request Real because their private
implementation requirements were not recorded.

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

Qualification and independent review are recorded in `ide-units-evidence.json`
and `../reviews/ide-units-v1.md`. Emulator evidence is not physical hardware
qualification.

Run the registered checks through `tools/run-tests.py`; it builds selected native
prerequisites with ZDE before freezing artifacts and starting parallel readers.
For a standalone driver invocation, build its native target first with ZDE.
