# Review: main-units-integration-v1

## Opus turn 20261108T135917-44326b45

Mode: code; read-only

# Review of `main-units-integration-v1`

I found **two blockers that prevent a passing example report or TF package**, plus one high-severity provenance gap. All findings come from static reading; I ran nothing and have no native report to assess.

## Established defects

**1. Blocker: `tools/package-hardware.py`, `main()`, example loop, `source = (ROOT / entry['source']).resolve()`**
- **Trigger and impact:** The packager raises `ValueError` on the first program in `examples/binaries.json`, so no TF directory can be produced even with valid reports.
- **Evidence:** `test-examples.py` writes `'source': str(source.relative_to(ROOT / 'examples'))`, for example `lighthouse/lighthouse.pas`. The packager resolves that to `ROOT/lighthouse/lighthouse.pas`, then calls `.relative_to(ROOT/'examples')`. The include path correctly uses `ROOT / 'examples' / include`.
- **Fix:** Use `ROOT / 'examples' / entry['source']`. Add a host test that runs the packager against synthetic reports and a synthetic build tree, covering success and each refusal path.

**2. Blocker: `tools/test-examples.py`, first check, `word('_test_passed') == 184`, with `tests/native/examples-main.c`, `app_main`**
- **Trigger and impact:** A fully successful run gives 185 assertions, so `native_checks_passed` is false. The script then asserts before setting `passed`, so no packageable report can exist.
- **Evidence:** The final single `check(pages_used()==code_required)` became two checks.

| Stage | Assertions | Running total |
| --- | ---: | ---: |
| Setup and Lighthouse, including 4 replays and terminal checks | 38 | 38 |
| Parts Bin (chdir, prepare, 3 replays) | 23 | 61 |
| Loan | 17 | 78 |
| Concord | 11 | 89 |
| Life | 11 | 100 |
| Tasks | 11 | 111 |
| Calc | 17 | 128 |
| Queens | 17 | 145 |
| Measure, through the final Stats example | 38 | 183 |
| Old final check | 1 | **184** |
| New final checks | 2 | **185** |

- **Fix:** Expect 185, then confirm it against the real report.

**3. High: `tools/package-hardware.py`, compiler and ZVB provenance**
- **Trigger and impact:** The package can pair a shipped compiler or ZVB programs with a build that the reports did not test. The manifest would still label them as production-suite output.
- **Evidence:**
  - The launcher check only proves `zpascal.bin`, `zpascal.zpl` and `zpascal.banks.json` match each other.
  - The example report hashes `zexampletest.*`, not `zpascal.*`, and the `zexampletest_bin` target does not rebuild `zpascal`.
  - The ZVB report already includes `zzvbtest.bin`, `zzvbtest.zpl` and `CRT.ZPU` in `fixture_sha256`, but the packager never compares them with the current build.
  - The ZVB report has no `src/` hashes. Only client sources, includes and `ZVB.ZPU` are compared, so an old passing report paired with its old `build/ZVB.ZPU` is accepted.
- **Fix:**
  - Compare the ZVB compiler and `CRT.ZPU` hashes with current `build/` files.
  - Record `src/`, `CMakeLists.txt` and runtime input hashes in the ZVB report, then require them to match the example report.
  - Hash `build/zpascal.{bin,zpl,banks.json,map}` during the verified run, or otherwise bind them to the same source state, and check them at packaging.

**4. Medium: `tools/test-native.py`, `line_layout` and `ready`**
- **Trigger and impact:** If a run chunk expires while `publish_bin` is writing the `.bin`, `input_layout()` sees an empty or partial image and raises `assembled input observer layout changed`. That aborts input delivery with a false failure.
- **Evidence:** `line_layout` parses any existing file, and `ready` now calls it around every chunk. `publish_bin` truncates, then writes 128-byte blocks. A short partial file also takes the compact-layout branch.
- **Fix:** Refresh only after the header extent matches the file length and the image is complete. Until then, retain the prior layout. Add a truncated-image host test.

**5. Low: `tools/package-hardware.py`, manifest `checkpoint`**
- **Trigger and impact:** Packaging from the current dirty tree records `git rev-parse HEAD`, a commit that does not contain the tested harness or packager. Physical results could then be attributed to the wrong checkpoint.
- **Fix:** Record a dirty flag and a tracked-tree digest, or refuse to package a dirty tree.

