# Review: optional-real-v1

## Opus turn 20261105T171007-34bddba5

Mode: code; read-only

# Review of optional-real-v1: findings

**Verdict:** I found no blocker or demonstrated miscompile in the supplied code, but there are four medium findings and one low finding. Findings 1 and 4 should be resolved before relying on selection completeness. I ran nothing, so test results below are your reported evidence.

## Established findings

### 1. Medium: Real demand raised after the patch decision is silently dropped
- **Location:** `src/compiler_compile.c:150-152`, `cc_real_runtime_finish`, `cc_require_real` in `src/compiler.h`
- **Trigger:** Any emitter that runs in `cc_params_finish()`, `cc_random_finish()` or a later finisher reaches `math_call`, or otherwise needs the Real engine.
- **Impact:** `cc.real_needed` becomes 1 after the only point that reads it. The build then succeeds with the integer core. In `src/real.c`, its `zr_eval` returns `zr_error=0xff` for anything except `ZR_VAL_INTEGER`, so the program fails or misbehaves at runtime instead of at compile time.
- **Evidence:** The patch decision is made once, before all finishers. Nothing checks the flag afterwards. `compiler_random.c` is the first file to check (Question 1).
- **Fix:**
  1. Record that the decision was taken, for example with `real_decided`.
  2. Make `cc_require_real` raise `cc_fail(99)` if demand arrives after that point on a non-CHN build that did not apply the patch.
  3. Add a native test that reaches this guard.

### 2. Medium: `compiler_runtime.c` is missing the header dependencies this SDK requires
- **Location:** `CMakeLists.txt:875-878`, and the `foreach(source IN LISTS PASCAL_CORE)` dependency loop
- **Trigger:** An incremental build after changing `compiler.h`, `library.h`, `zp.h`, `sources.h`, `real.h` or `chain_abi.h`.
- **Impact:** `compiler_runtime.c` is added to `optional_real_core` outside `PASCAL_CORE`, so that loop never covers it. Its only declared dependency is `optional_runtime.h`. The file dereferences `cc`, `cc.sources->options` and `cc.error`, and it uses `code_pages` and the `CODE_*` indices. A stale object could therefore use wrong struct offsets or bank numbers in the production compiler.
- **Evidence:** The CMake comment states that the SDK disables compiler depfiles, which is why the loop exists.
- **Fix:** Add explicit `OBJECT_DEPENDS` for at least `compiler.h`, `library.h`, `zp.h`, `sources.h`, `real.h` and `chain_abi.h`. Add `zp.h`, `sources.h`, `output_transport.h` and any other direct includes to `tests/native/optional-real-main.c`.

### 3. Medium: `integer_runtime.private` is consumed but never declared as a byproduct
- **Location:** `CMakeLists.txt:69-71` (producer), `CMakeLists.txt:853-854` (consumer)
- **Trigger:** A clean configure and build, or an incremental runtime rebuild, under a generator that requires explicit byproducts.
- **Impact:**
  - The `pascal_runtime` and `compact_runtime` rules both declare `.private` and `.runtime.json` as `BYPRODUCTS`, but the new `integer_runtime` rule does not.
  - The optional runtime rule still lists `integer_runtime.private` in `DEPENDS`.
  - Under Ninja with CMP0058 NEW, a clean build can stop with "missing and no known rule". An incremental build can also miss the regenerated file until a later run.
  - `validate_optional_runtime` should reject stale assets, so the likely failure is a broken build, not a silently bad product.
- **Limit:** The effect depends on the generator, and the `zos_add_outputs` implementation was not supplied.
- **Fix:** Add `BYPRODUCTS` for `integer_runtime.private` and `integer_runtime.runtime.json`. Verify from an empty build directory using the commands in `docs/optional-real.md`.

