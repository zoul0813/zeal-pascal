# Public unit types

Unit interfaces can export `const` and `type` sections as well as routine
headings. `uses` is supported at the start of the interface and after
`implementation`. These are deliberate native Zeal extensions, with an
unreleased ABI1 container; they do not read Turbo Pascal TPU files.

Supported exported type metadata covers aliases, enumerations, ordinal
subranges, fixed strings, arrays, records (including variants), sets, pointers
and file types. The normal Pascal rules still govern routine results and value
or Var parameters. Public mutable variables, typed constants and initialization
sections remain unsupported. Implementation helpers that require unrepresented
runtime services are still subject to the existing unit-authoring limits.

For example:

```pascal
unit Shapes;
interface
const MaxPoints = 16;
type
  TPoint = record X, Y: Integer end;
  TPoints = array[1..MaxPoints] of TPoint;
procedure Translate(var P: TPoint; DX, DY: Integer);
implementation
procedure Translate(var P: TPoint; DX, DY: Integer);
begin
  P.X := P.X + DX;
  P.Y := P.Y + DY
end;
end.
```

Compile this in Unit mode to produce `SHAPES.ZPU`. A program or another unit
can then declare `uses Shapes;`, declare `P: TPoint`, use `MaxPoints`, and call
`Translate`. Qualification such as `Shapes.TPoint` is also supported. The PAS file
is optional when importing the compiled unit. Rebuild dependent ZPUs after
changing a dependency; manifests bind exact artifact checksums.

File-local type IDs are mapped to compiler type IDs. An imported alias records
its original unit/type identity, so passing a record through an alias in another
unit preserves Var compatibility. Distinct enums and records keep distinct
identities. Pointer cycles are valid; recursive values and inconsistent layouts
are rejected. Variant tags/member indices are rebased into the compiler's
metadata arena, rather than retaining the author's symbol numbers.

The loader checks all type envelopes before binding dependencies. It then
validates and caches complete type graphs, including unused private descriptors,
and checks every public signature/constant. This uses compile-time metadata
storage; it does not emit unused type tables or constant pools into programs.
Exported names acquire symbols when looked up. Cached lookups reuse the symbol
without reloading the metadata code bank.

Types and constants are compiled into the instructions/data that use them.
Saved programs have no runtime dependency on the ZPU or PAS files. Only reachable
routine fragments and their shared data enter the output. The existing one-page
ZPU artifact limit and physical compiler-memory limits still apply.

Type processing uses the existing transient metadata bank. It adds no startup
or warm-cache code page: those budgets remain ten and fifteen respectively.
The file compiler lends its idle page during graph checks, serialization and
final linking. The integrated suite passes 1,595 target assertions; Crt passes
502. The IDE suite passes 66 checks, including typed unit creation/import on HostFS, TF and
EEPROM, and the failure/cancellation/retry suite passes 51 checks. Independent
type-envelope/graph fixtures pass 39 assertions under ASAN and UBSAN. The
metadata-only record/Integer client retains two execution pages and omits the
Real engine despite the unit declaring a Real-containing variant record.

Evidence is in [unit-types-evidence.json](unit-types-evidence.json). Independent
Opus review remains pending because the account usage limit prevented review.
The first attempt returned API429 without an actual review. This qualification
does not claim a complete recovered TP4–TP7 specification.
