# Low-memory file data: native experiment

This continues the compact-prefix investigation in `explore/pascal-units` after
the user requested the next step. It is a native Zeal experiment, not general
unit support or a production allocator/layout change. The previous proof and
its limitations remain in `compact-layout-experiment.md`; its artifacts and
reports describe that earlier snapshot.

## Behavior and ownership

The existing file module executes in page1 (`$4000–$7FFF`) using its own C stack.
Replacing page1 again would hide its code and stack. The new module-local bridge
instead maps the caller's prefix RAM page into page2 for a bounded copy. Caller
low addresses `$6000–$7FFF` become `$A000–$BFFF` through that alias. The module's
code, request, existing128-byte staging buffer and stack remain accessible.

Maskable interrupts are disabled only during the copy, with page2 and the original IFF
restored before returning. NMI behavior during the alias is not established and is
outside this proof contract. There is no syscall, callback or retained mapped
pointer in that interval. The gate's saved caller-prefix page handle is the
source of mapping authority; only valid RAM page indices are accepted. The
native language's raw Mem/Port/Inline facilities remain outside this proof's
safety contract, as in the preceding experiment.

The bridge validates its six-byte internal request, count1–128, direction0/1,
module-local staging span, caller-low span, physical RAM index and retained
stack frame before copying. The C/assembly field offsets are compile-time
checked. It returns an explicit invalid-argument status without copying or
mapping on failure. File runtime authority failures keep the existing FF path.

Only typed/untyped **data** transfers opt into the reserved low interval.
A request must fit wholly in it, or wholly in an existing valid high-memory
span. Crossing `$8000` in one request is rejected. File control blocks, type
descriptors, standard-stream tables, numeric/Text contexts and raw module path
pointers keep their existing checks. The compiler already snapshots Assign's
String argument on its high stack, so an ordinary Pascal filename String can
reside low without a new path mechanism.

Successful SDK reads copy only the returned byte count to the caller. Errors,
oversized counts and zero-byte reads do not copy stale staging data. Short reads
preserve the existing whole-record `Actual` semantics. Zero-count block transfers
do not invoke the bridge. The existing128-byte buffer is reused; there is no new
staging-page allocation.

## Isolation and binding

`compact_file_runtime` is built separately with ZP_COMPACT_EXPERIMENT and the
bridge. `zcompacttest` replaces its file asset bank as well as the previous
compact template and fail-closed compiler objects. Its private profile and
packager bind the exact compact file module and compressed asset. The ordinary
file runtime uses the original direct-copy path and unchanged validation.
Chain/Execute/CHN remain rejected in the compact compiler. No compact transfer
coordinator ABI is introduced.

Normal runtime/file payload hashes are compared with the prior artifacts. The
compact file module must still pass the existing `$7C00` code/data bound, leaving
its reserved C stack. The compact prefix remains below `$6000` and is unchanged
by this work.

## Required proof

- Native bridge component: read/write, smallest and largest chunks, upper/lower
  bounds, invalid request/buffer/count/direction/page/stack, independent page-byte
  oracle, caller SP/MMU/IFF preservation, and page release.
- Native compiler and generated RAM/COM programs: full8192-byte round trip with
  adjacent canaries,300-byte typed records (non128-byte tail), byte at `$7FFF`,
  short EOF record, zero-count write, and filename String in low storage. A padded
  fixture places the live FIB and high canary inside the page2 alias offsets and
  checks both after multi-chunk reads/writes.
- Negative native programs: typed read/write and block read/write below-region
  and crossing-region buffers fail before transfer. RAM-only bytewise checks
  verify every rejected-read destination byte; RAM and saved COM both check the
  error result and location. Counts65 and511 exercise low-span rejection without u16 overflow; a low file control block still fails at the source operation.
- Host sanitized file model: normal behavior plus low spans, partial SDK reads,
  read/write failures, oversized read results, no stale destination mutation,
  zero-count copies and no leaked handles/pages. First/later bridge-copy failure
  injection verifies Actual, position, destination bytes, disk bytes and cleanup.
- Packaging: mixed compact raw/compressed modules, the wrong file variant, and
  stale linked file bytes fail without publishing a replacement BIN/ZPL pair.
- Existing normal native file suite and packaging rejection checks remain green.

## Reproduce

```sh
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-low-file.py
```

The gate records an incremental ZDE rebuild transcript and binds source hashes
to its resulting build artifacts before running acceptance, including a rebuilt
normal compiler for the packaging control. It is not a clean-build
guarantee. Each native runner creates disposable media and records
source/artifact/emulator/ROM hashes. Host reports bind fresh sanitized binaries,
exact assertion counts, input hashes and captured output; the gate retains the
normal packaging check transcript and its exact expected count. Raw manifests,
logs and media remain local under ignored `analysis/`; the retained evidence JSON
embeds test reports, command/log hashes and the local manifest reference. Running
the gate creates fresh captures; those local raw files are not an archival bundle. Tests run on the unchanged pinned emulator/OS through the existing SDK;
physical hardware and general allocator capacity are not established.

## Observed results

The [hash-bound evidence](low-file-evidence.json) records these passing checks:

| Proof | Passed | Failed |
| --- | ---: | ---: |
| Native MMU copy component | 583 | 0 |
| Native compiler + RAM/COM integration | 787 | 0 |
| Normal native file component | 1712 | 0 |
| Normal sanitized host file model | 1522 | 0 |
| Compact sanitized host file model | 12583 | 0 |

The integration also passes four compact-file binding rejections, four compact
allocation guards,3959 integer differential cases, exact disk bytes, four physical
input receipts, supervisor mapping restoration, and immutable-bank comparisons.
The existing normal packager passes63 rejection/no-publication checks.

Normal Pascal runtime and normal file-module bytes retain their preceding
SHA-256 values. The compact file module grows from13671 to14008 initialized
bytes (+337), with code/data ending at `$7907`, below the `$7C00` limit. It uses
the existing module page and128-byte buffer. No extra staging page is allocated.

The first integration run exposed an incorrect probe assumption: it expected
a low pathname to fail, but the compiler's existing String-value marshalling
already stages the pathname. The corrected positive Assign/Rewrite probe
passes. Low file-block aliases still fail as intended.

This removes the typed/untyped file-data blocker for the explicit8-KiB interval.
It does not make low storage automatic: placement/allocator ownership, low file
control blocks, other metadata/Text cases, runtime selection and Chain/Execute
compatibility still need specification and proof before a production layout
change. Units and binary library formats remain separate unimplemented work.
