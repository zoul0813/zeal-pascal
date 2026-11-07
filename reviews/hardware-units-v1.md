# Review: hardware-units-v1

## Opus turn 20261107T052546-23d6b049

Mode: code; read-only

# Review: hardware-units-v1 (initial)

**Verdict:** I found no blocker, but this change isn't ready to call complete. One failure-path regression needs a fix, and the code paths this milestone adds have regression-coverage gaps. No tests were run for this review. Native full-unit success and the current regressions remain pending, as you noted.

## Established findings

### 1. Medium (high if nothing retries the restore): Bank restore can fail after a late serializer failure
**Location:** `src/compiler_units_names.c` `cc_units_write`, `src/compiler_units_object.c` `cc_units_write_object`, `src/library_phase.c` `code_phase_unit_write_end`, `src/compiler_compile.c` `failed:`

**Trigger:** Prefix growth takes the slot released by metadata, then serialization fails before `cc_tables_end()`. Causes include page exhaustion, cancellation and `UNIT_FORMAT`.

**Impact:**
- At that point `write_end` must load two banks, but only the I/O page is guaranteed to come back. The tables and artifact chain are still held.
- With no free pages, `restore()` reloads `CODE_EXPRESSIONS` and then fails on `CODE_FILE_COMPILER`.
- `cc_units_write` returns immediately, and `failed:` frees memory without retrying the restore. A later compile could enter an unloaded bank, and even the retained set no longer matches the warm policy.
- The failed restore also overwrites `platform_error` with its own error rather than the primary one.
- Before this change, I/O and metadata releases covered the single restored file bank, so this is a regression. Page exhaustion is also the condition this work is meant to handle.

**Related path:** If `code_phase_unit_write_begin` drops the file bank but fails on the expression bank, it also returns without a restore.

**Uncertainty:** `library.h` says "the next compile retries it", but no supplied path does so, including `compiler_compile_unit` to `compile_project`. If a wrapper does retry, the remaining impact is mainly the wrong error and a missing bank until the next compile.

**Fix:**
- On failure, release the tables and artifact chain before `write_end`.
- Also retry the idempotent `code_phase_unit_write_end()` in `failed:` after `cc_units_close()`, and preserve the primary error.

**Verification:** Inject allocation failure after metadata release, plus cancellation in the reference stream. Then assert `closed()`-equivalent state and a successful second compile.

### 2. Medium: The regression plan never crosses the new page boundaries
**Location:** `tools/test-plan.json`, `tools/test-zvb.py --hardware-only`, `tests/native/zvb-main.c`

**Trigger:** The plan runs only `--hardware-only`, and `HwProbe` is small. The large-unit path runs only in the separate ZVB probe, which is out of scope for this milestone.

**Impact:** The plan won't catch regressions in these paths:
- multi-page catalog growth in `cc_unit_record`
- `put_byte` and `out_byte` reserving at 16 KiB boundaries
- multi-page module loading
- binding tables beyond a page boundary
- real `cc_unit_descriptor` chain capture
- streamed output and hashing across a prefix page boundary

**Fix:** Add a generated, ZVB-independent fixture that crosses at least one page for each of the catalog, prefix and module. Add a limit case at 65470/65471 bytes, then import and execute the unit.

### 3. Medium: Reference-merge tests are not in the plan
**Location:** `tests/host/unit-references.c`, `tools/test-plan.json`, `tests/test_runner.py`

**Evidence:** The plan diff adds only `tools/test-zvb.py --hardware-only`, and the runner counts rise by exactly one. No supplied runner builds this host test, so the 1824 sanitized assertions are not repeatable through the regression plan.

**Fix:** Add a host runner with ASAN/UBSAN and an exact assertion count to the plan.

### 4. Medium: Unit code above F400 is allowed but unproven
**Location:** `src/compiler_emit.c` `cc_emit`, `src/compiler_front.c` `cc_relocate_value`

**Evidence:** Unit mode now allows emission up to `SOURCE_LIMIT`. The largest measured unit has 29669 code bytes, just below the old 0x7400 (29696-byte) limit.

**Impact:** Nothing tests staging, `cc_code_get`, patching or relocation above F400. Code addresses could also overlap the numeric range of global placeholders. Any `CC_STATIC_LIMIT` or `GLOBAL_BASE` classification elsewhere could then misbehave, and those sources were not supplied.

**Fix:** Add one unit fixture that places a relocated reference and an external call above F400. Add another that stops cleanly at 65534. Confirm a client can import it while executable placement still fails cleanly above F400.

