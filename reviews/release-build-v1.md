# Review: release-build-v1

## Opus turn 20261108T183333-d1174e4a

Mode: code; read-only

## Review: release-build-v1

**Verdict: not ready to merge.** There are two blockers: stale-Unit detection doesn't run on incremental builds, and the test runner no longer builds many of the native binaries it needs. I ran nothing, and the results below come from static review of the supplied text.

### Established defects

**1. Blocker: an incremental build can ship a stale `ZVB.ZPU`**
- **Location:** `CMakeLists.txt`, `add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/ZVB.ZPU ... DEPENDS zpascal_bin runtime_profile ...)`
- **Trigger:** Build once, change a compiler or runtime source, then run `zde cmake` again.
- **Impact:** `zpascal.bin` changes, but `package-zvb.py` doesn't rerun. The build succeeds and leaves the old `build/ZVB.ZPU` beside the new compiler.
- **Evidence:** `zpascal_bin` and `runtime_profile` are `add_custom_target` targets, so `DEPENDS` creates only target-level ordering. CMake adds a file-level dependency only for executable and library targets. None of the three bound build files is a file dependency, which contradicts `docs/building.md` ("A changed binding fails the build"). `tests/test_stock_zvb.py` calls `stage()` directly, so it can't catch this.
- **Fix:** Add `zpascal.bin`, `zpascal.zpl` and `pascal_runtime.profile` as file dependencies, or make validation an always-run `zvb_library` command. Because `replace()` keeps unchanged timestamps, the file-dependency form may rerun on every build; that is cheap and acceptable.
- **Verify:** Build, modify a compiler source, rebuild, and require a "must be re-authored" failure.

**2. Blocker: the runner doesn't build many binaries that used to come from `ALL`**
- **Location:** `tools/run-tests.py`, `native_targets()`, together with `cmake/Tests.cmake`
- **Trigger:** Run `python3 tools/run-tests.py`, or a selection such as `--only math:sqrt-programs`, after a default build or in a fresh build directory.
- **Impact:**
  - In a fresh tree, binaries are missing, including likely `zmathtest`, `zfiletest`, `ztexttest`, `zedittest`, `zcorpus`, `zstdtest` and `zoptionstest`.
  - In an older test-enabled tree, those binaries aren't rebuilt, so tests can pass against stale harnesses. They are also hashed as frozen inputs, which hides the staleness.
- **Evidence:** The map covers 24 runners, while the README describes 80. Missing runners include math, files, text, editor-native and language-conformance. The non-`ALL` test targets (`zunittest_bin`, `zzvbtest_bin` and others) are all in the map, which suggests it was only ever a supplement to `ALL`. The test-only `*_bin ALL` targets now exist only when `ZP_BUILD_TESTS=ON`, yet the README says native selections "build their prerequisites".
- **Inference:** `test-plan.json` and most drivers weren't supplied, so the exact binary used by each driver is inferred from the target names.
- **Fix:** Add a `zp_test_binaries` aggregate in `Tests.cmake`, or complete the map and enforce its coverage with a test. Don't simply build `all`: that includes `zvb_library`, which should fail after a compiler change and would block the tests needed for re-authoring.
- **Verify:** Run a full plan from a clean build directory, and separately confirm an edited harness source is rebuilt.

**3. Medium: the ZVB manifest doesn't bind the compiler that authored the Unit**
- **Location:** `tools/package-zvb.py`, `record()`; authoring in `tools/test-zvb.py`
- **Trigger:** Change a source, rebuild only `zpascal_bin`, run `test-zvb.py` with the old `zzvbtest`, then record the report.
- **Impact:** `record()` accepts a Unit from the old compiler and binds it to the new `zpascal.bin`. A later default build then validates it as current.
- **Evidence:** Authoring runs `zzvbtest.bin/.zpl`. `record()` compares only `zpascal.bin`, `zpascal.zpl` and the runtime profile, and ignores the report's `zzvbtest` hashes. The docs' two-target build avoids this only by convention.
- **Fix:** Add a single authoring target that rebuilds both compilers, and record the `zzvbtest` hashes. Ideally, also reject a report whose compiler banks don't match `zpascal`.

