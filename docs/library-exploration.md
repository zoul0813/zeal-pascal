# Pascal libraries on Zeal: investigation and proposed direction

> Historical checkpoint/proposal. Its limits, feature status and measurements
> describe that stage. Use [Pascal units and ZPU libraries](units.md) for current
> support, authoring instructions and resource limits.

Status: exploration, November 4, 2026. Branch `explore/pascal-units`, worktree
`/private/tmp/zeal-pascal-units`, source baseline
`4bf9aef63fb88d9c715d52ee6fdf44835e7dce68`. No compiler/runtime source changes,
new language support, or measured memory savings are claimed. This document
does not authorize implementation.

## Recommendation

Add native, separately compiled Pascal **units**, with selective static linking
shared by IDE Run and saved executables. Use this mechanism for user libraries
and optional runtime components. Retain a small resident System runtime. Keep
Real as an implicit language dependency; do not require `uses Real` merely to
use a Real variable, arithmetic, Read, Write, Str or Val.

This can remove unused functionality from programs. It does not let a program
execute library code that exists only at compile time. Referenced code and live
state must be present during execution. Banked libraries and disk overlays are
separate placement/loading mechanisms with separate costs.

There are two outcomes: reusable libraries and more memory for programs. The
library feature could succeed while adding no usable capacity. With the proposed
legacy default, units provide reuse and omit unreached unit code, but ordinary
programs keep the complete existing runtime and gain no reclaimed prefix capacity.
Any capacity gain requires the separate experimental compact layout. If that
succeeds, integer-only programs are the strongest candidate beneficiaries;
Real-heavy programs may save much less. ZVB should use static linking first,
with banking considered against a measured remaining limit.

| Item | At compilation/linking | In the executable and memory during execution |
| --- | --- | --- |
| Unit interface and `.ZPU` container | Read by the compiler | Not needed during execution; optional map metadata is separate |
| Unreached unit procedure | Available for selection | Omitted |
| Reached procedure/dependencies | Selected and relocated | Present |
| Nonempty imported initialization | A link root | Present and executed once |
| Real literal decoding | Compiler performs conversion | Value bytes only, unless other operations need the Real engine |

The decisive memory requirement is **reclaiming the address space**: the current
fixed runtime page and globals base must change before removing Real creates
more usable ordinary program memory. A unit parser alone achieves neither goal.

## What the TP4–TP7 manuals document

These are documented manual findings, not observations of executed Borland
compilers or recovered TPU internals. Download URLs and SHA-256 hashes of the
retrieved OCR texts are in [the evidence record](library-exploration-evidence.json).
Local OCR copies are under `analysis/libraries/`. Printed page references below
are preferable to OCR page/line numbers. OCR contains mistakes: the TP5 smart-link
example says “PI and P3” where its call graph clearly requires P2 and P3.

| Release/source | Relevant documented behavior |
| --- | --- |
| TP4 Owner's Manual (1987), Introduction “File Extensions”; “Units”; “Smart Linking,” p.372 | A compiled `.TPU` holds symbol information and compiled code. `unit`, `interface`, `implementation` and `uses` support separate compilation. The linker removes uncalled procedures/functions. `.TPL` groups units. |
| TP5 Reference Guide (1989), “Programs and Units,” pp.93–98; “Smart Linking,” pp.220–221 | Removes unused procedures/functions and unused data. Code granularity is a procedure; data granularity is a declaration section. Smart linking is disabled when compiling to memory. A unit may have executable initialization. |
| TP6 Programmer's Guide, “Programs and units,” pp.114–118; “Smart linking,” pp.258–259; “The Overlay unit,” chapter 13 | Retains the same unit and selective-link model, including the memory-mode exception. Overlays load code into a reusable buffer at runtime. |
| TP7 Language Guide (1992), “Programs and units,” chapter 10; “Smart linking,” pp.250–251; “Using overlays,” chapter 18 | Retains selective code/data linking and the memory-mode exception. Unit initialization and dependency rules are semantically significant. |
| TP7 User's Guide (1992), “The TPUMOVER utility,” p.104 | TPUMOVER adds/removes units in the compiler's standard unit collection. The collection can be loaded with the compiler; this is distinct from a shared library loaded by each executable. |

A `uses` clause imports a public interface. The linker includes referenced
implementation code and its transitive dependencies. Treating retained address
references as reachability edges is a necessary linker design inference here, not a separately
executed Borland observation. Initialization is executable behavior,
so an imported unit can contribute code even without an explicit routine call.
The manuals describe initialization in uses order, with indirect dependencies
initialized before their users. Zeal must specify the exact graph traversal.