### 5. Medium: Rejection tests cover only one unsafe fixup form
**Location:** `tests/native/zvb-main.c`, `src/compiler_inline.c`, `cc_unit_relocations_prepare`

**Evidence:** The native suite tests only `Inline(<State)`.

**Missing cases:**
- `State+*` and `State+State`
- `5-State`
- `<RoutineName`
- `State-1` at the first global
- an addend at exactly one past the end of globals
- the last valid addend

**Fix:** Add these as table-driven unit-authoring cases that assert both the error code and the cleanup state.

### 6. Low: Cycle branch in chain capture is not exercised
**Location:** `tests/native/fragments.c` `paged_sources`, `cc_fragment_source_capture`

**Evidence:** The cycle sits on a chain of four pages, so capture exits at `i==3` before reaching the duplicate-page check.

**Fix:** Add a two-page cycle.

### 7. Low: Memory exhaustion during load is reported as corruption
**Location:** `cc_units_load_object`

**Evidence:** A `buffer_reserve` failure sets `ok=0`, and the caller then reports `UNIT_FORMAT`.

**Fix:** Use `cc_fail(99)` for the reserve failure.

### 8. Low: Zero-global hashing doesn't poll
**Location:** `cc_units_write_object`

**Evidence:** The globals loop runs one 32-bit multiply per byte without `cc_poll`, while the adjacent code loop polls. Large global areas become a long uninterruptible stretch.

**Fix:** Poll every 64 bytes, matching the code loop.

### 9. Low: Documentation status is stale
**Location:** `docs/unit-hardware.md`, verification section

**Evidence:** It still says larger-unit storage is the next stage and that the Opus review is pending. Its evidence also predates the paging work.

**Fix:** Update it after the measured follow-up results are available.

## Questions to resolve
1. **Hidden calls:** Where do `cc_code_get`, `cc_constant_literal`, `cc_real_literal`, `cc_family`, `cc_runtime_profile_get` and `cc_poll` live? `CODE_EXPRESSIONS` and `CODE_FILE_COMPILER` are unloaded during writing, and I can't prove these calls stay out of them. Please supply a linker-map audit. Native tests should export string, Real and LongWord constants.
2. **Stack footprint:** Each `CompilerFragmentObject` grows from 18 to 46 bytes by my count, so 32 descriptors grow from 576 to 1472 bytes.
   - `cc_units_link_object` has about 2.2 KB of locals, plus about 330 bytes for `Link`.
   - `cc_units_graph_validate` needs about 1.8 KB.
   - Both frames are shallow, but neither is preceded by a `cc_stack_check`.
   - Please measure minimum `SP - s__HEAP` at link and validation.
3. **Word counts:** Do Move and FillChar handle counts of 0 and 32768 or more correctly? Tests use only `W:=3`, and `integer_value` now accepts Word for every caller. Please audit those callers.
4. **`tools/zvb-source.py`:** `tools/test-zvb.py` imports it unconditionally, even with `--hardware-only`, but it isn't in the diff. Is it tracked at this baseline?
5. **`page_initialized`:** The catalog now marks only 256 bytes, and continuation pages are never marked. Does anything depend on the old full-page marking?
6. **Job cleanup:** Does `compiler_owned` cover continuation pages, and is `buffer_following` safe when a job reclaims them directly?

## Checked with no defect found
- **Reference merge:** sorting, external supersession, cursor accounting and failure termination are correct in the supplied code.
- **Streamed hash:** it matches the loader's whole-file hash, including the excluded bytes 28–31 and zero globals.
- **Capacity guards:** catalog, artifact, binding-table and module-ID bounds hold at 65534, 65470 and 0x8000, and the uint16 arithmetic I reviewed doesn't wrap.
- **Fragment snapshots:** capture, liveness, overlap and four-page limits are consistent.
- **Metadata release:** nothing after the early release calls into metadata code, assuming `compiler_units_object.c` is in `BANK17`. Its call structure implies this, but the CMake assignment wasn't supplied.
- **Hardware rules:** retained-fragment requirements, Real behavior and ordinary program Inline behavior match the assignment in the supplied code.

## Review limitations
- **Sources not supplied:** `zp.h`, the full `compiler_emit.c` and `compiler_front.c`, job and compile-entry wrappers, `unit_output.c`, and linker maps. Bank placement is partly inferred.
- **Follow-up inputs:** the rebuilt artifacts and measured results are still needed, along with the failure-injection, boundary and stack evidence requested above.

### Codex reconciliation