**4. Medium: test runs silently change the product build configuration**
- **Location:** `tools/run-tests.py`, `tools/test-fragments.py`, `tools/test-low-file.py`, `tools/test-unit-corpus.py`
- **Trigger:** Run any native test, then run `zde cmake` in the same checkout.
- **Impact:** `ZP_BUILD_TESTS=ON` remains cached, so the default command builds the test targets too. `docs/building.md` documents this, but the README's unqualified product-only statement and the requested default behavior are only true until the first native test.
- **Fix:** Use a dedicated test build directory, or restore the option after the run. If neither is practical, make the runner print a clear warning and add the caveat to the README.

**5. Low: freeze and provenance checks omit relocated inputs**
- **Location:** `inputs()` in `tools/run-tests.py`; input lists in `test-zvb.py`, `test-fragments.py` and `test-unit-corpus.py`
- **Impact:** `run-tests.py` doesn't hash `units/`, so edits to ZVB sources or the prebuilt Unit during a run go unreported. The three drivers hash `CMakeLists.txt`, but `zzvbtest`, `zfragmenttest` and `zunitcorpus` are now defined in `cmake/Tests.cmake`.
- **Fix:** Add `units` to the runner's folders and `cmake/Tests.cmake` to those three input lists.

**6. Low: published Unit files get mode 0600**
- **Location:** `tools/package-zvb.py`, `replace()`
- **Impact:** `NamedTemporaryFile` creates mode 0600 files, and `os.replace` keeps that mode. This affects `build/ZVB.ZPU` and the refreshed prebuilt files on disk, and can break reads by another user or a container/host UID mapping.
- **Fix:** Set the mode explicitly, for example to 0644 adjusted by umask, and add a test.

**7. Low: some documentation still describes the old build**
- **Locations and fixes:**
  - The README says to run `tools/test-screen.py` and `test-overlays.py --native` "after `zde cmake`", but those binaries now need `ZP_BUILD_TESTS=ON`.
  - `docs/zvb-library.md` says the Zeal-native compiler "produces `build/ZVB.ZPU`". A default build now validates and stages the prebuilt artifact.

### CMake split: no defect found
- `LEGACY_CORE` restores the runtime and file assets directly after `src/native.asm`, preserving the old link order.
- `UNIT_CORE` still resolves to the core without those two assets.
- `include()` keeps source properties in root directory scope.
- The `ZP_TEST_COMPILER` loop still runs before `zlegacypascal` and later targets are defined, and it still excludes `zpascal`.
- `zexampletest` keeps banks 16–19 and `--optional-real`.

### Questions
1. Does `git grep -nE 'libraries/ZVB|libraries.*\.INC'` return anything outside the supplied diff? `examples/zvb/README.md` is the main concern, since `docs/units.md` sends maintainers to its "Maintaining the stock unit" steps. `generate-zvb-assets.py`, `test-zvb-interactive.py` and `test-compact-programs.py` should also be checked.
2. Do any selected tests read `build/ZVB.ZPU`? The runner never builds `zvb_library`, so a fresh test-only build wouldn't have it.
3. Is a hard build failure intended when a different SDCC or SDK produces a different `zpascal.bin`? Binding exact hashes would then require emulator re-authoring just to complete a release build.
4. Should `record()` accept reports with no client cases? The documented `--skip-hardware` command proves that the Unit compiled, while client validation is only a manual follow-up step.

### Review limitations
- `test-plan.json`, most test drivers, `tools/test-native.py`, `docs/release-build-evidence.json` and the binary `ZVB.ZPU` were not supplied.
- Build and test results are as reported in the assignment. The `ZP_BUILD_TESTS=ON` builds and native tests were still in progress, so test-configuration correctness remains unproven.

### Codex reconciliation