### 4. Medium: production selection has narrow regression coverage
- **Location:** `CASES` in `tools/test-optional-real.py`, the `zptest`-family link lists in `CMakeLists.txt`, and `tests/native/optional-real-main.c`
- **Trigger:** Any Real path or opaque feature outside the supplied case list.
- **Impact:** The native suites listed in `CMakeLists.txt` link `zp_core`, not `optional_real_core`. `cc_require_real` also compiles to nothing without `ZP_OPTIONAL_REAL`, so the 366841 host assertions cannot prove selection unless that host build defines the macro. The production selector is therefore exercised by only the optional cases supplied here.
- **Gaps:**
  - Opaque selection covers `Inline` only. There are no `external`, hook variable, `Chain` or `Execute` cases.
  - No case covers a CHN that uses Real under a parent built by production `zpascal`.
  - No case covers `Random`, implicit Integer-to-Real conversion in argument or assignment position, or Real in Text write, typed files or overlays where supported.
  - Allocator exhaustion calls `code_phase_real()` directly. It does not exercise the `compile()` path.
  - Cancellation during patching has no test, including in disk mode.
- **Fix:**
  1. Add the missing cases, and compare their saved bytes against `zlegacypascal`.
  2. Add a corpus oracle that compiles the existing corpus through the optional core and fails on any Real helper use in an integer-selected image.
  3. Add compile-path exhaustion and interruption tests for both RAM and disk.

### 5. Low: dependency scope and generator inputs are incomplete
- **Location:** `CMakeLists.txt:853-857`, `CMakeLists.txt:877-878`
- **Issue 1:** The `OBJECT_DEPENDS` on `optional_runtime.h` applies to `src/compiler_asset.c` as a source-level property. It therefore also affects `zp_core` and `unit_core`, which do not use that header, and may couple legacy targets to the integer runtime build.
- **Issue 2:** `package-optional-runtime.py` reads `src/zp.h`, `pascal_runtime.runtime.json` and `integer_runtime.map`, but none of them are in `DEPENDS`. A stale `RT_CHAR` gate proof is possible until something else triggers a rerun.
- **Fix:** Add the three inputs to `DEPENDS`. For the header dependency, use a dedicated wrapper source for the optional build or another target-scoped arrangement.

## Questions needing unsupplied files

1. **`compiler_random.c`:** Does unbounded `Random` reach `RT_REAL`, either directly or from `cc_random_finish`? This is the most important check, because either route could bypass selection or hit Finding 1.
2. **`compiler_expr.c`, `compiler_keyboard_engine.c`, `compiler_interrupt.c`, `compiler_params.c`:** Do all uses of `RT_REAL`, `RT_FORMAT` and `RT_VAL` go through the flagged paths? Does any `cc_runtime_get` or `cc_original_entry(...,0)` copy bytes outside the five verified gates?
3. **`platform.c`, `compiler_job*.c`, `native.asm`, `tools/test-native.py`:**
   - Does anything still assume `CODE_RUNTIME` is loaded?
   - Does anything call `cc_runtime_*` outside a compile? `check-startup-banks.py` audits direct imports only, not transitive use of the cold BANK10 head.
   - Does `compiler_resources_ready()` still budget pages correctly with the temporary Real page?
4. **`compiler_disk.c`:** RAM and disk patching share `cc_put` and `image_get`, and your byte-identical saved output supports that path. I could not inspect sector-cache coherence, write-error behavior or `cc_disk_image` before placement.
5. **`absolute` and `Mem` policy:** Should direct access to the absent engine region count as opaque? For example, `var P:Byte absolute $4D8A` reads different bytes in integer mode, while selection does not treat `S_ADDRESS` as opaque.
6. **`.gitignore`:** The new whitelist entries omit `src/compiler_runtime.c` and `tests/native/optional-real-main.c`. Please run `git check-ignore` on both before staging.

## Reviewed with no actionable finding

These conclusions cover the supplied code only:

| Area | Result |
|---|---|
| **Prefix ordering** | The patch runs before finalizers and placement bindings. The supplied finalization writes all come after it. Patch data starts at `0xBD6`, and the only parse-time prefix write shown is `RT_CBREAK`. |
| **Private ABI and gates** | Indices 6, 14, 15 and 21 are the correct enum entries. Together with `RT_CHAR`, they match the sites restored by `cc_activation_place`. |
| **Bounds and malformed input** | The native parser and host validator agree on header, record, destination, back-reference, trailing-data and unused-flag rules. Full reconstruction is checked at package time. |
| **Failure and cancellation** | Partial patches go through the existing failure path, which discards the Build. Primary `platform_error` is retained across release. `cc_fail` retains the first error, and polling is bounded. |
| **Pending owner, free and retry** | `drop_bank` matches the previous behavior. A failed free retains ownership for later release, and `resume_mask` cannot restore the released component. |
| **CHN and transfers** | CHN skips patching, and any `Chain` or `Execute` statement forces the parent to the full engine. A shared profile is safe on the supplied paths. |
| **Non-optional objects** | `real_needed` is the last `Compiler` field, so objects built without the macro keep the same offsets for earlier fields. |
| **Claims and artifact separation** | `docs/optional-real.md` claims compiler and IDE RAM savings only, and says generated page counts stay fixed. Its size arithmetic is correct. `.ZPU` and `.ZPL` remain separate, and the diff touches no original CP/M or third-party artifact. |

## Review limits

- `docs/optional-real-evidence.json` and `reviews/optional-real-v1.md` were not supplied, so I could not cross-check the evidence index.
- Build outputs, the SDK `zos_add_outputs` implementation, and the files named in the questions were also not supplied.
- New console Read cases and production startup and low-memory reruns remain unfinished evidence, as you noted.
- The harness lists 11 cases, while the reported matrix covers nine. The two console Read additions account for the difference and still need to pass in both the selection and standalone runs.

### Codex reconciliation

1. **Rejected as a current defect, with emitter audit.** All calls to `RT_REAL` come from parse-time `compiler_real.c:math_call`. Formatting and Val calls come from the flagged parse-time I/O paths. `compiler_random.c` emits its own Real6 normalization into the existing public buffer and calls no Real engine, including in `cc_random_finish`. Params, keyboard, interrupt, file, console, hook, transfer, error, unpack and startup finishers introduce no unflagged engine demand. The follow-up includes those sources. Adding a second state machine and an artificial guard test would protect a hypothetical future emitter rather than fix current behavior. The production-selector corpus, explicit Random/raw-copy tests and conversion/format cases qualify the current invariant.
2. **Accepted.** Added explicit SDK header dependencies to the optional decoder and native fixture, including compiler/library/source/ABI/output/disk headers.
3. **Accepted.** Declared integer runtime `.private` and `.runtime.json` byproducts. An empty `build-optional-clean` ZDE build passes and produces byte-identical production `.bin`/`.ZPL` outputs.
4. **Adapted.** Expanded the production/control matrix to17 cases, added the existing15-family language/Real/format corpus under the production macro, native compile-path exhaustion for RAM and disk (791 assertions), and live keyboard cancellation at an authenticated decode poll with a live Real page in both output modes. The corpus fixtures now count actual allocated cache pages and capture absent immutable bank bytes directly from the existing open library handle. Those are observer corrections, preserving the existing assertion counts. External declarations, hook variables and transfer statements compare byte-for-byte with the full-runtime control. The new direct CHN check compiles a Real CHN and its parent in both IDEs, verifies byte identity for both files, then executes the production pair without compiler/library/source files. The SDK supervisor verifies the Real output marker, success, foreign ownership and complete page recovery.
5. **Partially accepted.** Added `zp.h`, full-runtime metadata and integer map to generator dependencies. Retained the shared source-level generated-header dependency. It safely overbuilds non-optional variants under this SDK and avoids a duplicate wrapper source solely for build isolation. The independent legacy control and fresh production build qualify it.

Questions were audited with the newly supplied emitters, platform/job/disk/native code and test driver. Raw Mem/absolute access intentionally observes the selected native template. Public helper addresses and copied callback gates remain compatible. Adding all Mem accesses to opaque selection would defeat the approved raw Real-copy use case. Direct native Inline/external/hook access requests the full template. `git status` and `git check-ignore` confirm both new C files are included by existing src/native directory rules. The final evidence index and complete source context accompany the follow-up. The production-selector corpus passes all1982 assertions across15 families. Decode cancellation/retry passes both output modes, native failure/retry passes791 assertions, and the clean build is byte-identical.

