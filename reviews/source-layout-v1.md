# Review: source-layout-v1

## Opus turn 20261108T234334-f644ec48

Mode: code; read-only

# Source-layout stage 1: initial review

**Result:** I found no blocker or high-severity defect in the supplied stage 1 diff, and this is not a readiness claim. The bank-relevant `COMPILE_OPTIONS` assignments and object-list order in the supplied CMake diff map one-to-one. The include rewrites I checked also match `plan.json`.

## Established defects (stage 1)

**1. Low: `tools/test-examples.py`, `compiler_digest`**
- **Trigger and impact:** The recursive inventory still hashes `path.name`. Moving a file between directories without changing its sort position leaves `compiler_sources_sha256` unchanged, so the manifest no longer binds source location.
- **Evidence:** `compiler_digest.update(path.name.encode() + ...)` now runs over `rglob('*')`, and the tree already has duplicates such as `codegen/emit.c` and `link/emit.c`.
- **Fix:** Hash `path.relative_to(ROOT/'src').as_posix()`.

**2. Low: `src/compiler/codegen/builtins/crt.c`, screen-engine include**
- **Trigger and impact:** `"../libraries/screen-engine.inc"` is no longer correct relative to the file. It resolves only through `-Isrc` (`src/../libraries`), so a non-Units target without `-Isrc` would fail.
- **Evidence:** The include is unchanged while the file moved three directories deeper.
- **Fix:** Use `../../../../libraries/screen-engine.inc`, or add an explicit project-root include path.

**3. Low: `tools/check-startup-banks.py`, object inventory**
- **Trigger and impact:** Moving to `link.txt` correctly avoids stale objects, but it also adds `unit_fragment_core`, which the old five-target list did not scan. Its objects are absent from `allowed` and `providers`, so any cold-bank reference or definition now fails.
- **Fix:** Keep the broader scope if intended, record it as a deliberate checker change, and run the checker on the final link.

## Verification gaps (stage 1)

**4. Medium: `cmake/Tests.cmake` and renamed host commands**
- **Trigger and impact:** Identity from the default build will not exercise test-only targets such as `zcompacttest`, `zfragmenttest`, `unit_core`, and `optional_real_core`.
- **New `-Isrc` dependency:** Rooted includes such as `compiler/jobs/job.h` in `src/banks.c` previously resolved as siblings. Every test target compiling `banks.c`, `image.c`, or `platform.c` now needs `-Isrc`, and the full `Tests.cmake` was not supplied.
- **Reported evidence gap:** No results were supplied for `test-native-host`, `test-staging-limits`, `test-linked-addresses`, `test-compiler-messages`, `test-fragments`, or either bank checker.
- **Verification:** Build the affected ZDE test targets and run these commands before relying on them as migration evidence.

## Stage 2 plan risks

**5. Medium: targets without `-Isrc`**
- **Trigger and impact:** `ide_paths`, `real_compiler`, `real_runtime`, `key_runtime`, and `compact_integer` declare no `src` include directory in the supplied CMake. Rewriting their includes to forms such as `common/path.h` or `common/zp.h` would fail unless an include path is set elsewhere.
- **Fix:** Add the include directory to each target, or keep valid sibling includes. Check the host commands for chain, transfer, and file runtime as well.

**6. Medium: assembler include paths, needs ASM contents**
- **Trigger and impact:** `transfer_call` depends on `build/transfer_call_data.inc`, but its target declares no include directory. If `src/transfer_call.asm` uses a source-relative path, moving it to `src/runtime/process/` breaks that path.
- **Fix:** Audit every `.include` and `.incbin` across the 69 stage 2 files, including sibling references split by the plan.

**7. Medium: stale paths can fail silently**
- **Trigger and impact:** `set_property(SOURCE ...)` on an old path can silently stop applying dependencies. Identity may catch lost bank options, but not lost `OBJECT_DEPENDS`, which only affects incremental rebuilds. Tool lists filtered by `if p.is_file()` also drop stale inputs from evidence.
- **Known flat-path consumers:**
  - `package-file-unit.py`: `ROOT/'src'/source` and its CMake dependencies
  - `check-unit-banks.py`: `src/library.h` and `src/library_phase.c`
  - `test-linked-addresses.py`, `test-runtime-layout.py`, and `test-fragments.py`
  - `generate-wide` dependencies and `Units.cmake`