Accepted 1: validation is now an always-run stock target, ordered after compiler/runtime generation. Incremental proof pending.
Accepted 2: CMake now owns one complete zp_test_binaries aggregate over all test-only targets. Runner uses that aggregate for every selection, including ABI fixtures. It excludes production stock validation, allowing compiler changes to be tested before re-authoring. This intentionally trades focused-build speed for complete prerequisite ownership.
Accepted 3: zvb_authoring builds both compilers then records source and compiler hashes. Recording requires a matching authoring stamp captured by the native report, current source hashes, and exact zzvbtest BIN/ZPL hashes. Stock manifest preserves those author compiler hashes.
Adapted 4: preserve existing build directory and documented CMake cache behavior; print a clear warning and qualify README. Separate build directories would require wider driver changes outside this cleanup.
Accepted 5: runner now freezes units/ and cmake/; fragment, corpus, and ZVB reports include Tests.cmake.
Accepted 6: atomic publication explicitly sets readable 0644 permissions; host test verifies it.
Accepted 7: corrected focused build and stock staging documentation.
Questions: functional old paths were searched across tools/tests/docs/examples; historical evidence remains immutable. Exact compiler hashes deliberately require re-authoring with a different toolchain. Native authoring may omit client cases, but client verification remains a separate required release validation step. No new hardware package requested.
Verification so far: initial default build 39 targets/134 compile units/no test sources; six artifacts equal known-good main. Stock8, packager13, runner host tests, docs133 SDK definitions/64 registers, and 4,046,917 inline assertions pass. Initial standalone sanitized host attempt failed only on missing ABI fixture; central runner prerequisites will be used for final acceptance. Representative ON builds and native tests still running; follow-up will include final evidence.

## Opus turn 20261108T185722-b3dbb259

Mode: code; read-only

## Follow-up review: release-build-v1

**Verdict: not ready to finalize.** The design fixes for both original blockers look correct, and I found no new blocker in the product build. One new crash must be fixed, and the shipped ZVB manifest still lacks the new provenance. I ran nothing: this is static review, and the pending native, runner, incremental-rejection and OFF-rebuild results remain unproven.

### Status of earlier findings

| # | Status | Basis or remaining work |
|---|---|---|
| 1. Stale ZVB on incremental builds | Fixed in source, proof pending | `zvb_library` is now an always-run custom target ordered after `zpascal_bin` and `runtime_profile`. |
| 2. Incomplete runner builds | Fixed in source | The runner always builds `zp_test_binaries`, which excludes stock validation. See question 1 about the graph evidence. |
| 3. Authoring compiler provenance | Fixed in code, not yet in the artifact | `record()` now checks the stamp, report, sources and both compilers. See finding 2. |
| 4. Persistent test cache | Adaptation acceptable | The runner warns and the README is qualified. The three self-building drivers don't warn, which is minor. |
| 5. Missing frozen inputs | Fixed, but introduced a regression | See finding 1. |
| 6. Mode 0600 files | Partly fixed | `package-zvb.py` is fixed. See finding 4. |
| 7. Stale docs | Partly fixed | See finding 5. |

### Established defects

**1. High (new regression): `tools/test-fragments.py` crashes after its build**
- **Location:** `main()`, immediately after `artifacts += [ROOT/'build/sdk_glue.asm']`
- **Trigger:** Run the driver directly. It reaches this code after configuring with tests on and building `zfragmenttest_bin`.
- **Impact:** `artifact_paths += ...` raises `UnboundLocalError`. Even without that line, `artifact_hashes` is no longer defined, and `hashes` is overwritten with artifact hashes instead of the source and CMake input hashes.
- **Why acceptance won't catch it:** `test-fragments.py` is not in `tools/test-plan.json`.
- **Fix:** Delete the `artifact_paths` line and restore `artifact_hashes = {...}`. The earlier `paths += ...cmake` line is correct.
- **Verify:** Run the driver and confirm `report.json` has `passed: true`, with `cmake/Tests.cmake` in `input_sha256`.

**2. Medium: the shipped manifest predates the new provenance path, and staging doesn't require it**
- **Location:** `units/zvb/prebuilt/ZVB.json`; `stage()` in `tools/package-zvb.py`
- **Evidence:** The supplied manifest has no `authoring_compiler_sha256`, and `record()` is the only code that writes it. Its report hash necessarily comes from a run before this stamped flow, since the latest authoring report failed. `stage()` checks only the source and build key sets.
- **Impact:** The default build accepts a Unit with no recorded authoring-compiler identity, so finding 3's guarantee doesn't yet apply to what ships.
- **Fix:** Require non-empty `authoring_compiler_sha256` entries for `AUTHOR_BINDINGS` in `stage()`, then complete the clean authoring run and `--record-authoring`.
- **Ordering:** `package-zvb.py` is part of `authoring_sources()`, so edit it before running `zvb_authoring`.