## Opus turn 20261105T173906-d1f328da

Mode: code; read-only

# Follow-up review of optional-real-v1

**Verdict:** I found no blocker or high-severity defect, and I accept all five prior dispositions for the sources supplied. There is one new medium finding: Real patching in disk mode thrashes the one-sector cache. Measure it on `T:` and `B:` before staging. I ran nothing, so all results below are your reported evidence.

## Prior findings

| Finding | Disposition | Assessment |
|---|---|---|
| **1. Late Real demand** | Rejection accepted for supplied code | In the supplied sources, `RT_REAL` is reached only through `math_call`. `cc_random` normalizes Real6 itself and calls no engine, and `cc_random_finish` emits only the generator. `number_result` selects `ZR_VAL_REAL` only from a job kind flagged at parse time. The supplied finishers add no engine demand. Question 1 lists the remaining audit limit. |
| **2. Header dependencies** | Resolved | `compiler_runtime.c` and `optional-real-main.c` now have explicit dependencies. |
| **3. Byproducts** | Resolved | `.private` and `.runtime.json` are declared. The evidence index records clean-build byte identity, with the capture gap noted in Finding 9. |
| **4. Coverage** | Resolved, with the residual risk in Question 2 | The 17-case matrix now covers external, hook and transfer selection, Random, conversion and Text write. Compile-path exhaustion, live cancellation and an executed Real CHN are also covered. |
| **5. Dependency scope** | Accepted as adapted | The inputs were added. Keeping the shared header dependency is an acceptable overbuild, and I no longer treat it as a defect. |

The earlier questions on `absolute`/`Mem`, `.gitignore`, `platform.c`, job page handling and native bank use are also closed for the supplied code. The `Mem`/`absolute` behavior is now documented as intentional.

## New findings

### 6. Medium: disk-mode Real decoding thrashes the single-sector cache
- **Location:** `unpack` in `src/compiler_runtime.c`, and `sector`, `flush`, `raw_get` and `raw_put` in `src/compiler_disk.c`
- **Trigger:** A saved COM compile of any Real or opaque program, when a back-reference source is in a different 128-byte sector from its destination.
- **Mechanism, for each such byte:**
  1. `image_get` flushes the dirty destination sector and reads the source sector.
  2. `cc_put` reads the destination sector back and marks it dirty.
  3. The next byte repeats the cycle.
- **Impact:** Each byte can cost one sector write, two sector reads, their seeks and a `cc_poll` in `flush`. The patch expands to thousands of bytes, so this may mean thousands of rewrites of the same scratch sectors. That adds compile time and write wear on TF or EEPROM media, and `T:` is the default source location.
- **Evidence gap:** Disk patching was only exercised on `H:` in the supplied Real and cancellation tests. Those tests prove correctness, not media cost.
- **Fix:**
  1. Keep a private 4 KiB circular history, for example in unused space on the temporary Real page if the patch leaves room.
  2. Resolve back-references from that history, and make output strictly sequential.
  3. Keep the existing bounds checks and polling.
- **Verification:** Count `flush` and `sector` operations during patching, and time a Real saved compile on `T:` and `B:`. Compare both with `zlegacypascal`.

### 7. Low: the standalone gate can silently shrink
- **Location:** `tools/test-optional-standalone.py`, the `if not(source/(name+'.COM')).exists(): continue` check and `assert previous['optional']['passed']`
- **Impact:** A missing COM is skipped without a failed check, so a partial or empty run can still report success. The source-report guard also no longer requires the control comparison to have passed. The current evidence is unaffected because it records all 17 runs.
- **Fix:** Require the expected case set unless a subset was explicitly requested, and require a nonempty `checks`.