TP5–TP7 data selection is coarser than “one variable”: referencing any variable
in a retained declaration section can retain its neighbors. Do not promise that
one referenced exported function necessarily means only that function's bytes.

TP4's cited paragraph establishes procedure elimination, not the later releases'
data-elimination rule. No TPU binary compatibility is proposed. These are DOS
compiler design precedents, not a replacement oracle for CP/M TP3 semantics.

The proposal deliberately changes Borland's Memory-mode behavior: both IDE Run
and saved output should use selective linking, so they execute the same linked
program behavior. This adds linker work and compiler workspace to Run, which must be measured rather than assumed free. We inherit
unit structure and fragment granularity; defer archives, banking and overlays;
and do not propose TPU binary compatibility or general finalization.

## Current ownership and behavior

Source conclusions below refer to the baseline commit. Existing build measurements
refer only to the hash-bound artifacts copied into `analysis/libraries/baseline`;
those artifacts were not rebuilt from that commit in this investigation.

* `CMakeLists.txt` links `real.c` and `transcendental.c`, adapters, formatting,
  input and keyboard support into `pascal_runtime`. Its section bases are fixed.
  `real_compiler` is already a separate, smaller compiler-only conversion build.
* `compiler_asset.c` reads the immutable runtime template from the verified
  compiler library bank. `compiler_compile.c` copies it into every ordinary
  Memory image, or writes it plus zero padding as a full 16-KiB disk prefix.
  CHN deliberately borrows its caller's runtime instead.
* `zp.h` fixes globals at `$8000`; `compiler.c:compile_limits` and
  `compiler_build.c` place globals/code above that base. `native_image.c` checks
  that base and the 16-KiB prefix in saved-image headers. Deleting bytes from
  the prefix without changing placement leaves an unusable hole.
* `compiler_real.c` emits calls to the fixed `RT_REAL` entry. `real_adapter.asm`
  bridges arithmetic, Real formatting **and integer Val** into `zr_eval`.
  `format.asm`, `input.asm` and `compiler_files.c` also reach that shared path.
  An integer-only program must not acquire the full Real engine just for input.
* Screen/CRT support is already demand-generated in `compiler_crt.c`, cached
  per operation. It is not an unconditional whole-screen package. No `Screen`
  Pascal unit currently exists. Moving these names into a unit changes namespace
  policy more than it immediately saves memory.
* File/transfer modules, deferred helpers and the unpacker already have optional
  acquisition/emission patterns. Extend fitting ownership rather than add another
  parallel special-purpose module loader.
* The current `.zpl` IDE/compiler container is not a Pascal user-unit format.
  Reusing its name/loader would confuse compiler deployment with program linkage.
* `compiler_members.c`, `compiler_groups.c` and `compiler_late.c` preserve
  spooled overlay members and deferred references. They offer relevant patterns,
  but their current scalar final-placement delta is not a general unit linker.
  Integrated overlays remain deferred; production readers reject overlay fields.

### Size evidence and limits

| Existing build measurement | Bytes | Interpretation |
| --- | ---: | --- |
| Copied runtime payload | 16,191 | Inside a reserved 16,384-byte prefix |
| `_REAL` code section | 10,819 | Major extraction candidate; includes shared integer conversion paths |
| `_REAL_ADAPTER` | 279 | Bridges several operations, including integer Val |
| `_REAL_DATA` | 345 | Mixed arithmetic, input and formatting state; not wholly removable |

The 10,819-byte figure is about 10.6 KiB, **not a demonstrated saving**. Exact
savings require splitting dependencies, linking real programs and measuring
placement, bridges, alignment and retained workspace. Do not add all three rows
and advertise that total as free memory.

```text
Today:          $4000 [ runtime + padding       ] $8000 [ globals + code ... ]
Dependency proof:$4000 [ core | unused hole      ] $8000 [ globals + code ... ]
Layout goal:    $4000 [ core | usable storage   ] $8000 [ safe code/data ... ]
```

The desired storage is in page 1 (`$4000–$7FFF`), the same window temporarily
replaced by CRT and file operations. Under the fixed layout, deleting prefix
code alone changes neither globals/code addresses nor saved-file length.

