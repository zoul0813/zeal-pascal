# Compact runtime experiment

This records the first proof snapshot. The [follow-on low-file experiment](low-file-experiment.md)
adds bounded typed/untyped file data access; the earlier results below remain
historical evidence for their hash-bound source/artifact snapshot.

This is an isolated experiment in `explore/pascal-units`, not a production
memory-layout change or a Pascal unit implementation. The user authorized the
compact-layout proof following the library exploration. Original CP/M binaries,
installed Zeal tools, and the normal IDE runtime are unchanged.

## Contract and invariants

The native compiler is reused without changing `GLOBAL_BASE`, image versions,
heap layout or the ordinary loader. Two proof-only compiler objects reject
transfers in the compact target; ordinary targets keep their Chain behavior. A dedicated `zcompacttest`
launcher substitutes a checked compact runtime template in its runtime bank.
The regular `zpascal`, `zptest` and math controls still use `pascal_runtime`.
The experimental compiler rejects Chain/Execute statements and CHN output with
COMPILE_PLATFORM_FAILURE before publication: its normal transfer assets have no
compact binding. Raw Inline/native code is outside this experiment's safety contract.
All Zeal artifacts are built with `zde cmake`.

The compact profile retains the same public gates and the same integer Val
parser in `real.c`, but excludes Real arithmetic/formatting/transcendentals.
Unsupported numeric operations return the existing internal FF failure rather
than fake a result. Compiler-side Real literal conversion remains available;
this experiment does not infer which program operations need Real and does not
select a runtime automatically.

The compact link places numeric code below `$5000`, input/formatting/keyboard
code at `$5000`, and private data at `$5800`. Packaging checks all initialized bytes and every nonempty relocatable linker
area below `$6000` before the template is embedded. `$6000–$7FFF` is a test-only
8-KiB data interval accessed through explicit Pascal absolute declarations.
It is not automatically available to ordinary globals or the heap. CRT helpers
continue to execute above `$8000` and restore page 1 before returning.

Each Pascal fixture is compiled by the native compiler running on the unchanged
Zeal emulator and executed both through IDE-style RAM execution and as a saved
COM. Existing harness checks cover machine state, page ownership, source identity,
and exact source error locations during RAM execution; saved error cases
check the exit status. The dedicated launcher and runtime constants
are checked for mutation across runs.

## Proof cases

- Empty program, to check ordinary startup/return.
- Integer Val into `$6000`, including -32768, hexadecimal -1, overflow/error
  positions, unchanged destination on failure and empty-input behavior.
- Fill and verify every byte of an 8192-byte low array across ClrScr, GotoXY,
  color and cursor queries. This exercises actual page-1 VRAM replacement.
- File BlockWrite/BlockRead using an ordinary high-memory buffer, with exact
  external file-byte verification.
- Physical keyboard integer ReadLn into a low variable, including signed bounds.
- Low-buffer BlockWrite must fail with the existing FF memory-authority error,
  including with I-; its output file must remain empty.
- Real arithmetic, output, Val, input, comparison, conversion, Trunc, Sqrt and Ln
  in the integer profile must fail at the correct source positions.
- Chain/Execute and CHN compilation must fail before producing a usable build.
- The normal full-runtime control executes Real programs separately.

A host differential test compares integer Val with the full engine for existing
edge cases plus decimal/hexadecimal samples. It also checks that every unsupported
numeric operation fails. Existing full Real host vectors guard the original path.
Host tests supplement native execution; they do not establish target acceptance.

## Why file buffers are a blocker

`src/file_runtime.c:readable` accepts static storage beginning at `$8000`, a
checked live stack span, or a validated heap allocation. A low buffer is rejected
before transfer. That is necessary under the current mapping model: the file
module replaces page 1 with its own code and data. Merely lowering that bound
would make a raw pointer refer to module storage instead of the caller's bytes.
File control blocks, paths and numeric destinations also need an ownership and
mapping audit before general globals can move into this interval.

A production compact layout therefore needs explicit low-memory transfer staging
or a mapping-aware access contract, together with symbolic placement and allocator
changes. Passing the low-array/CRT proof alone is insufficient to change defaults.
Chain runtime requirements and shared-global layout remain separate release gates.

## Reproduce and evidence

From the exploration worktree, with the existing ZDE available:

```sh
zde --no-tty cmake --target zcompacttest_bin zpascal_bin zptest_bin zmathtest_bin --parallel 6
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-compact.py
python3 tools/test-real.py --host
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-math.py --case sqrt-programs
```

When adding a new target to an already-configured build, run
`zde --no-tty cmake --target rebuild_cache` first. No additional application
installation or third-party source customization is required.

The runner writes `analysis/native-slice/<run>/compact-report.json`, raw console
journals and disposable media/output files. It binds source, build artifacts,
emulator and ROM to SHA-256 hashes. Final results and limitations are recorded
below after execution; no target result is inferred from a successful build.

## Result and next decision

The [hash-bound evidence](compact-layout-evidence.json) records **249 native
assertions passed, zero failed** for the compact probes and **37 passed, zero
failed** for the normal Real/sqrt control. Host verification passes 3959 integer
Val differential cases, 59758 full Real vectors, four adversarial allocation
checks and 63 normal packaging/no-publication checks. Compiler/runtime bank
captures exactly match the loaded library before and after the compact run.

| Observation | Result |
| --- | ---: |
| Normal runtime payload | 16191 bytes |
| Compact integer payload | 6297 bytes |
| Payload reduction | 9894 bytes |
| Explicit low interval verified bytewise | 8192 bytes at $6000–$7FFF |
| Saved-image prefix field | 16384 bytes |
| Saved-image globals base | $8000 |

The explicit interval size is the hashed fixture declaration, with every byte
verified after CRT access; it is not a measurement of an automatic allocator.
Automatic globals/heap gain and OS-page release remain zero by the unchanged
layout contract. Compiler peak pages and physical hardware were not measured.
The normal runtime SHA-256 matches the pre-experiment artifact byte-for-byte.

**Go** for further compact-layout design and reusable units; **do not change the
production memory layout yet**. Low ordinary storage and CRT coexist in this
proof, including native output and physical keyboard integer input. Low file
buffers correctly fail before transfer. Before using this space for general
globals, define mapping-aware transfers/staging, placement/allocator ownership,
and Chain compatibility. The experiment deliberately does not relax the file
validator: doing that alone would access the module's mapped memory.

The first execution exposed overlong probe source lines and an observer that
missed an already-halted supervisor. Both were corrected; the final run uses
valid source lines, authenticates either completion PC, requires the exact
assertion count and compares independent output/VRAM/file/bank evidence.

Two restricted Opus code-review passes found no milestone blockers. Final minor
hardening recommendations were implemented and retested after the second pass;
the exact final inputs were not re-reviewed, so the review runner milestone is
not finalized. See the [reconciled review](../reviews/compact-layout-v1.md).