- **Fix:** Add a post-move check that every `src/...` token in `cmake/`, `tools/`, and `tests/` exists, and that no baseline path from `plan.json` remains.

**8. Low: ownership direction in planned groups**
- `src/platform/banks/phase.c` would include `compiler/internal.h`. The checker also requires the `code_phase_unit_finish_begin` callers to be compiler and Unit finish roots, so this is compile-phase policy rather than a pure platform service.
- `compiler/link/link.h` includes `units/linker/fragments.h`, making the core linker depend on the Units tree.
- **Suggestion:** Move the phase code under `compiler/` or `app/`, and either give fragments a neutral link location or document this edge as intentional.

## Questions before the checkpoint

1. **Are all new paths trackable?** `.gitignore` contains allowlist-style entries such as `!/tests/test_file_unit.py`. Please run `git check-ignore -v` over all new source paths, then confirm the staged rename count.
2. **Does anything depend on SDCC module basenames?** Stage 1 introduces duplicate basenames in the link, such as `image`, `emit`, and `fragments`. Binary identity covers the output, but not tools parsing map or module names. `package-native.py` was not supplied.
3. **Do unchanged consumers still contain old names?** Please provide a repository-wide grep for the 156 baseline paths, covering docs, evidence JSON, `tests/test_file_unit.py`, `tools/screen-library.c`, and flat `src` globs.
4. **Why does `worker.c` belong in `storage/`?** The supplied context shows platform and banks dependencies, but not enough to confirm its responsibility.

## Baseline note, outside this review

`tools/test-staging-limits.py` looks for the SDK at `Zeal-8-bit-OS/sdk/...`, while sibling tools use `zeal-dev-environment/home/Zeal-8-bit-OS/...`. This may stop that test from running in the expected environment.

## Limitations

- I executed nothing. Test results and the normalization proof are taken as reported.
- Final artifact identity is still pending.
- I did not have `CMakeLists.txt`, the full `Tests.cmake`, source and ASM bodies beyond the diff, or `package-native.py`.
- The stage 2 assessment covers only the plan and visible path assumptions.

### Codex reconciliation

Stage 1 reconciliation, updated with stage 2 verification:

1. Accepted: example compiler manifest now hashes source-relative POSIX paths.
2. Accepted: CRT engine include is explicitly relative to its new directory.
3. Accepted: audit intentionally checks every linked object, including fragment-core objects. Actual map/REL definitions expose existing checker drift: chain.c and units/io/loader.c provide cold code, and units/symbols/names.c imports the loader. These objects were already in unit_runtime_core; adding fragment objects did not introduce them. Explicit sets updated; both bank audits pass. Release bytes unchanged, so no residency change.
4. Accepted verification gap: test-enabled configure and selected compact, fragment, math, main and execution target builds passed, along with the sanitized host commands and native fragment/linker checks. Adapted the request for exhaustive variant rebuilds: all 63 generated targets with project C have the src include root, all active source paths resolve, and CMake declarations are otherwise identical after normalization. unit_core/optional_real_core were not separately rebuilt; no full legacy-suite claim. Existing experimental resident overflows are outside this task.
5. Accepted: stage 2 adds project-header include roots to the five identified targets.
6. Accepted: only source ASM include is generated transfer_call_data.inc, retained verbatim and assembled from build cwd; stage 2 build will verify it.
7. Accepted: stage 2 includes CMake/source path existence and stale path scans, plus incremental header-dependency comparison.
8. Adapted: move phase policy to compiler/phase.c and parent worker services to platform/worker.*. Fragments stay with Unit implementation because they encode the ZPU fragment model consumed by final linking; document this intentional compiler-to-Unit dependency rather than introduce another linker abstraction.