Four different budgets must be reported: program logical address space, physical
pages during execution, compiler peak pages, and file size. Zeal OS maps three
execution pages for ordinary binaries. Freeing logical space inside that mapping does not
by itself release an OS page. Banked code can increase total page use while
freeing logical space; overlays can trade disk I/O for bounded resident storage.

## Proposed native unit model

This is a design recommendation requiring a native extension specification,
not a claim that these constructs compile today.

### Authoring workflow and language

Proposed first workflow: save `TILEMATH.PAS`, compile that Work file as a unit in
the IDE to `TILEMATH.ZPU`, then import it from a program. Compile dependencies
manually in dependency order. After editing a unit, recompile it. If its public interface changed,
recompile dependent units in dependency order, then compile or run the program
to relink it. A missing unit, changed available source fingerprint, mismatched imported
interface, or incompatible option/ABI fingerprint stops Run/link with a diagnostic;
there is no automatic Make in the first version. Without source, a valid compiled
unit may be used, but the compiler cannot detect edits to unavailable source.
The cost of available-source checks on each Run must be measured on Zeal.
A unit compile error navigates to its source; linked runtime errors use the unit's
identity and source map, or report a unit/routine/offset when source cannot be verified.

```pascal
unit TileMath;
interface
function TileOffset(X, Y: Integer): Integer;
implementation
function TileOffset(X, Y: Integer): Integer;
begin
  TileOffset := Y * 80 + X
end;
end.
```

```pascal
program TileDemo;
uses TileMath;
begin
  WriteLn(TileMath.TileOffset(2, 3))
end.
```

Qualification is optional for an unambiguous imported export; ambiguous imports
require qualification rather than a silent winner. Existing local-scope rules
still apply. Interface variables/types/constants and implementation-only uses
are part of the proposed unit model. Recognize the new words contextually in
module/import positions, preserving otherwise valid TP3 identifiers; verify this
with source fixtures. Do not globally reserve them as a shortcut.

Exported headings must match implementations; implementation-only declarations stay
private. The IDE/compiler on Zeal must compile both units and final programs;
a host-only library packer does not satisfy the feature.

### Compiled format

Use one compiled-unit file per unit initially, with a distinct native extension
such as `.ZPU` (provisional). An archive like `.TPL` can follow if measured lookup
cost warrants it; it is not necessary to create reusable libraries.

The format needs an explicit version and ABI identity; case-insensitive unit
identity; imported interface identities; serialized exported types/constants and
routine signatures; separately selectable code/data fragments; symbolic
references and relocation records; initialization entry; and source-map identity.
Serialize fixed-width fields, never compiler C structs or raw internal TypeIds.
Identity of exported record/pointer types must survive imports and aliasing.
Mutable typed constants must retain their existing writable semantics.

### Linking

Link roots are the main body, initialization of imported units, required System
services, and retained address-taken/callback/interrupt/native-entry references.
Compute transitive closure over code and data references, then lay out and
relocate only retained fragments. Do not root every exported routine merely
because its interface was imported. Unreachable recursive cycles stay omitted;
reachable recursive routine cycles work. Opaque Inline/native code needs explicit references or
conservative retention of its declared domain; byte scanning cannot prove reachability.

Use procedure fragments with associated literals/nested code and declaration
sections for data as the first granularity. Keep self-contained constants with
their owner. Encode references to code, data, runtime services and maps explicitly.
The existing relocation coefficient times one delta is sufficient for moving an
entire image together, but not for independently placing unit fragments or globals.
Extend that responsible relocation layer with target-symbol plus addend semantics;
keep final image relocation distinct from unit-internal identifiers.

### Initialization

Define dependency-first initialization exactly once, honoring left-to-right uses
order when dependencies permit it. Empty initialization contributes no executable
work. Reject unit dependency cycles, including implementation-only uses, in the first version with a useful path diagnostic;
this is a deliberate simpler Zeal restriction, not full TP7 cycle compatibility.
Retained initialization may allocate state and pull in dependencies. Avoid automatic
video mode changes or graphics-buffer allocation merely for importing ZVB.
No new general finalization mechanism is needed for the initial unit feature.

### Lookup and failures

Search the importing source's directory, then explicit configured unit directories
in a documented order. Identify the matching unit, validate imported interface
identities and ABI, and reject stale/malformed objects before emission. Preserve
normal filename/path case rules separately from Pascal identifier case folding.
Map a unit to its explicitly compiled source/output basename; do not silently
truncate Pascal names to eight characters or assume DOS filename rules. Carry
unit identity independently of basename and enforce the supported Zeal filesystem
and existing path limits. Exact filename case/alias behavior needs platform proof.
A changed public interface requires dependent recompilation; implementation-only
changes still require relinking. Source lookup and compiler-option fingerprinting must be specified for the
first manual workflow. Automatic rebuild policy must be specified before
exposing an IDE Make command.

