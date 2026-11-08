# Review: premerge-validation-v1

## Opus turn 20261108T163242-9e50059c

Mode: code; read-only

No blockers. Findings 1 and 2 should be resolved or explicitly scoped before the follow-up evidence is treated as complete.

## Established defects

**1. Medium: focused input runs can use a stale production compiler**
- **Location:** `tools/run-tests.py`, `native_targets()` entry for `tools/test-input.py`; `tests/test_runner.py`, `test_legacy_input_regressions_build_their_harnesses`
- **Trigger:** `run-tests.py --only input`, or any selection without a `test-native.py` command.
- **Evidence:** The mapping builds only `zinputtest_bin`. However, `test-input.py` calls `n.user_break_checks()`, which reads `symbols('zpascal')` and launches `H:/zpascal.bin` through `program_launch()`. `execute()` also unconditionally copies `build/zptest.{bin,zpl}`.
- **Impact:** A stale `zpascal` can drive the user-break checks, and the suite freezes its stale hash as input. This is the staleness the cleanup is meant to prevent. `input-report.json` does not bind `build/zpascal.*`, so it cannot reveal the problem. On a clean tree, the missing `zptest` files fail the run.
- **Test gap:** The regression test checks only the union with `test-native.py`, which hides the omission.
- **Related issue:** `tools/test-zvb.py` has the same unconditional `zptest` copy, but only `zpascal_bin` and `zzvbtest_bin` are mapped. A focused clean-tree ZVB run fails loudly, and `test_focused_examples_and_units_rebuild_the_product` pins that target set.
- **Fix:** Map `test-input.py` to `zpascal_bin`, `zptest_bin`, and `zinputtest_bin`, and add `zptest_bin` for ZVB. Assert each script alone, and add `build/zpascal.*` to the hashes in `test-input.py`.

**2. Medium: native GFX does not verify the end of the bank as claimed**
- **Location:** `tests/native/zvb/GFX.PAS`, `ReadVRAM($11FF00, 64)` and `ReadVRAM($11FF80, 64)`
- **Evidence:** The addresses imply 256-byte tiles in 8-bit mode (`$110000 + 255*256`). `Result` holds only 64 bytes.
  - The 8-bit check covers `$11FF00..$11FF3F`, leaving the final 192 bytes unread.
  - The 4-bit check covers `$11FF80..$11FFBF`, leaving the final 64 bytes unread.
- **Impact:** A passing native GFX report does not show that the fill reached `$11FFFF` through the public API. The CPU oracle covers offset 16128, count 256 for the Inline leaf only.
- **Fix:** After each fill, add `ReadVRAM($11FFC0, 64)` and assert all bytes. For 8-bit mode, this still leaves `$11FF40..$11FFBF` unread unless intermediate windows or a larger buffer are added. Otherwise, narrow the evidence claim to the bytes actually read.

**3. Low: HelloWorld acceptance lacks artifact and compiler identity**
- **Location:** `docs/snake-hardware-acceptance.json`, `compiler.hello_world_*`
- **Trigger and impact:** The entry sits under the file-level `retest_checkpoint`, implying that exact compiler was tested, but no supplied evidence establishes this. The nearby `life` record explicitly says what was not verified, while this entry has no equivalent limits.
- **Fix:** Add the known compiler or package identity. Otherwise, record the compiler checkpoint as unverified and add fields such as `physical_source_hash_verified: false` and `com_sha256_captured: false`.

## Questions requiring missing context

1. **Are all other `test-native.py` build inputs covered by the new target list?** `--case allocation` reads `build/zpbootfail.{bin,map}`, but `zpbootfail_bin` is not listed and `artifact_hashes()` does not cover it. If `zptest_bin` does not depend on it, allocation can use a stale harness or fail on a clean tree. `user_break_checks()` also reads `linked_integer_runtime.bin`, so that dependency needs the same check. Please supply `CMakeLists.txt`.
2. **Do `stringabi_bin`, `setabi_bin`, `pointerabi_bin`, and `realabi_bin` exist and produce the `.zpl` files that are hashed?** The follow-up build log can confirm this.
3. **Does `zpascal_bin` refresh `build/CRT.ZPU`?** `test-zvb.py` consumes that file for client runs. The fixture hash records its contents, but not its freshness.
4. **Is `tools/test-zvb-leaves.py` in `tools/test-plan.json`?** The expanded oracle is only permanent regression coverage if a standard path runs it.
5. **Will the ZVB client run use `--prebuilt` with the exact Unit?** The follow-up should show `library_sha256` beginning `a5c11...` and `prebuilt library used: true`.

