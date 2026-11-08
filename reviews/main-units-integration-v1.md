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

Verification so far: 44 host tests, 5528 sanitized phase assertions, runtime-object oracle and Unit documentation checks pass. General examples compile unchanged through the production Unit compiler objects, and all native output/replay/ownership checks pass. Final ZVB/CRT run results and actual package verification are supplied in the follow-up. Physical acceptance remains unperformed.