Do not require writeable scratch files to link a small self-contained Memory
program. Stream compiled-unit records from disk in bounded buffers; use owned far
pages for tables. Reuse disk-backed emission/spooling for larger builds. Allocation,
EOF, invalid counts/offsets/targets, cancellation, partial writes and resource
cleanup must follow existing error/ownership rules. Refuse overflow before patching.
Protect an existing valid unit output from a failed replacement within the OS's
actual guarantees; do not invent atomic rename/power-loss guarantees.

## Making runtime memory smaller

Split the minimal resident prefix from optional implementation code. Keep fixed
public service gates where compatibility requires them, but resolve each gate to
a retained implementation. Integer Val/input must have an independently selectable
path. Real literals may still be decoded by the compiler's existing conversion
engine without retaining a program-side arithmetic engine. A plain Real variable
or byte copy alone does not imply that all transcendental routines are needed.

Retain ordinary TP3 predefined names. `System` is implicit; Screen/CRT and Real
would be organized internally without forcing existing programs to add uses clauses.
The first version adds no new `Screen` unit.
User-authored units use explicit imports. New ZVB APIs are explicit unit exports.

For a measurable increase in ordinary capacity, replace the fixed `$8000` globals
base with a checked per-image layout after the compact resident prefix. Preserve
absolute Pascal addresses as actual native addresses; do not silently remap an
explicit `absolute` declaration. Apply symbolic placement to globals and code,
revise image/header/loader validation together, and version the ABI if required.

This needs a dedicated placement proof. CRT helpers currently execute at or above
`$8000` so they can temporarily replace page 1 with VRAM. Keep any such helper
outside the window it maps. The file/transfer gateways also replace page 1;
audit all code, global buffers, retained pointers and callback lifetimes before
placing them there. A file caller's buffer cannot disappear underneath a transfer.
Account for stack-in-page-3, syscall scratch, RAM return gates, heap layout,
Chain shared globals, Execute and native Call contracts. Do not simply lower
`GLOBAL_BASE` or move the whole generated program below `$8000`.

Keep the current layout available until compact-layout invariants are proven.
An interim selective-link implementation can demonstrate omitted code, but must
not claim extra user capacity while its fixed prefix remains reserved. If safe
low-page data placement proves too complex, compare a compact core plus banked
optional code explicitly; that has different memory and call costs.

### Chain compatibility is a release gate

CHN files borrow their caller's runtime and share declared globals. A selectively
linked compact parent cannot assume a future CHN needs only its own services,
and a different globals base breaks shared storage. Initial recommendation:
Chain-capable parents and CHN builds retain the existing fixed layout and complete
runtime contract; reject compact/legacy-chain combinations. Whether this rejection is at compile
time, at the runtime handoff, or both needs an enforceable artifact contract. Detecting
or declaring Chain capability, including opaque native transfer paths, must be
specified. Do not silently switch layout after compilation or infer child needs.
A later chain-requirements ABI or self-contained child model is a separate change.
Imported-unit storage and initialization across a parent/CHN pair remain
unspecified: selected unit globals could shift shared storage, and initialization
cannot be assumed once across independently linked images. Until a shared layout
and activation rule is specified, do not claim units and legacy Chain compose.
Test the chosen capability enforcement; until proven, compact layout must remain
experimental and unavailable as a general default.

## ZVB library and banked execution

A ZVB unit can expose palette, tilemap, sprite, tileset and synchronization APIs.
Use the existing Zeal VideoBoard SDK as a hardware/API reference (local commit in
the evidence record), with Pascal/native ABI adapters as necessary. Its C entry
points are not automatically callable through Pascal's calling convention.
Large tilesets and palettes are assets with explicit ownership/loading, not
unconditional static arrays bundled into every graphics program.

Recommend shipping ZVB as a bundled, separately compiled optional Pascal unit
whose source also demonstrates the ordinary user authoring path. This is a
packaging recommendation, not a commitment to its entire API. First-version
native access uses existing Port/Mem/Inline facilities behind Pascal procedures;
carry symbolic Inline references through unit relocation. Do not promise arbitrary
SDCC objects or host-built C library import. If those facilities cannot implement
the required hardware boundary safely, stop that proof and specify the missing
native adapter contract explicitly.

