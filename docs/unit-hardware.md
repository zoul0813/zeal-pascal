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

The larger-unit path uses compiler-owned chained buffers for catalogs and
metadata. ZPU files may contain up to 65,470 bytes, including code and initial
globals. Unit code may end at address 65,534 while being authored; executable
placement retains its existing `$F400` limit.

Import scans and validates the complete file but retains only metadata in RAM.
The code stays on disk. Imports close the file after validation. Linking reopens
it for each
needed file page, streams bounded selected ranges into the unpublished staging
image and checks the complete page hash before relocations can use that data.
A failed check rejects and discards the build. No separate file-window page is
allocated. Re-reading a changed or truncated file fails
compilation. A saved executable has no dependency on that handle or file.
For the current 60,430-byte ZVB unit, the resident metadata prefix is 28,846
bytes: two pages instead of four for the complete file, with no extra file-window
page. These are compilation allocations, separate from program RAM.
The compiler startup policy remains 10 cold / 15 warm pages and 18 logical banks.

`tools/test-unit-storage.py` independently generates page-crossing units,
exports string/Real/LongWord constants, exercises relocated code above `$F400`,
checks exact limits, deletes libraries before saved execution and injects
allocation failure and cancellation. Both failure cases require a clean cache
and a successful fresh compilation. Host reader tests exercise file mutation,
truncation, partial reads, seek/read errors, cancellation and close failures.
Zeal VFS removes a descriptor even when close reports an error; the owner
reports that error and clears the handle, allowing a fresh compilation.

Current verification: 336 focused native hardware checks; 286 native storage
boundary checks; 57 allocation-failure and 54 cancellation checks; 1,188 unsigned,
1,703 unit/Real and 623 initialization checks. Sanitized host checks include
80 metadata-decoder, 1,824 reference-merge, 65,980 reader and 45 loader checks.
The loader host suite isolates transport/allocation; semantic validation is
covered separately. Runtime Move/FillChar passes 229,402 Word-count checks.
Observed native stack watermark margins are at least 8,453 bytes for these
storage and ZVB clients; this is a lower-bound observation, not a proof for all
possible programs. Both Opus turns have completed. Later fixes pass local checks but require an
additional requested review before approval; full ZVB demo and physical-board
qualification are separate milestones. See [storage evidence](unit-storage-evidence.json).