Questions: all 87 new paths are staged and trackable; rename-aware diff recognizes the source moves. Module basenames are preserved in generated outputs except renamed files, and probes now identify objects by full linked source-relative paths. Full artifact identity passed for BIN, ZPL, CRT, SYSFILE, ZVB and profile against 5e1ea56. Historical docs/evidence retain their original paths; living references and executable consumers are migrated. Worker manages parent-owned child page/descriptor services, hence platform ownership.

Baseline staging SDK lookup will be checked with the existing local SDK path; no install is needed. Stage 2 verification is recorded in analysis/source-layout/verification.json. The staging SDK lookup now matches the existing local environment and the boundary checks pass. All 156 files moved; flat src file count is zero. Source bodies and six release artifacts remain unchanged. Final follow-up and post-review dispositions follow below.

## Opus turn 20261109T001324-df0a2814

Mode: code; read-only

# Source-layout final follow-up

**Verdict: not ready to close yet.** The shipping tree looks sound, based on the reported byte identity for all six artifacts and both bank audits. However, the final diff breaks two host test commands and leaves stale paths in three test tools. These fixes are confined to `tools/` and should not change the shipped artifacts.

## Established defects

**1. High: `tools/test-real.py` (`host`, `compiler_conversions`) and `tools/test-compact.py` (`integer_parity`)**
- **Trigger and impact:** These commands compile `real.c` with no include path. `real.c` and `transcendental.c` now include `runtime/numbers/real.h`, which does not resolve relative to their directory. Real host vectors, compiler-conversion checks and integer parity will stop with a missing-header error.
- **Evidence:** Only `sanitize()` in `test-real.py` passes `-I<root>/src`. In `integer_parity`, `flags` is either `-DZP_INTEGER_RUNTIME` or empty. No result for these checks appears in `verification.json`.
- **Fix:** Add `'-I'+str(ROOT/'src')` to all three compile commands, then run `test-real.py` host mode and `test-compact.py`.

**2. Medium: `tools/test-native.py`, `main()` input list**
- **Trigger and impact:** The rename rewrote a test fixture name, producing `ROOT/'tests/native'/'editor/ide.c'`. That file does not exist, and `if p.is_file()` silently drops it, so `tests/native/main.c` disappears from the report's input hashes.
- **Fix:** Restore `'main.c'` in that list.

**3. Medium: `tools/test-overlays.py` (`native`) and `tools/test-module-unpack.py` (`native`)**
- **Trigger and impact:** Bare names joined to `ROOT/'src'` were missed. `test-overlays.py` still names `overlay.h` and `chain.h`, and hashes every input with no `is_file()` filter, so native mode should fail on the missing paths. `test-module-unpack.py` still names `chain.h`; its hash line was not supplied, so the result is either a failure or a silently lost input.
- **Evidence:** `stage2-path-audit.json` reports no stale paths, so its scan does not cover names built this way.
- **Fix:** Use `runtime/process/overlay.h` and `runtime/process/chain.h`, then extend the audit to `ROOT/'src'/name` joins.
- **Baseline note:** `test-overlays.py` also lists `sha256.c`, `sha256.h` and `sha_z80.asm` under `src/`, but none are in the 156-file inventory. Native mode was probably already broken, so correct or remove those entries in the same edit.

**4. Low: `tools/check-startup-banks.py`, expanded `allowed` and `providers`**
- **Trigger and impact:** The comment and reconciliation attribute the new entries to scanning linked fragment objects. But `chain.c`, `units/io/loader.c` and `units/symbols/names.c` belong to `unit_runtime_core`, which the old checker already scanned. This looks like existing checker drift, and it now permits a new cold import from `names.c`.
- **Fix:** Correct the comment and record the `names.c` import as an existing condition that was reviewed. Byte identity shows no residency change, but it does not show the import is desirable.

## Disposition of prior findings

| Prior item | Status |
|---|---|
| 1. Manifest path hashing | Resolved |
| 2. CRT relative include | Resolved; the new path reaches the repository root |
| 3. Linked-object audit scope | Resolved, except the rationale in finding 4 above |
| 4. Test-target and host coverage | Partly resolved; see finding 1 and residual risk |
| 5. Missing `src` include roots | Resolved in CMake; not in the host commands from finding 1 |
| 6. ASM include paths | Resolved; the reported artifact identity covers the retained `transfer_call_data.inc` include |
| 7. Stale paths | Partly resolved; see findings 2 and 3 |
| 8. Phase, worker and fragment ownership | Resolved; the fragment dependency is documented |
| Module basenames, worker ownership | Answered |