Author ZVB as an ordinary Pascal unit with a small documented native boundary
where hardware access requires it. Validate ranges, mode, MMU restoration and
interrupt state. Keep code executing outside any mapped VRAM window, and make
context/buffer lifetimes explicit. Prefer caller-owned contexts and explicit
Init/Done or equivalent mode restoration; define IDE restoration after errors.
Any lower-level cleanup support needed must fit existing lifecycle ownership.

Static smart linking omits unused graphics operations, but retained ones execute
from program memory. A later banked-unit mode could put retained routines in
owned physical pages behind a resident trampoline. That requires a real ABI for
nested calls, callbacks, recursion, error exits, interrupts, mapping restoration
and pointer lifetimes. Raw procedure/data pointers cannot transparently cross a
replaced window. A disk overlay additionally needs availability and reload/error
semantics. Neither mechanism should be smuggled into the first unit format as
an untested promise, nor should ordinary static programs require library files
during execution after successful linking.

## Recommended decisions and stop conditions

If implementation is authorized, pursue reusable units independently of
compact-layout success. Keep compact layout experimental unless an integer
program demonstrates a positive net increase in allocatable ordinary memory,
after all gates, alignment and workspace costs, all page-1 safety cases pass,
and Chain capability enforcement is proven. These conditions are necessary,
not sufficient: wider availability still needs a measured owner decision. Report the measured increase before choosing a
minimum product threshold; this investigation supplies no defensible numeric one.

Keep existing image readers for old version-2 artifacts; use an explicit new ABI
for compact output. Proposed initial build selection is legacy by default, with
an explicit experimental compact option; long-term coexistence/default policy is
open. Broader TP4–TP7 source compatibility, including `uses Crt, Dos`, is not
promised: document supported native unit names and fail unknown imports normally.
Do not change requirements or defaults on the strength of this proposal.
Reassess unit reuse if compilation/linking cannot fit Zeal's compiler/Run limits
or if TP3 regression fixtures fail; do not trade away existing correctness.

## Possible implementation sequence and acceptance gates

1. Specify the native extension: grammar/export/type identity, initialization,
   lookup and diagnostics, artifact/ABI rules, placement and source maps. Keep
   TP3 recovery status separate. Confirm the proposed version-2 artifact support boundary.
2. Use hand-built target fixtures to prove compact placement and split Real
   dependencies before relying on the not-yet-built unit linker.
   Compare empty, integer I/O/Val, Real storage/copy, Real arithmetic, Real
   formatting and transcendental programs. Validate Memory and disk behavior,
   low-page hazards and old-layout regression behavior before any wider compact availability. A failed compact proof
   stops capacity work, not unit reuse.
3. Add unit compilation/import and fragment linkage in the compiler. Share the
   same reachability/layout path between IDE Run and saved programs. Demonstrate
   two independently compiled user units before converting bundled facilities.
4. If the capacity proof succeeds, integrate System/Real extraction. Retain
   existing demand-generated CRT behavior in either layout.
   Adopt common linkage wherever it replaces special cases cleanly; do not
   require a premature rewrite of already optional modules.
5. Build a ZVB unit using that public authoring path. Measure bytes and pages;
   consider banking/overlays only against a demonstrated remaining limit.

Proof must cover: unreached procedures and unreferenced declaration sections absent; transitive references retained;
mutual recursion; nested routines/static links; address-taken callbacks; unit
initialization once/in order; duplicate/private/qualified names; interface type
identity; mutable constants; stale dependencies; corrupt/truncated artifacts;
allocation/I/O/cancellation cleanup; direct Run versus saved output equivalence;
and exact source location inside a unit for compile and runtime failures.
If unit source is absent or mismatched, report unit/routine/offset without
selecting unrelated editor text. Map publication must bind the linked unit build.

Measure code/data/heap capacity, actual execution and compiler peak pages, file
size and any extra call cost. Exercise Screen/CRT, file operations, Chain/Execute,
callbacks and errors with the altered layout. Build target proofs with `zde cmake`
and use the existing emulator/native harness. A host model alone is insufficient.

## Readiness

The manual/source investigation supports selective static units as the design
direction, and identifies the fixed-prefix blocker. Unit support, compact images,
runtime extraction, Chain capability enforcement and target savings remain unproven. TPU internals were not
examined and their recovery is not proposed.
No third-party application/source customization is needed for this proposal.