### 8. Low: the shared page assertion no longer checks the code-page count
- **Location:** `cached_code_pages` in `tests/native/harness-execute.c`, and its uses in `tests/native/main.c`
- **Impact:** `pages_used()` includes the same loaded code pages, so `pages_used()==cached_code_pages()+1` now checks only pending pages and ledger pages. In the `zp_core` suites, it can no longer detect a deferred bank that failed to return after parking. It also misses a retained Real page in the corpus, although `closed()` in `optional-real-main.c` covers that case for the targeted test.
- **Fix:** Pass an explicit warm-page expectation for each build variant: `code_required` for legacy, 14 for optional.

### 9. Low: the evidence index does not bind every claimed gate
- **Location:** `docs/optional-real-evidence.json`
- **Gaps:**
  - File, path and Ctrl-C workflows have no entry. The startup report covers library, allocation, lifecycle and interactive checks only.
  - `clean_zde_build_byte_identity` contains booleans, with no capture path or input hash.
  - The startup report binds `CMakeLists.txt` to `9dde0583...`, while the final file is `d4ded9a7...`. Its `zpascal` artifacts match the final hashes, so this does not undermine the result, but the record is not final-source bound.
- **Fix:** Add the missing reports and hashes, or rerun those gates on the final source and record the captures.

## Questions

1. **Emitter audit outside the supplied files:** `compiler.c`, `compiler_strings.c`, `compiler_types.c`, `compiler_bridges.c`, `compiler_module.c`, `compiler_late.c`, `compiler_members.c` and `compiler_groups.c` were not supplied. Please attach the `RT_REAL`, `RT_FORMAT`, `RT_VAL`, `RT_INPUT_VALUE` and `cc_real_` search results for all of `src/` to close Finding 1 completely.
2. **Overlay members:** Real use in an overlay member was not covered by the supplied optional tests, and the group sources were not supplied. Please confirm that `cc.real_needed` survives member compilation, or add one case if production can compile overlays. The optional corpus also excludes the separate suites such as math, Text, hooks and chain, so those remain covered only by the 17-case matrix and targeted tests.

## Review limits

- I did not inspect build outputs, captures, the SDK's `zos_add_outputs`, or `reviews/optional-real-v1.md`.
- Hashes in the evidence file were compared only against each other.
- Generated execution-page savings remain unclaimed. User `.ZPU` integration remains separate, and I saw no change to original CP/M or third-party sources.

### Codex reconciliation

6. **Accepted and implemented after this follow-up.** The LZ decoder uses the final4 KiB of its already owned temporary data page as circular history. The packer and native preprocessor reject overlap with the compressed asset. Back-reference reads no longer touch the output cache. Authenticated native profiles on disposable T:/B: media observe92 flush calls for11310 decoded output bytes. Both saved outputs equal the legacy control. Instrumented wall times were1.64s versus1.07s on TF and4.17s versus2.77s on EEPROM; the optional measurement includes92 debugger flush breakpoints, so these are observer timings rather than hardware latency or a fair throughput benchmark. The history consumes no additional page. Integer/Real native byte comparison, standalone execution,791 allocation/malformed/retry assertions, RAM/disk decode cancellation,1982 corpus assertions, startup/allocator/lifecycle/interactive workflows and independently configured build identity all pass after the fix.
7. **Accepted.** Standalone now requires the completed production/control source report, derives the exact explicitly compiled case set from its labels, requires it to be nonempty, and opens every expected COM. A missing output raises an error. The updated gate passes on the final Integer/Real subset.
8. **Accepted.** The shared legacy observer again expects `code_required` pages. The optional observer is compiled separately with `ZP_OPTIONAL_REAL` and expects `code_required-1`. The full corpus passes with these explicit contracts and unchanged assertion counts.
9. **Accepted.** The evidence index now records final-source startup/file/path/Ctrl-C captures, the initial empty-directory build log, final independent-directory rebuild log, config/log/source/artifact hashes and byte comparisons. Historical17-case/CHN evidence is kept separate from final-source history-fix qualification.