Accepted findings 1–9. Failure cleanup releases abandoned tables/output, retries
idempotent phase restoration and preserves the primary platform error. Native
late-prefix allocation failure passes 57 checks; cancellation passes 54. Both
verify the warm cache, no pending phase handles and a fresh successful compile.
The debugger fault stub is written in <=6-byte chunks and read back before use;
an earlier incomplete patch stalled the harness and is excluded from evidence.

The regression plan now includes independent paged-unit storage and sanitized
reference merging. Storage passes 286 boundary checks: catalog/prefix/module
crossings, exported String/Real/LongWord constants, 65,470-byte acceptance,
65,471-byte rejection, relocated external calls above $F400, code ending exactly
at 65,534, and saved clients after deleting all unit sources and libraries.
Native hardware coverage now passes 336 checks including every requested unsafe
Inline fixup, last valid storage addend and Word-sized memory counts. A real
two-page cycle is covered. Loader allocation errors report 99. Zero-global
hashing polls every 64 bytes. Documentation describes current storage and status.

The necessary import-memory fix now scans and authenticates complete files but
retains only metadata, keeping file handles in a retained owner. Linking verifies
one physical file page before emitting selected code. Generic fragment emission
rejects metadata-only descriptors; the unit linker owns disk-backed emission.
Failed file closes remain retryable across job cleanup. Reader/loader transport
host suites pass 65,525/45 sanitized checks; metadata semantics pass separately
in 80 decoder checks and native suites. The PC-site validator now merges sorted
streams linearly (700-site/1,400-reference fixture bounds metadata reads).

Questions: linker map places cc_code_get, cc_poll and cc_runtime_get resident;
cc_constant_literal BANK4, cc_real_literal BANK9, cc_family BANK7 and unsigned
metadata decoding BANK11. None is in lent BANK6/BANK10. Complete CMake/map context
is supplied. Descriptor arrays remain invocation-local; native watermark margins
are >=8,421 bytes across storage and ZVB clients, with >=2,048 required by the
harness. This is observed lower-bound evidence, not an all-input stack proof.
Move/FillChar passes 229,402 independent CPU assertions including 0, 32,768 and
65,535 counts and overlap. integer_value callers all consume 16-bit bit patterns
(Move/FillChar lengths, heap sizes and StackPtr); unrepresented unit heap/stack
operations remain rejected. tools/zvb-source.py is tracked at the baseline.

page_initialized is diagnostic high-water accounting, not access authorization;
all catalog bytes are explicitly written, and snapshots use role/lifetime/version.
Continuation pages allocate PAGE_COMPILER under the existing job ownership mask.
Raw job reclamation is terminal; subsequent page allocation calls buffer_init
before any traversal, replacing stale following entries. Loader/reader cleanup
is independently tested for retryable errors.

Latest regressions: unsigned 932, unit/Real 1,703, initialization 623, focused
hardware 336, host fragments 995 and native fragments 934 all pass. Additional
ZVB clients (separate API milestone) pass graphics 140, page/sound 59 and basic
API/CRC/DMA 109 checks. Production startup policy remains 10 cold / 15 warm,
18 logical banks; resident 15,958 bytes, ZPL 275,317. No claim of smaller compiler
ZPL or physical-board validation. Follow-up assessment is outstanding.

## Opus turn 20261107T082439-ec3cafce

Mode: code; read-only

# Review: hardware-units-v1 (follow-up)

**Verdict:** All nine initial findings are closed, and I found no blockers. **One new medium issue remains:** holding a file descriptor per imported unit changes the supported dependency graph, and that limit is untested. One complete run on the final sources is also still needed before claiming acceptance. I didn't run any tests; all results below are as you reported them.

## Dispositions of initial findings

| # | Status | Basis |
|---|---|---|
| 1 | Closed | Failed writes now release tables and output before restoring banks. `failed:` retries the restore, and partial lending is rolled back. Page accounting shows that a successful link, import or write can always restore its lent banks. |
| 2 | Closed, pending final run | Generated page-crossing fixtures are now in the plan. See finding 3 below. |
| 3 | Closed | `tools/test-unit-references-host.py` is in the plan, and the runner counts match. |
| 4 | Closed, pending final run | `HIGH` and `LAST` cover code above F400 and the exact end of unit code. |
| 5 | Closed | All requested unsafe fixups and the last valid addend are now covered. |
| 6 | Closed | The two-page cycle reaches the duplicate-page check. |
| 7 | Closed | Loader allocation failure reports 99. |
| 8 | Closed | The zero-global hash loop now polls. |
| 9 | Closed for now | The storage description is current. Update the status text after this review. |