**6. Low: `docs/hardware-testing.md`, packaging workflow**
- **Trigger and impact:** Following the documented steps does not produce an accepted ZVB report or the required library.
- **Evidence:**
  - The plan's `test-zvb.py --skip-hardware --clients --disk-snake` uses the stock `SMOKE.PAS`, which the packager rejects because it expects `crt-guide.pas`.
  - `build/ZVB.ZPU` has no CMake target in the supplied build, and the docs omit `--output-library`.
  - "Keep the ZIP manifest" is stale because the deliverable is now a directory.
- **Fix:** Document the exact command, including `--source SMOKE tests/native/crt-guide.pas --output-library build/ZVB.ZPU`, and update the ZIP wording.

**7. Low, apparently pre-existing: `tools/test-examples.py`, non-`--package` provenance comparison**
- **Trigger and impact:** The planned `tools/test-examples.py` fails after `examples/binaries.json` is committed.
- **Evidence:** The manifest includes `compiler_commit` from the current `HEAD`, and a committed file cannot contain the hash of its own commit. The script asserts equality before setting `passed`.
- **Fix:** Exclude `compiler_commit` from the equality check, or compare source and artifact hashes instead.

## Questions

1. Do `test-runtime-objects.py`, `test-unit-phase.py` and `test-unit-docs.py` need build outputs beyond the preflight targets? They have no `native_targets` entry, and `runtime_object_a` and `runtime_object_b` may be required on a clean build.
2. Does `zvb-main.c` verify `CRT-GUIDE-OK` for the `SMOKE` override? `test-zvb.py` has no such check, so the shipped `CRTCHECK.COM` may only be proven to compile and exit successfully.
3. Does ZShell run the bare `measure.bin` after `cd`, as the README instructs? The supplied tests only launch programs by explicit paths such as `H:/...`.

## Limitations

- **Missing context:** I did not have `package-native.py`, `package-screen.py`, `zvb-main.c`, the newly registered test scripts, or `examples/binaries.json`. I therefore could not confirm the `_code_version` and bank metadata assumptions.
- **Native behavior not established:** Lighthouse screen and paint expectations, the 15-warm-bank count, and all emulator results still need real reports.
- **Follow-up evidence needed:** Please supply passing example and ZVB reports, plus a packager run after these fixes. Physical-board status remains pending.

### Codex reconciliation

All seven findings accepted and addressed:

1. Resolve example sources under `examples/`. Added 13 host packager tests covering successful construction and refusal paths.
2. Correct expected native assertions to 185. The fresh general example report `analysis/native-slice/20261108T140813.258566Z-1046/examples-report.json` passes 185 assertions with zero failures and 139 physical input lines.
3. Bind production launcher/banks/map, runtime inputs, compiler sources, fixtures and stock Units to reports. Packager compares current hashes, shared inputs, captured implicit SYSFILE, ZVB unit and each tested executable. These checks prevent mixing reports and builds.
4. Retain the previous input layout until a complete ZP80 image has been published. Permanent host regression covers truncated publication.
5. Refuse dirty worktrees. A clean integration checkpoint precedes package construction.
6. Document exact stock ZVB authoring, client/CRTCHECK commands and flat directory overlay. Launch examples by absolute drive path after changing to their working directory.
7. Exclude only `compiler_commit` from later manifest equality; substantive source/build/runtime/binary hashes remain compared, avoiding the self-commit hash impossibility.

Questions reconciled:

- Added preflight mappings for runtime-object oracle targets and the other newly registered native suites. Host test-plan counts are 93 monolithic / 167 expanded.
- CRTCHECK has executable `Require` checks: failure raises a runtime error. ZVB supervisor checks RAM success and saved execution exit status. It is exercised, rather than only compiled; no exact stdout golden is claimed. Conditional ReadKey is covered separately by CRT acceptance.
- Package instructions use an explicit executable path after `cd`.