**3. Medium: `package-tf.py` can install a stale ZVB Unit**
- **Location:** `tools/package-tf.py`, snapshot and validation of `build/ZVB.ZPU`
- **Trigger:** Change the compiler, then either let `zde cmake` fail at `zvb_library` or build only `zpascal_bin` or `zp_test_binaries`. Then run `package-tf.py`.
- **Impact:** A failed `stage()` leaves the old `build/ZVB.ZPU` in place, and the aggregate never runs validation. The script checks launcher and library consistency but not the Unit's bindings, so it can put a new compiler and old ZVB on the card.
- **Fix:** Expose a validation-only path from `package-zvb.py`. Have `package-tf.py` call it and confirm that `build/ZVB.ZPU` matches the prebuilt bytes. The documented flow records before packaging, so this fits.

**4. Low: `test-zvb.py --output-library` still publishes mode 0600**
- **Location:** `tools/test-zvb.py`, the `NamedTemporaryFile` and `os.replace` block
- **Impact:** The documented re-authoring step leaves `build/ZVB.ZPU` at 0600 until staging runs. `replace()` then needs to read and `chmod` it, which raises `OSError` and fails the build if ZDE runs under a different UID.
- **Fix:** Set 0644 before `os.replace`, as `package-zvb.py` now does.

**5. Low: the README names the wrong overlay target**
- **Location:** README overlay paragraph, "after enabling `ZP_BUILD_TESTS` and building `zscreentest_bin`"
- **Impact:** That target doesn't build the overlay harness, so `test-overlays.py --native` can still fail or use a stale binary.
- **Fix:** Name `zoverlaytest_bin`, or `zp_test_binaries`.

### Questions
1. **Was `test-enabled.json` generated from the current CMake?** Its `test_aggregate_closure` omits `zpascal` and `zpascal_bin`, yet `zp_test_binaries` explicitly depends on `zpascal_bin`. The listed `screen_library` and `file_library` are reachable through test targets anyway. Unless the script filters `zpascal`, the file predates that dependency, so regenerate it before citing it as evidence.
2. **Does any planned test read `build/ZVB.ZPU`?** This earlier question is still open. The aggregate deliberately doesn't stage the Unit, so a fresh test-only tree won't have it. The final full run from a clean build directory should settle this.

### Before final acceptance
Apply these edits before the final runs, because the runner hashes `tools/`, and `package-zvb.py` is an authoring input:

1. Fix findings 1–5.
2. Run `zvb_authoring`, native authoring with unchanged inputs, then `--record-authoring`.
3. Confirm the manifest contains `authoring_compiler_sha256`.
4. Run client validation and the full runner from a clean build directory.
5. Prove incremental rejection: change a compiler source, then require a "must be re-authored" failure from `zde cmake`.
6. Reconfigure with tests OFF, rebuild, and confirm the product-only target set and artifact hashes.

### Review limitations
I did not have the decision record beyond the supplied reconciliation, `docs/release-build-evidence.json`, `tools/test-native.py`, most test drivers, or the binary `ZVB.ZPU`. Exact-hash failure on toolchain changes is treated as the stated design choice.

### Codex reconciliation