For the earlier questions:
- **Hidden calls:** Closed. The map and CMake show that the write and link paths avoid lent BANK6 and BANK10.
- **Word counts:** Closed by the 229,402 CPU checks you reported.
- **`tools/zvb-source.py`:** Closed. The file is supplied.
- **`page_initialized` and job cleanup:** I accept your answers, but `platform.c` and the job page allocation code weren't supplied, so I couldn't verify them directly.
- **Stack:** Only partly closed; see finding 1.

## Established findings

### 1. Medium: One open descriptor per module adds an untested limit to dependency graphs
**Location:** `src/compiler_unit_loader.c` `cc_units_load_object`, `src/compiler_unit_files.c`

**Trigger:** A program or unit imports enough modules to exhaust the kernel's descriptors. Descriptors are also needed for the code-library handle, output, and any later `{$I}` file.

**Impact:**
- Before this change, each file closed after loading, so the graph limit was the 32-module catalog limit. Now every module holds a descriptor until `cc_units_close`.
- The new `files[32]` table doesn't guarantee that the OS can open 32 files.
- If `open` fails for any reason, the code reports `UNIT_MISSING`, so descriptor exhaustion looks like a missing unit.
- If a later `{$I}` or disk-output open fails, the failure surfaces there, not at the import that consumed the descriptors.

**Evidence:** The host reader test checks only the table's 32 slots with a stubbed `close`. The supplied native fixtures use only a few modules. The kernel's limit wasn't supplied, so the exact failure point is unknown.

**Fix:** Close each descriptor after the scan, then reopen it for each page window. Each window is already verified against its page hash, so reopening doesn't weaken the snapshot check. You could instead define and enforce a lower module limit. Either way, report any open failure other than `ERR_NO_SUCH_ENTRY` as a platform failure.

**Verification:** Run a native chain of 32 modules, plus a program that uses `{$I}` after importing them. Record stack depth in the same run. Current stack evidence covers only ZVB and the storage clients. My rough estimate is that `cc_units_load` through `cc_fragment_validate_source` needs about 800 bytes below `cc_units_graph_load`, close to the 768-byte `cc_stack_check` margin.

### 2. Low: New sources have incomplete rebuild dependencies
**Location:** `CMakeLists.txt`

**Evidence:** This SDK has no compiler depfiles.
- `compiler_unit_loader.c` doesn't list `zp.h` or `output_transport.h`.
- `compiler_unit_reader.c`, `compiler_unit_files.c` and `compiler_unit_fragments.c` don't list `zp.h`.
- `compiler_fragment_access.c` and `compiler_fragment_source.c` have `zp.h` only through the standalone fragment target's property on the same source, so check that it applies here.
- `compiler_unit_references.c` and `compiler_units_symbols.c` use `set_source_files_properties(... OBJECT_DEPENDS ...)`, which replaces the earlier appended list. That removes `compiler_fragments.h` and leaves `zp.h` unlisted.

**Impact:** Changing `SOURCE_LIMIT`, a call macro or a structure can leave stale objects in incremental ZDE builds.

**Fix:** Add these sources to the existing dependency property after the final `set_source_files_properties` calls.

### 3. Low: Acceptance evidence comes from separate runs
**Location:** `docs/unit-storage-evidence.json`, `docs/unit-hardware.md`, `tools/test-unit-storage.py`

**Evidence:** `test-unit-storage.py` passes only when the boundary and fault phases succeed in one run. The boundary capture predates the harness patch, and the documentation lists its count as current. The evidence hashes also omit the failure-path sources `compiler_units_names.c`, `compiler_compile.c` and `library_phase.c`, plus the harness.

**Fix:** Run the full plan entry once on the final tree. Record that capture and extend the hash list, then change the `follow-up pending` status.

## Questions
1. **Short reads:** `cc_unit_code_page` treats any `n!=want` as `UNIT_STALE`, while the loader accepts partial reads. Does every supported Zeal filesystem return full reads before EOF? If not, loop until EOF.
2. **Persistent close failure:** `cc_unit_files_close` keeps failed descriptors for retry, and every compile runs it first. Could a `close` that always fails, such as after media removal, block all later compiles until restart? If so, define when to abandon the handle.
3. **Word-count build input:** `tools/test-word-counts.py` isn't in `native_targets`, and neither it nor `word-counts-runtime.h` was supplied. Does it build its runtime inputs itself?

