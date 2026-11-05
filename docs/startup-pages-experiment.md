# Startup code page experiment

The experimental worktree now allocates **11 code pages at startup instead of 13**,
saving **32768 bytes (32 KiB)**. This is measured in the native emulator, including
a positive allocator budget that reaches the IDE with only 15 available pages:
three process pages, eleven code pages and one Work page. One fewer cannot
allocate Work. This is a deliberate Zeal behavior, not recovered TP3 semantics.

The logical directory still has 15 banks. Zero-based banks 5 and 9 (expression,
interrupt, file and I/O compiler emitters), plus the existing suffix 13 onward,
stay on disk until compilation. The phase loader in BANK5 remains loaded. Sparse
loading seeks absolutely across skipped payloads. Compilation restores required
pages before calling compiler code. RAM Run and saved child execution park the
compiler pages; native entry probes observe eleven retained code pages and no
pending owners. On return the previous warm set is restored, so Execute from a
cold session stays cold. A failed compile may keep valid partially loaded cache
pages; source/build descriptors remain intact and compilation retries after
memory pressure is released. Parking releases partial cache pages for the child's
lifetime; returning reloads the previous partial set. A successful compile fills
the remaining cache, and quitting releases it.

This reduces allocated RAM, not disk size. The current `.zpl` is 202359 bytes
(previous 202277, +82); resident `.bin` is 15285 bytes (previous 15068, +217).
Static data ends at 52366 (+4 bytes), leaving 13170 bytes of C stack reserve
(previous 13174). BANK5 occupies 11587 of 16384 bytes, leaving 4797 bytes.
No new resident or data page is allocated. The runtime prefix and generated
standalone program layout remain unchanged. Real/Screen extraction and an IDE
unit-authoring command remain separate work.

Evidence is indexed in [startup-pages-evidence.json](startup-pages-evidence.json).
The native startup report passes loading, allocation, lifecycle and interactive
checks. Production file, path, full-media, user-break, Options, Execute,
directory/logged-drive and TP3 capacity workflows are qualified separately in
the evidence index. Members tests pass 186 positive and 193 negative/retry
assertions, including
real SDK low-memory compile/retry, cold Execute, kernel ownership before automatic
OS cleanup, pending ownership and real assembly partial-release retry after
poisoning the freed loader page. The host suite passes 366841 assertions across 2528
fault/publication/cancellation scenarios, including free failure at each logical
slot. All 323 native unit checks pass. `tools/check-startup-banks.py` verifies the
cold symbol providers and compile-only callers in the normal linked build.

These are incremental ZDE builds on the pinned local emulator and ROM, not full
conformance or physical hardware evidence. The original review baseline
preserves the
preceding dirty unit prototype. Its relevant product files are now committed in
`10f3812`; the preceding fixture-correction commit is indexed as the commit
baseline for this startup stage. No original CP/M binary,
third-party SDK, emulator, or OS source was modified.

Reproduce with the existing ZDE build and local environment:

```sh
zde --no-tty cmake --target rebuild_cache
zde --no-tty cmake --target zpascal_bin zptest_bin zpbootfail_bin zmemberstest_bin zpexecchild_bin --parallel 6
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-startup-pages.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-members.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-units.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-output-host.py
python3 tools/check-startup-banks.py
for case in files paths full-media user-break; do
  ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-native.py --case "$case"
done
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-options.py --ide
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-ide-execute.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-ide-directory.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-tp3-open-items.py
```

The qualification fixes change test observations, not product behavior: saved
compilation releases its RAM cache; Save has no overwrite-confirmation prompt;
Options/Return refreshes the menu before screen sampling; and Run may allocate
an absent Build image window. Ownership checks account for the exact new image
and restored compiler handles, preserving every other owner/bitmap entry.
Full-volume checks fail on creation
of a new path, avoiding the pinned OS's previously documented existing-file
growth fault. Raw captures remain local; the evidence index retains results and
source, build, external-artifact and audit hashes.

Two read-only Opus review turns found no product defect. Codex closed the remaining
evidence gaps above. All 29 reviewed product source files are unchanged; the final
fixture and documentation corrections were not re-reviewed, so formal runner
finalization of this exact snapshot is not claimed. See the
[reconciled review](../reviews/startup-pages-v1.md) for dispositions and limits.