Verification so far: 44 host tests, 5528 sanitized phase assertions, runtime-object oracle and Unit documentation checks pass. General examples compile unchanged through the production Unit compiler objects, and all native output/replay/ownership checks pass. Final runs pass: six ZVB clients, 179 assertions, saved execution with all ZPU/PAS/INC files removed, exact CRC output and 180 Snake demo frames; CRT 502 assertions plus standalone Chain and exact output. The actual packager created `build/tf-acceptance-review` at checkpoint `5d787b6`, with 88 files and all provenance checks passing. `docs/main-integration-evidence.json` records hashes and report paths. Physical acceptance remains unperformed.

## Opus turn 20261108T143342-573a8a39

Mode: code; read-only

# Follow-up review of `main-units-integration-v1`

**I found no remaining blockers or high-severity defects, so the overlay can proceed to physical acceptance.** Three low-severity provenance and verification gaps remain. None requires rebuilding the payload unless the check in finding 1 fails.

I ran nothing. I cross-checked the supplied reports and manifest by reading them:
- The launcher, bank file, stock Units and six ZVB executable hashes agree across the reports and manifest.
- The ZVB source hashes agree, including `crt-guide.pas` as `CRTCHECK.PAS`.
- The manifest lists 87 payload files, plus `MANIFEST.JSON` for 88 in total.
- All 10 general BIN entries are present.

## Disposition of initial findings

| # | Finding | Status | Basis |
|---|---|---|---|
| 1 | Example source path | Resolved | The packager now uses `ROOT/'examples'/entry['source']`, a host test covers it, and a real package was built. |
| 2 | Expected assertion count | Resolved | The expected count is 185, and the report shows 185 passed and 0 failed. |
| 3 | Compiler and ZVB provenance | Mostly resolved | Reports now hash `src/` and `zpascal.*`, and the packager compares them with each other and the current build. See findings 1 and 2 for the gaps. |
| 4 | Partial publication | Resolved | `line_layout` keeps the previous layout until the header extent matches the file length, with a regression test. |
| 5 | Dirty checkpoint | Resolved | The packager refuses a dirty worktree. |
| 6 | Packaging documentation | Resolved | The docs give exact authoring, client and packaging commands, and the ZIP wording is gone. |
| 7 | Self-referencing commit | Resolved | Only `compiler_commit` is excluded from the equality check. |

Prior questions 1 and 3 are closed by the runtime-object target mappings and the absolute executable paths. Question 2 is partly closed; see the first question below.

## Remaining established gaps

**1. Low: `tools/package-hardware.py`, `ZVB.ZPU` authoring provenance**
- **Trigger and impact:** The final client run uses `--prebuilt`, so its report proves which library was tested, not which compiler authored it. An older `ZVB.ZPU` could pass packaging if it still works with the tested clients.
- **Evidence:** The packager compares `build/ZVB.ZPU` only with `library_sha256` from the client report. The authoring report is listed in `docs/main-integration-evidence.json`, but the packager does not read it. Its 13:54 timestamp also predates the first review, whose `test-zvb.py` did not record `input_sha256`.
- **Verification:** Before relying on the overlay, confirm that the authoring report's `library_sha256` is `1174eab6…` and its `zzvbtest.bin` and `zzvbtest.zpl` fixture hashes match the client report. A future packager could take this report as a required input.

**2. Low: `tools/run-tests.py`, `native_targets`, stale `zpascal` risk**
- **Trigger and impact:** A focused run of `test-examples` or `test-zvb` can hash an old `build/zpascal.*` as tested, because those entries build only `zexampletest_bin` or `zzvbtest_bin`. The packager would then accept that launcher and bank pair.
- **Evidence:** Report hashes prove `zpascal.*` has not changed since the test, not that it was built from the hashed sources. The documented command avoids this by building all three targets. The current bank sizes are also consistent with shared objects, but that is not proof.
- **Fix:** Add `zpascal_bin` to both target mappings in a later change.

**3. Low: shared input observer not rerun on legacy suites**
- **Trigger and impact:** `DebugConsole.physical_line`, `ready`, `observe` and `matches` changed, but the supplied evidence exercises physical input only through the general examples.
- **Evidence:** Static risk is low because `identity()` already called `input_layout(image)`. The assignment confirms that the full historical suite was not rerun.
- **Verification:** Run `python3 tools/run-tests.py --only native:basic --only native:read --only input` before merging to main. This need not delay board testing.

## Questions that do not block testing