## Checked with no defect found
- **Loader framing:** Bounds, prefix detection, the 64-byte binding reserve and the excluded hash bytes are correct. So is the layout of the eight hash words across `UR_FILE` and `UR_HASH`.
- **Disk-backed emission:** Window indexing, per-module cache reset, hash verification before use and page bounds are correct. `cc_fragment_link` also rejects metadata-only objects.
- **Metadata limits:** `cc_unit_module_byte` can't read past the stored prefix, and bindings are reached only through direct binding-table accessors.
- **Cached types and symbols:** `cc_unit_import_symbol` needs no decoder, and `resolved_type` fails closed for unresolved entries. The literal `2` for `UT_READY` is correct, but a shared constant would be safer.
- **PC-site merge:** It preserves the old overlap check, including fragment changes, and fits the validated relocation ranges.
- **Ownership and cleanup:** Module pages, fragment indexing, descriptor closing and failure ordering are sound in the supplied paths.

## Review limitations
- **Sources not supplied:** `banks.c`, `output_transport.c`, `compiler_routines.c` and the kernel's descriptor limit. The current `tools/test-zvb.py` and `tools/test-word-counts.py` also weren't supplied.
- **Bank headroom:** BANK11 has 2 bytes free, BANK9 has 29, and BANK6 has 46. This isn't a defect, but small changes there will need code moved to another bank.
- **Review budget:** This is the final budgeted turn, so any further Opus review needs your request.

### Codex reconciliation

Accepted the descriptor-limit finding and rebuild-dependency correction. Imports
now close their scan descriptor and retain only the resolved path in compiler
catalog records. Each requested file page reopens that exact path, streams its
bounded ranges, then closes it. Failed closes remain retained for retry; they
are never silently abandoned. Non-missing open errors report platform failures.
A persistent platform close failure can prevent subsequent compilation until
restart; retaining ownership is deliberate, since the OS has not confirmed the
handle is released. Short reads are accumulated rather than rejected.

The initial full-page link window still exhausted physical pages for graphics
clients at the second staging-page allocation. Its replacement copies selected,
preplanned ranges into the unpublished image using 64-byte reads and verifies
the complete page hash before any relocation processing. A mismatch rejects the
whole build. This removes the separate window page; it does not increase startup
pages or lend an active compiler coordinator bank. Host transport checks pass
65,626 assertions plus 45 loader checks after these changes. Full final-tree
native verification remains outstanding.

All new compiler sources now have explicit SDK/header rebuild dependencies. The
remaining request for a combined storage run will be performed after the target
build fits. The final evidence must include the failure-path and harness hashes.
32 sequential open/close cycles are checked with no retained descriptors. A
native dependency-depth/capacity probe is being prepared; the format limit of
32 modules is separate from available physical RAM for their metadata pages.

These fixes changed source after the last Opus turn. No review approval or
finalization is claimed; the skill budget is exhausted, and any further Opus
turn requires the user's request.

Checkpoint verification after fixes: production and all regression drivers fit
18 banks. Native unsigned1188, unit/Real1703, initialization623, hardware336 and
fragment934 pass; combined storage286 + allocation57 + cancellation54 pass on
one final harness run. Host reader65980 + loader45, decoder80, references1824,
Word-count229402, generated engine26004 and full sanitized host regression pass.

The Zeal VFS source audit supersedes the earlier retained-close assumption:
zos_vfs_close clears the descriptor entry after every filesystem close attempt.
The unit owner now clears its single active handle while preserving close errors.
This avoids retrying a stale descriptor or blocking later compiles. The exact
read-only kernel excerpt/hash is in reviews/zeal-vfs-close-reference.txt.

The SDK exec argv-array correction affects only test-demo arguments; the final
combined storage run uses the updated driver. Full proof and current hashes are
in docs/unit-storage-evidence.json. This is a tested local checkpoint, not an
Opus-finalized milestone.

Checkpoint verification after fixes: production and all regression drivers fit
18 banks. Native unsigned1188, unit/Real1703, initialization623, hardware336 and
fragment934 pass; combined storage286 + allocation57 + cancellation54 pass on
one final harness run. Host reader65980 + loader45, decoder80, references1824,
Word-count229402, generated engine26004 and full sanitized host regression pass.

The Zeal VFS source audit supersedes the earlier retained-close assumption:
zos_vfs_close clears the descriptor entry after every filesystem close attempt.
The unit owner now clears its single active handle while preserving close errors.
This avoids retrying a stale descriptor or blocking later compiles. The exact
read-only kernel excerpt/hash is in reviews/zeal-vfs-close-reference.txt.

The SDK exec argv-array correction affects only test-demo arguments; the final
combined storage run uses the updated driver. Full proof and current hashes are
in docs/unit-storage-evidence.json. This is a tested local checkpoint, not an
Opus-finalized milestone.