The full-src emitter search is retained in `analysis/optional-real-review/real-emitter-audit.txt`, with its hash in the evidence. Production Pascal overlay syntax is currently rejected (`compiler_routines.c`); the separately staged native overlay/group C fixtures do not implement Pascal overlay compilation. Thus no supported overlay member path resets Real demand. `cc` is initialized once per compile and engine demand remains in that invocation.

**Remaining review gate.** These fixes changed code after the second review. The two successful turns are recorded, but their completion is not reused for the current snapshot and runner finalization is not claimed. All required verification for this checkpoint passes. One additional restricted Opus pass requires the user's explicit request under the project skill's two-turn limit. No additional review has been started.

## Opus turn 20261105T192028-d26413aa

Mode: code; read-only

# Extra review of optional-real-v1 at f968c6c

**Verdict:** Findings 6, 7 and 8 and both prior questions are resolved in the supplied source, and I found no blocker, high-severity defect or decoder defect. Do not close this review yet: Finding 9 is still open in the supplied snapshot, and the legacy half of Finding 8 has no reported rerun. I ran nothing, so all results below are your reported evidence.

## Prior findings and questions

| Item | Status | Assessment |
|---|---|---|
| **6. Disk thrashing** | Resolved in source | Back-references now read only the history at offset 12288, and output goes strictly forward through `cc_put`. A record cannot reference an earlier record because `distance>done` is rejected and `done` starts at 0 for each record. A 4096-byte distance reads the old byte before overwriting that slot. Polling and bounds checks are preserved. With a 7935-byte patch at offset 0, there is no overlap. About 92 flushes matches 11310/128 plus the four record starts. The 92 and the timing figures are reported values, and their report is missing (Finding 10). |
| **7. Standalone shrink** | Resolved | The gate now asserts top-level `passed`, derives its cases from labels, rejects an empty or unknown set, and fails on a missing COM. |
| **8. Page assertion** | Resolved in source | The observer now has an explicit contract for each variant, and `zoptionalcorpus` links the separately compiled object. The legacy rerun is still a gap (Finding 11). |
| **9. Evidence binding** | **Not resolved in the supplied snapshot** | See Finding 10. |
| **Question 1. Emitter audit** | Closed | The all-`src` search shows `RT_REAL` only in `math_call`. `RT_FORMAT` and the `cc_val` and `cc_read` paths are flagged at parse time. The `RT_VAL` and `RT_INPUT_VALUE` code emitted late in `compiler_files.c` picks Real only from a job kind already flagged in `cc_file_transfer`. This also closes my remaining limit on Finding 1. |
| **Question 2. Overlay members** | Closed | `cc_block` has no overlay branch, so `overlay` always fails. `compile_project` zeroes `sources`, and no supplied production path sets `groups`. `cc` is cleared once per compile, so no supported path resets `real_needed`. |

## Established finding

### 10. Medium: the evidence index still describes the pre-fix build
- **Location:** `docs/optional-real-evidence.json`, including its copy in `changes.diff`. The claims it contradicts are in Finding 9's reconciliation in `reviews/optional-real-v1.md` and the assignment.
- **Impact:** The decoder fix, media measurements and final-source gates cannot be traced to this checkpoint. The evidence file presents a passing result for a different binary.
- **Evidence:**
  - `library_bytes` and the startup report's `production_library_bytes` are 201639, while `docs/optional-real.md` and the assignment state 201725. The startup report ties that 201639 size to `build/zpascal.zpl` hash `5aeeb096...`, so the recorded production artifact is not the current build.
  - `CMakeLists.txt` is still `d4ded9a7...`, the hash quoted in my second turn, although `post-review.diff` changes that file.
  - There is no `media` report, and `tools/test-optional-media.py` is not hashed.
  - There are no file, path or Ctrl-C workflow reports, and no hash for the emitter audit.
  - `clean_zde_build_byte_identity` still contains only two booleans, with no logs, config, source or artifact hashes.
  - Capture paths still carry the earlier run timestamps, such as `20261105T1726...`.
