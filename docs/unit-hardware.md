# Hardware access in Pascal units

Native Pascal units can use `Port`, `Mem`, private absolute variables and
`Inline` without a C adapter. The compiler records hardware access per code
fragment. Importing the unit does not initialize hardware, include unused
routines or implicitly retain the Real arithmetic engine.

```pascal
unit VideoMode;
interface
function ReadMode: Byte;
procedure SetMode(Mode: Byte);
implementation
function ReadMode: Byte;
begin ReadMode := Port[$9C] end;
procedure SetMode(Mode: Byte);
begin Port[$9C] := Mode end;
end.
```

Compile in Unit mode, then use the resulting ZPU from a program. The compiled
unit is needed only during compilation; saved programs keep their reachable
code. The public source is optional when importing a ZPU.

## Addresses and buffers

`Mem[Address]` accesses a byte at a16-bit CPU address. `Port[Address]` uses the
full16-bit Z80 I/O address; writes send its value's low byte. Integer and Word
addresses/values are accepted; LongWord raw addresses and port values fail
compilation rather than being mistaken for pointers to temporary storage.

`Word(PointerValue)` obtains a16-bit CPU address, and `Ptr(WordAddress)` produces
an untyped pointer. For a buffer passed by reference, use `Word(Addr(Buffer))`.
These are CPU addresses, not DMA physical addresses. A ZVB DMA address must
snapshot the current MMU mapping with `DMAVirtToPhys`.

Ordinary memory-copy/fill count operands accept Word. Heap/stack manipulation,
console/file services and callbacks still require separately represented unit
runtime support. Hardware access does not bypass those authoring checks.

## Inline and relocation

Inline operands naming private unit storage receive ordinary unit-data
relocations. This includes parameter workspace slots, so their addresses remain
correct when a client has its own globals. Typed-constant operands remain code
storage references. A `var` parameter's workspace slot contains its referent
pointer, consistent with existing Inline behavior.

A unit can use a complete16-bit storage address, or an address plus a constant
that remains within its storage fragment. Byte-sized storage-address fixups and
expressions combining code and data relocation bases are rejected. Plain
numeric opcodes and relative-branch displacements need no such fixup.

The compiler puts its guard before the complete Inline stream. It does not
insert a guard between the stream's machine instructions. If temporary mapping
would hide the OS, keep the entire borrow/restore sequence in one stream:
restore the mapping and original interrupt state before returning to Pascal.
The compiler does not interpret arbitrary machine instructions or infer hidden
calls to optional compiler-private runtime entry points. Express Real operations
in Pascal so their runtime dependency is recorded.

Ordinary program Inline keeps its existing conservative Real requirement.
Unit Inline records hardware access without implicitly requiring Real. Only
retained unit fragments contribute requirements to the final program; a program
using only a plain routine from a hardware unit can still use its shared stack.

Use one source file when authoring a unit. The sealed unit's source-map contract
currently names one source; the stock ZVB build combines its maintainer include
files into one Pascal source before native Unit-mode compilation.

## Verification and checkpoint status

The focused native test is `tools/test-zvb.py --hardware-only`, after building
`zzvbtest_bin` through ZDE. It exercises relocated Inline storage, private fixed
addresses, Word pointer round trips, byte memory access, I/O and Word-sized
copies. It also checks source-free saved execution, Real omission, selective
hardware requirements and continued rejection of unrepresented services.

The full Pascal ZVB authoring probe is a separate mode. Its initial run reached
the existing single-page catalog limit; larger-unit storage is the next stage.
The hardware checkpoint's required Opus review remains pending the CLI usage
limit. This document does not claim the full ZVB library is ready.

Checkpoint verification:159 native hardware assertions,105 native Inline
assertions,932 unsigned assertions,64 sanitized decoder assertions,24 runner
tests and the host regression pass. The Inline fixture had stale expectations
from before the deliberate lazy file-block initialization and source-map removal;
it now checks zero initial storage and standalone image bounds. Native assertions
still verify live included-source identity. See unit-hardware-evidence.json.
