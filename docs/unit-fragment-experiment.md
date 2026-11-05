# Native symbolic fragment linking

This is the next isolated library foundation in `explore/pascal-units`, following
`library-exploration.md` and the low-file proof. It is deliberate Zeal extension
work, separate from recovered Turbo Pascal 3 behavior.

The current compiler's relocation coefficient moves one whole image by one
delta. It cannot independently place separately compiled routines. The new
`compiler_fragments.c` owns a private native fragment linker with explicit
reference targets and reachability. It is built only into `zfragmenttest`.
Neither the normal compiler nor the compact compiler uses it yet.

## Contract

A source object occupies one immutable caller-owned compiler page. Its private
`ZPF1` record stream contains an object identity, fragment extents, word patches
and payload. Each patch names an object identity, fragment and byte offset.
All serialized fields are explicit little-endian integers; no host/native
structure layouts or internal TypeIds enter the stream. Extents are canonical,
complete and nonoverlapping. Word patches are ordered and disjoint. Referenced
objects, fragments and actual target bytes must exist, even in unused fragments.
All objects are validated before output allocation or copying.

This is **not the public `.ZPU` format**. Its numeric catalog identities, one-page
source objects, word-only references, at most32 objects and4096-fragment far
catalog bound the current proof. Public unit names, interfaces/type identities,
ABI/option fingerprints, source identity and maps, dependency initialization and
publication transactions still require the unit contract and compiler path.

Explicit roots retain their transitive references. Reachable recursive cycles
work; unreachable cycles remain omitted. A worklist in the far catalog avoids
recursive traversal and repeated processing. Each selected fragment gets its
own final address, in object/fragment order, within `$8000..limit <= $F400`.
References are patched from those independently placed addresses, including
words crossing a physical-page boundary. Code and data use the same reference
mechanism. A source library is never used as execution backing.

One owned compiler page holds the temporary catalog. Output pages are allocated
only for physical pages touched by the selected span, with leading/trailing
bytes in allocated pages cleared. An upper-page-only span leaves the lower
slot unallocated.
The workspace is released before a valid result is published. Caller-owned
source pages are checked against their role, lifetime and version throughout
linking. Source pages and their bytes remain the caller's property.
Cancellation, malformed objects, stale ownership, unresolved references and
capacity/allocation failures leave no valid image. Failed page releases retain
handles in the invalid result for a later close retry; an owner cannot release
a replacement page with a different lifetime. The fixture uses production
far-memory assembly and OS `palloc`/`pfree`, but
test-owned `cc_poll` and `page_*` implementations. Integration with the
production poll and page ledger is not established. After any failed link other
than an argument rejection, the caller must close the invalid result and retry
close if ownership handles remain. Argument rejection can preserve an existing
valid image.

## Proof and page accounting

The hand-authored fixture creates two library objects using native code, writes
them with the real Zeal SDK, frees their first backing pages, and reloads them
from disk. These are linker fixtures, **not separately compiled Pascal units**.
A root in object A calls object B, which loads a data fragment in A and returns
41. The root increments it and returns42. Two unused mutually recursive fragments
in A and an unused recursive fragment in B demonstrate omission and retention.

| Fixture | All roots | Selected root |
| --- | ---: | ---: |
| Available payload bytes | 18014 | 18014 |
| Retained payload bytes | 18014 | 11 |
| Retained fragments | 6 | 3 |
| Output pages, origin `$8000` | 2 | 1 |
| Linker ledger peak pages (2 source, 1 workspace, output; excludes own process pages) | 5 | 4 |
| Output pages after source pages are released | 2 | 1 |

The one-page reduction applies **only to this fragment output fixture**. The
normal fixed runtime prefix, program layout and allocator are unchanged. No
whole-program RAM-page reduction is claimed. The earlier compact proof still
makes8 KiB available only for explicit placement and releases no whole OS pages.

Target execution checks the generated42 result at two single-page origins and
again after overwriting and freeing the source-library pages. The call wrapper
must restore MMU, SP and IFF state; this does not establish MMU discipline for
arbitrary generated routines. The host and native cases also check relocation
bytes, a physical-page
crossing, zero padding, roots, malformed unreachable records, identity/extent/
patch/target bounds, stale source ownership, allocation failures, cancellation
at every poll in a successful small link, and retryable workspace/output release
failures. Format failures assert exact statuses and zero allocation calls.
Native cases cover adjacent/overlapping/duplicate/descending patches, nonzero
and last-byte target offsets, boundary page counts, a cycle reached by a root
and through another fragment, 4096 catalog entries with the highest entry
selected, rejection at4097, and a full16-KiB source object. Header/count wrap
cases exercise width-sensitive bounds. The32-source-object case is host-only:
all32 modeled RAM slots hold real objects, header validation reaches bases[32],
and workspace allocation correctly fails. Hardware cannot hold32 source pages
plus its running process and workspace; no successful32-object link is claimed.
An independent Python author checks the exact two library files against both
the host and native writer/reader path. Host checks use ASan and UBSan; target
checks use the production far-memory assembly with a test ownership ledger and
the unchanged emulator/OS. Worker/IDE ownership integration is not established.

## Observed acceptance

The retained [evidence](unit-fragment-evidence.json) records831 sanitized host
assertions and770 native assertions, all passing. The native runner authenticates
completion and the loaded program, checks both disk objects against an independent
Python author, and verifies source/build/emulator/ROM hashes stay unchanged.
The final gate is `analysis/fragments/20261105T040838.905303Z-71263/report.json`.

Target testing exposed a width-sensitive SDCC4.4 constant fold in the original
rounded page-count expression. The implementation now uses the actual bounded
memory model: allocate only the lower/upper slots touched by the validated
span. Exact end `$C000`, end `$C001` and origin `$C000` cases pin that rule. A
separate test observer fix puts before/after machine snapshots in identical assembly frames,
so C register spills cannot masquerade as an SP change. Generated42 results pass
for single-page images at origins `$8000` and `$8100`.
Relocation bytes, exact upper bounds and physical-page crossings are verified
by reading output pages through far-memory operations. Two-page images and
page-crossing code are not executed by this fixture.

## Reproduce and limits

```sh
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-fragments.py
```

The runner records an incremental ZDE rebuild, fresh sanitized host compilation,
source/output hashes, exact assertion counts, disk goldens and pinned emulator/
ROM hashes in `analysis/fragments/`. Retained results are copied into
`unit-fragment-evidence.json`; raw logs/media remain local and ignored. This is
incremental build provenance, not clean-toolchain reproducibility or hardware
verification.

Pascal `unit/interface/implementation/uses`, separate unit compilation/import,
exported types and routine signatures, symbol-qualified relocations from the
Pascal emitter (including symbolic Inline expressions), automatic initialization,
unit error locations, native authoring, and System/Real extraction remain
unimplemented. The private fixtures cannot be passed to the existing Pascal
compiler. This foundation must not be advertised as user-facing library support.