## Residual risk and notes

- `unit_core` and `optional_real_core` were not rebuilt, and the full legacy suite was not run. Their declarations look correct, but building those two object targets would directly close this gap.
- The "all 63 project-C targets" claim needs narrower wording. `zpbootfail`, `zpexecchild`, `zchainreplay`, `zfilegatetest`, `zchaincallparent` and `runtime_object_probe` compile test C without a `src` root. Their sources are not in the include rewrite, so I see no defect.
- `src/compiler/phase.c` now matches `src/compiler.*\.c$`, so it gains four extra `OBJECT_DEPENDS` entries. This affects incremental rebuilds only and is harmless, but it is a small semantic difference from "exactly baseline after renaming."
- `.gitignore` adds `!/reviews/source-layout-v1.md`. Confirm with `git check-ignore -v` that no earlier rule excludes `reviews/` itself, since Git cannot re-include a file under an excluded directory.

## Limitations

I executed nothing, so all build, identity and test results are as reported. I did not have the bodies of unchanged tools, or the full `test-module-unpack.py`. No hardware testing is inferred.

After these fixes, rerun the affected commands and update `verification.json`. Under the project's two-turn review limit, another Opus round would require your request.

### Codex reconciliation

All four final findings accepted and addressed; the final reviewed snapshot itself was not approved, and fixes below were locally verified after the two-turn budget:

1. Added the src include root to both Real host compile commands and compact parity. Real host passed 59,758 vectors, all 65,536 Integer conversions and 2,033 decimal conversions. Compact passed 3,959 parity cases, four allocation rejections, four file-binding rejections and 787 native assertions. Running it exposed existing fixture/callback drift: the temporary fingerprint fixture now includes SYSFILE payload, rejection checks identify the current file-asset validation layer, and the input observer accepts/validates the current image argument. Invalid assets remain rejected before publication; exact instruction/header and input receipts remain checked.
2. Restored tests/native/main.c in the input inventory. Expanded static path verification covers 137 directly/computed ROOT source/fixture path pairs, including an explicit main.c membership check; none are missing. All 60 host unit tests pass again. Full test-native.py family was not rerun.
3. Corrected computed overlay/unpack header paths. Removed the three nonexistent, unused native SHA inventory entries; the actual native target uses no SHA implementation. Both targets built and now run through hashing, execution and unchanged-input checks. This exposed existing oracle/count drift: native overlay now matches its existing host oracle for the intentionally modified trusted byte (3,372 assertions, zero failures). Unpack records the already computed free-page count and derives an exact expected count from fixture lengths and per-block checks (1,328 actual/expected, zero failures). MMU/IX/IY, full cases, suffix data, and the independent deliberately broken-restore negative control still pass; no check was removed.
4. Corrected the audit comment and attribution above. Reviewed cc_units_load in units/symbols/names.c: cold loading occurs only after code_phase_unit_link_begin/code_phase_unit_io and is released via io_finish before the retained namespace resumes. This import is expected during Unit compilation, not during IDE startup; it predates this refactor. Both bank audits pass; shipping bytes and bank placement are unchanged.

Residual notes: 63 counts only targets compiling C from src/, not test-only C targets. unit_core and optional_real_core were not separately rebuilt; their include/dependency paths were audited. compiler/phase.c gains four conservative header dependencies because it now matches the existing compiler path regex; no old dependency is lost. The review file is tracked and its parent reviews/ is explicitly included in .gitignore. All 156 src/ bodies remain identical after path/include normalization, and all six release artifacts remain byte-identical to 5e1ea56. Full legacy suite and new physical hardware runs are not claimed; previously recorded experimental overflows remain outside scope.

No further Opus turn was requested. Current corrections have local verification, not a renewed Opus verdict. See analysis/source-layout/verification.json for the recorded evidence.