- **Limit:** A packaging error could explain this, but `changes.diff` shows the same content, so I cannot treat it as one.
- **Fix:**
  1. Regenerate the index from the final captures, keeping historical 17-case and CHN evidence in a clearly separate section.
  2. Verify that its source and artifact hashes match f968c6c.
  3. Include the media report, workflow reports, build records and audit hash.

## Verification gap

### 11. Low: no reported legacy rerun of the changed shared fixtures
- **Location:** `tests/native/main.c`, `tests/native/harness-execute.c`, `tests/native/code-capture.c`, and the `zp_core` and `unit_core` targets that link them.
- **Trigger and impact:** `main.c` and `code-capture.c` are shared with `zptest` and `zunitcorpus`, and `harness-execute.c` also serves the other `test_execution` targets. The assignment reports the optional corpus, which proves only the `code_required-1` branch. The legacy `code_required` branch has no reported execution.
- **Fix:** Run `tools/test-native.py --core-only` on the final source and bind the capture. If the unit fixtures stay in the milestone's claims, also run `zunitcorpus`.

## Question

1. **`PAGE_SIZE` in the `#if` guard:** `zp.h` was not supplied. `#if REAL_PATCH_BYTES > REAL_HISTORY_OFFSET` works only if `PAGE_SIZE` is a preprocessor macro. If it is an enum, the preprocessor treats it as 0 and the guard can never fire. The packer's `0x3000` check still protects the build, so this affects only the claim that both layers enforce the limit.

## Review limits

- Your historical/current separation is reasonable. The history fix changes only the shared decoder and packer checks, and selection did not change. One current Real byte-identity pass exercises the same fixed patch stream that every Real and opaque non-CHN program uses, and CHN skips decoding. This holds only once Finding 10 binds that rerun.
- I did not inspect build outputs, captures, `zp.h`, `compiler_artifact.c` or the SDK's `zos_add_outputs`.
- Hashes were compared only with each other and with the supplied text.
- Generated execution-page savings remain unclaimed, user `.ZPU` integration remains separate, and the diff shows no change to original CP/M or third-party sources.

### Codex reconciliation

10. **Accepted and corrected.** The generated final-source evidence had accidentally been written to `build/zpbootfail.banks.json`, leaving the tracked index on the earlier build. That misplaced local file was preserved for audit and the corrected index was written to its intended path, re-opened, and checked against actual files. The current root measurement is201725 bytes and the production ZPL hash is320b7786471be1c200b59845833d632ea76332ac0f9e3c722f8947910190d456. The index now includes media profiles (92 authenticated flushes on both TF and EEPROM), final-source workflow captures, build log/config hashes, emitter-audit hash and the separate historical17-case/CHN section. All516 implementation/fixture files were compared byte-for-byte with f968c6c; implementation and artifact hashes and final-source capture bindings were then independently recomputed and verified. This correction changes evidence only. The prior reconciliation's claim that the tracked index already contained those records was incorrect.
11. **Accepted and verified.** Rebuilt zptest and zunitcorpus through ZDE. The full legacy core run (`tools/test-native.py --core-only`) passes2109 assertions with zero failures and authenticated input/completion. The unit corpus passes1982 assertions across15 families with unchanged expected counts. Both capture reports and their hashes are bound in the final-source evidence index, alongside the final legacy/unit artifacts. This verifies the explicit legacy cache contract and the shared main/code-capture fixtures in both variants.

**Guard question resolved.** `src/zp.h` defines `PAGE_SIZE` as the preprocessor macro `#define PAGE_SIZE 16384u`, so the native compile-time overlap check is active. The definition and header hash are included in the evidence index.

**Final source assessment.** The extra Opus pass found no decoder defect or code blocker. Codex verified the corrected evidence and completed the requested legacy/unit regressions. No implementation source changed after f968c6c or during/after the extra pass. All technical findings are reconciled. The evidence correction and added regression records were completed after the pass and were not re-reviewed by Opus. Formal runner finalization is not claimed because its frozen evidence input changed. No further Opus invocation was started.