1. **Does step 5 of `docs/hardware-testing.md` match `CRTCHECK`?** It expects all 16 routines and `CRT-GUIDE-OK`, but no stdout is checked and the reconciliation describes `ReadKey` as conditional. `crt-guide.pas` was not supplied, so I cannot confirm either claim. On the board, record the exit status and actual output, and do not fail the step solely because `ReadKey` was not exercised.
2. **Does the standard host-test entry point run `tests/test_hardware_package.py`?** `tools/test-native-host.py` was not supplied, and `run-tests.py` does not list that file among its frozen inputs.

## Closing the final package

Regenerating the directory will change `checkpoint`, and therefore the manifest hash recorded in the evidence file. Compare the `files` map from the final manifest with the review manifest, and record the final checkpoint outside the tracked evidence file. Otherwise, recording it would create another commit.

## Physical risks, not implementation blockers

- **Timing:** Snake's collision wipe is known to be slower than C, and timing parity is not established. Graphics waits also depend on a running display clock.
- **Storage:** The example and ZVB suites ran from HostFS `H:/`. Load time, Unit lookup beside `T:/lib` and compile-and-save on a real card remain untested.
- **Video:** CRT helpers map VRAM with interrupts disabled, and their real-board timing and input latency are unmeasured.
- **Input and audio:** Human-speed PS/2 typing, audible ZVB output, CRC and DMA on the real board, and optional SNES input all need physical results.

## Limitations

`crt-guide.pas`, the authoring report, `test-native-host.py` and the review-record file were not supplied. Test results are as reported in the supplied artifacts and assignment. The overlay remains correctly marked `physical_hardware_passed: false`.

### Codex reconciliation

Follow-up accepted: no blocker or high-severity defect remains for the requested physical-test overlay. No source changes were made after the follow-up.

1. Authoring provenance verified directly: the authoring report records ZVB.ZPU SHA256 `1174eab677a92d8bf420f55d383791cefd98d1236f7fada5deb15dbb67a65e23`, identical to the client report and shipped Unit. Its `zzvbtest.bin` SHA256 is `fdd19cb8947cf55ba37427524ad0ecdc86c0cf9005982511babed428cc43a922` and `zzvbtest.zpl` is `2ff84280aeb97b5fa086a1a18c087f928d67554d18306204b59bf951874dcf8c`; both match the client report exactly. Authoring completed with 28 assertions and no failures. Requiring this report in future packager invocations is deferred as an automation improvement; the present payload is independently bound.
2. Accepted stale-product risk for focused standard-runner selections. This packaging task explicitly built production `zpascal_bin` alongside the Unit harnesses before freezing artifacts, following the documented command. Add `zpascal_bin` to the focused example/ZVB preflight mappings in a subsequent maintenance change. This does not block this verified payload.
3. Accepted legacy observer regression gap. Before merging Units into main, run `python3 tools/run-tests.py --only native:basic --only native:read --only input`. It is deferred from this hardware-package delivery: current native examples provide 139 physical input lines and the partial-publication host regression passes. No merge to main is claimed or performed.

Questions answered by local inspection:

- `tests/native/crt-guide.pas` ends with `WriteLn('CRT-GUIDE-OK')`; `Require` divides by zero on a failed cursor check. It calls the CRT operations, with ReadKey conditional on KeyPressed. The hardware operator should record the marker and exit status, and exercise ReadKey with the separate interactive BoardCRT fixture. The checklist's wording about all16routines is broader than the unconditional guide path; this is a documentation clarification for the next maintenance pass, not a board-test failure if no key was pending.
- `tools/test-native-host.py` selects `test_runner.py` and `test_native_chunks.py`, so it does not automatically include `test_hardware_package.py`. All13packager tests were explicitly run and passed in the44host tests reported here. Registration and frozen-input coverage in the standard host entry point remain maintenance work before the main merge.

Verified the real review package independently:87payload hashes match,10generalBINs,6COMdemos,88total files. The final package will be generated from the final clean review-record checkpoint; its `files` map must exactly equal the reviewed manifest. The final manifest records that checkpoint outside this tracked record, avoiding a self-referencing commit. Physical testing, storage/timing, human keyboard and audible output remain pending. The normal two Opus turns are complete; no additional review requested.
