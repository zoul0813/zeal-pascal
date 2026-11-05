# Optional native Real component

The separate Pascal-units worktree builds `zpascal` with a sparse integer core
and a disk-backed native Real component. This is deliberate Zeal behavior under
the approved library exploration. The full-runtime IDE is retained as
`zlegacypascal` for independent byte comparisons.

## Memory and files

The native prototype measured ten allocated code pages at startup and during
Run, compared with eleven in the preceding stage and thirteen before startup
optimization. The additional saving is 16384 bytes. The warm compiler cache
retains fourteen code pages. A Real compile temporarily loads one additional
component page and releases it immediately after decoding.

The production build currently packages a 201725-byte `.ZPL` and a 15275-byte
resident `.bin`. The independently rebuilt full-runtime control has a
202383-byte `.ZPL` and a 15285-byte resident image. The new `.ZPL` is 658 bytes
smaller than that control and 634 bytes smaller than committed checkpoint
`4880b4c`. Static data ends at 52367, leaving 13169 bytes of C stack reserve.
Qualification covers the production IDE and a clean ZDE build. Review status is
recorded in [the reconciled review](../reviews/optional-real-v1.md).

Generated programs reserve the existing 16 KiB runtime prefix and start globals
at `$8000`. This stage saves compiler/IDE RAM. Generated execution-page counts
and program extents retain that fixed layout. Integer programs leave the absent
Real engine region empty. Real selection restores the entire engine as one
component. Selection operates at whole-engine granularity.

## Selection and ownership

The integer and full templates have identical private helper addresses and
copied callback gate instructions. The packer verifies these invariants and
proves that the compressed patch reconstructs the full template exactly.
The integer head and tail occupy spare capacity in existing compiler banks.
The Real patch occupies the former runtime bank and stays on disk until needed.

Arithmetic, comparisons, conversions, formatting, Val Real, console Read Real
and text-file Read Real request the Real engine. Literal initialization and raw
Real storage/copies use the integer core. Inline, external declarations, hook
access and Chain/Execute conservatively request the full engine. A CHN contains
no runtime prefix and uses its parent's engine.

Parsing records demand. Finalization applies validated code-only patches before
installing keyboard, hook and error-handler bindings. Existing RAM/disk output
backends own every write. The last4 KiB of the already allocated Real data page hold the LZ history.
Back-references read that history so disk output stays sequential. TF and EEPROM
profiles observe92 scratch-sector flush calls for11310 decoded bytes, with saved
output identical to the full-runtime control. Profiling timings include debugger
overhead and are not hardware latency measurements.

The parser bounds compressed input, destinations,
back-references and cancellation intervals. A partial failure invalidates Build.
Source ownership survives failure, and the deferred loader supports retry.

Screen/CRT helpers already emit only the operations used by a program. Native
Screen regression coverage therefore verifies the existing selective behavior.
User-authored compiled Pascal units use `.ZPU`, with `.PAS` sources. The internal
native Real component is packaged inside the compiler `.ZPL`. Saved programs
embed their required engine and helpers. Raw Mem/absolute reads observe the
selected native template, including its empty Real-engine region in integer
programs.

## Review checkpoint

Two read-only Opus turns accepted the original dispositions and identified a disk
cache performance issue in the follow-up. The sequential decoder fix and stronger
acceptance observers were then implemented and tested. The user requested an
additional restricted review on2026-11-05. Its findings, reconciliation and final
status are recorded in [the review decision record](../reviews/optional-real-v1.md).

## Qualification

The evidence index records native selection, RAM Run, saved output comparisons,
compiler/library-free execution, malformed input, allocator exhaustion and
publication fault checks. The reviewed17-case matrix and standalone/CHN evidence
precede the common decoder history fix. After that fix, native Integer/Real byte
comparison and standalone execution, the full corpus, failure/retry, cancellation,
startup and media checks were repeated. The production-selector corpus passes1982 assertions
across15 families. Native allocator/malformed/retry checks pass791 assertions,
and publication fault checks pass366841 assertions across2528 scenarios. Live
keyboard cancellation during decoding passes for RAM and disk output. A Real CHN
and its parent match the legacy control byte-for-byte and execute successfully
without compiler or library files. Captures bind the existing local emulator and ROM.
The full Real template retains SHA-256
`1da9d420f5598640ed57fa37b099faf09c76fa60dbae31a212b1b341aadf53a3`.
The independent unit compiler remains a separate target and requires integration
qualification before combining both extensions. This stage does not establish
complete TP3 conformance or recovered TP4–TP7 binary library formats.

Evidence is in [optional-real-evidence.json](optional-real-evidence.json).

```sh
zde --no-tty cmake --target rebuild_cache
zde --no-tty cmake --target zpascal_bin zlegacypascal_bin zoptionaltest_bin zoptionalcorpus_bin zpbootfail_bin zexparent_bin --parallel 6
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-optional-real.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-optional-failures.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-optional-corpus.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-optional-interrupt.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-optional-chain.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-optional-media.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-optional-standalone.py PATH_TO_SELECTION_REPORT
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-startup-pages.py
python3 tools/test-optional-runtime-host.py
python3 tools/test-output-host.py
python3 tools/check-startup-banks.py
```