## Verified from the supplied code
- Preflight order is still lock, one build, then freeze.
- The fake `zde` now enforces the `--no-tty cmake build` prefix.
- Freezing all of `tests/` excludes `__pycache__`.
- The 13 packager tests are in the host entry point.
- The VRAM loop bounds are size-derived and the new cases stay within the oracle's buffers.
- The CRT checklist wording matches `crt-guide.pas`.

## Review limitations
I ran nothing and have no native reports. `CMakeLists.txt`, `tools/test-plan.json`, `tools/package-hardware.py`, and the unmapped runner scripts were not supplied, so I could not audit whether those runners have the same stale-artifact issue. The reported 28 runner tests predate the final ABI-target addition, so `test_runner.py` needs a rerun on the final code. Merge readiness remains unestablished until the native checks finish.

### Codex reconciliation

Initial findings reconciled:

1. Accepted and fixed. Focused input now builds zpascal/zptest/zinputtest; focused ZVB and general examples also build zptest because the shared executor copies it. Tests assert each driver alone, avoiding the union masking the missing dependency. Input reports now hash both base compiler fixtures and linked_integer_runtime.bin. Legacy native mapping also includes zpbootfail_bin and all four ABI assets. The standard ZDE preflight successfully built those named targets. CMake supplies linked_integer_runtime.bin through runtime_layout -> unit_runtime_core -> zpascal; zpascal also depends on screen_library for fresh CRT.ZPU. Added stock ZPU/PAS files to frozen build inputs, alongside all host/native test sources.
2. Accepted and fixed. New RequireSolidTile reads every 64-byte window: all 256 bytes at $11FF00 in 8-bit mode and all 128 bytes at $11FF80 in 4-bit mode. The final window of both reaches $11FFFF. The helper uses the existing DMA read and Require operations; no production graphics implementation changes. The final native GFX run must pass before this evidence is considered complete.
3. Accepted and fixed. HelloWorld acceptance explicitly marks compiler checkpoint and physical source hashes unverified and the COM hash uncaptured. This remains a user-reported physical compile/run observation, not an instrumented artifact attestation.

Questions addressed: CMakeLists.txt and test-plan.json will be supplied in the follow-up. The leaf oracle is now a standard plan entry, with the existing Zeal-root/CPU-root environment convention so it works from this worktree. Plan totals become 94 monolithic / 168 expanded commands. Existing exact-prefix/build-once-before-freeze integration test and all 28 runner host tests pass on the final target mapping. Standard host entry also passes all 6 chunk and 13 package tests. Permanent CPU suite passes 4,046,917 sanitized assertions. The focused standard suite has passed its ZDE build and host prerequisite; read regression and CPU entry pass; basic/input and four native graphics clients remain in progress. The graphics command uses the exact hardware-accepted a5c11... Unit, promoted to build/ZVB.ZPU only after verifying its authoring report, compiler fixture hashes and current authoring inputs. All TF package directories remain untouched. No merge-ready claim until the required final reports pass.

### Additional reconciliation and final verification

The first focused run exposed a real completion-observer bug: earlier key delivery could already reach the supervisor HALT at FC00, then the final large run resumed to halted PC FC01 and consumed its entire budget. A diagnostic run captured repeated FC01 with the supervisor image and ultimately 95 successful input assertions. Corrected only the native CLI main/input-driver completion commands to arm FC00 and FC01; strengthened input completion with exact supervisor-prefix, PC and stage checks. No deadline increase: the official rerun passes basic135/0 in8.051s, read78/0, input95/0 in130.723s, within its original300s deadline. Shared imported emulator-helper AST is unchanged; graphics runs do not invoke native CLI main. The independent input-harness timeout is resolved without production changes.

Final standard focused suite `analysis/test-runs/20261108T170421.182335Z/suite.json`: passed-focused, all5commands, no skipped/not-started tests, no leftover processes, no changed inputs. The full168command suite is not claimed. Sanitizedhostchecks:180362front-end/identity/allocation,104+98raw-key,4010editor,45809sourcevalidity;28runner+6chunk+13packagertests;4046917VRAMleafassertions.

Graphics report `analysis/native-slice/20261108T164404.908495Z-84521/zvb-report.json` passes115/0 forGFX,ZVBCHECK,SPRITES,FIRSTBAR, all4savedclients run withPAS/INC/ZPUfiles removed. GFX checks all256/128bytes through DMA to11FFFF. UnitSHAa5c11...matches the physicalacceptedfix. Sizes32789/22077/26455/16990 respectively. Canonicalbuild/ZVB.ZPU refreshed from that verifiedUnit; no TFpackage rebuilt or changed. All4existingTFdirectory payloads still match their original manifests.

`docs/premerge-validation-evidence.json` records paths, hashes, timings and scoped proof. Final sources are frozen for the follow-up. No product compiler/runtime/library implementation was changed in this task; no main merge or push performed.