Accepted 1: restored artifact_hashes and removed the accidental artifact_paths reference. The complete fragment driver now passes: 1,024 sanitized host assertions and 963 native assertions. Its input_sha256 contains cmake/Tests.cmake.
Accepted 2: staging requires authoring_compiler_sha256 for both author binaries. A clean zvb_authoring build and unchanged-input native run passed, then --record-authoring refreshed the shipped manifest. ZVB payload remains byte-identical to the hardware-accepted artifact (a5c11c1b...). The native report includes compiler identity and provenance stamp, and all 85 selected assertions pass, including SMOKE/GFX and saved-client checks.
Accepted 3: TF packaging calls read-only stock validation and verifies its snapshot payload hash before any image work. No TF image was generated for this task.
Accepted 4: native output publication sets 0644 before atomic replacement. Stock staging also repairs existing artifact permissions without changing timestamps.
Accepted 5: corrected the overlay prerequisite to zoverlaytest_bin.
Question 1: the earlier graph evidence predated an in-progress CMake regeneration. It was not final evidence. The aggregate approach has since been replaced: tools/test-plan.json now owns explicit prerequisites for all 80 drivers, including host ABI fixtures. Coverage tests reject a missing declaration. All declared targets resolve in the regenerated 192-target ON graph. Original commands, groups and partitions are preserved. This follows the initial review's alternative of completing the prerequisite mapping and avoids unrelated experimental probes in focused runs.
Question 2: searched all planned test drivers; ZVB tests author their own Unit, or explicitly accept --prebuilt. They do not implicitly consume build/ZVB.ZPU. Stock validation remains separate so compiler/bootstrap testing is possible before re-authoring.
Verification: actual central runner started with tests OFF, configured ON, built its explicit prerequisites, and passed the sanitized host suite and native basic acceptance. Final OFF default build passes with 39 targets, zero test sources, and all six product/runtime artifact hashes equal the original default and known-good main outputs. An actual temporary compiler-source change caused an incremental zvb_library build to fail with the expected zpascal.bin binding error; source was restored exactly and the default rebuild passed. See docs/release-build-evidence.json.
Unresolved validation: expanded build checks exposed resident-bound failures in three legacy/experimental test targets: zoptionalcorpus_bin, zunittest_bin and zunitcorpus_bin. Exploratory layout/optimization changes were reverted; compiler/runtime and native harness sources remain unchanged. Full 168-chunk acceptance is not claimed. These failures require a separate harness correction before a full-suite pass.
Review limit: two successful Opus turns are used. Final follow-up corrections, fresh manifest and complete prerequisite mapping have local verification but no third independent review. This record is not finalized and no final Opus approval is claimed. Extra rounds require an explicit user request under the project review skill.


## Follow-up implementation on main: grouped product build

The user clarified that legitimate internal component targets are desirable,
while the top-level file must be readable and unused legacy work must leave the
production build. CMakeLists.txt now contains 25 lines: setup, four production
module includes, product entry points and the opt-in Tests.cmake include.
Sources.cmake owns sources/header dependencies/bank placement; Runtime.cmake
owns numeric templates and execution assets; Compiler.cmake owns compiler
objects and BIN/ZPL linking; Units.cmake owns the three shipped Units.

The old file_runtime image/core/bin targets exist only in Tests.cmake. Production
fingerprinting no longer reads or validates that obsolete image. To preserve
file-ABI identity, SYSFILE compiles once to a neutral sysfile.payload, that
actual compiled implementation participates in the runtime fingerprint, then
the final Unit header is stamped. Invalid payloads, stale reports, non-neutral
headers and truncated fingerprints cannot publish an output. SDK/header/tool
inputs are explicit build dependencies. Project-local SDK output wrappers add
an explicit linker dependency, so component exclusion from ALL does not rely
on global default-target ordering. No installed SDK source was modified.

Native ZVB authoring explicitly requests only its modern base compiler images;
it no longer requires an otherwise unused legacy zptest image. All CMake
modules are included in relevant input/provenance hashes. Stock ZVB was freshly
authored and recorded against this compiler; its contents differ only in the
fingerprint/checksum fields. Compiler/runtime C and assembly remain unchanged.

Verification: final product-only default build passes, with four product entry
points and 37 production components, zero test sources and no old file-module
outputs. Retained compiled sources, compiler options and linker options match
the pre-refactor graph. Host checks pass: 60 packaging/no-publication checks,
four SYSFILE stamping tests, ten stock tests, 27 runner tests, 13 hardware
package tests and Unit documentation coverage. Native authoring/SMOKE/GFX/CRC32
passes 116 assertions, including direct file I/O and saved-program execution.
BIN/ZPL sizes remain 16,115 / 276,741 bytes. Fingerprint changes deliberately
update their hashes and all stock Unit headers. See the grouped_build_followup
section in docs/release-build-evidence.json for current artifact hashes.

Review status: these follow-up changes have local verification only. The two
Opus turns above remain the review budget used; no additional review approval
or full legacy-suite pass is claimed. Further Opus rounds require the user's
explicit request under .agents/skills/opus-code-review/SKILL.md.
