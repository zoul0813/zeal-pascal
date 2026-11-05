# Review: compact-file-access-v1

## Opus turn 20261105T022502-3b1267be

Mode: code; read-only

# Opus review: compact-file-access-v1, first pass

**Result:** I found no blocker or high-severity defects. The bridge direction, alias arithmetic, count and buffer bounds, SDK short/error handling, IX/SP/MMU/IFF restoration on production paths, and the CMake and profile selection are correct by inspection. There is one low-severity defect, which is not reachable through the production module stack, plus test and evidence gaps.

The limited proof looks sound for its stated contract, subject to the limitations below. Findings 2 and 3 should be closed before calling it complete.

## Established defect

### 1. Low: downward stack room is not validated
**Location:** `src/file_low_copy.asm`, `_zf_low_copy`, `stack_ok`

- **Trigger:** The helper is called from a page3 stack so that its SP after saving registers is `C000..C003`.
- **Impact:** One or both `push af` frames land in page2 at `BFFC..BFFF`. After `out (0xf2),a`, the `pop af` instructions read the alias page instead. That can restore the wrong page2 mapping and the wrong IFF state.
- **Evidence:** The check covers only `SP..SP+9`, but the helper pushes 4 more bytes before remapping. Production module stacks are in page1 and start at `0x8000`, so this path appears limited to the explicitly accepted page3 case.
- **Fix:** Reject stacks unless `SP-4` is in the same page, or remove page3 acceptance and adapt the test harness. If page3 stays accepted, add native cases at SP `C003` and `C004`, the last rejected and first accepted values.

## Missing meaningful tests

### 2. Medium: the new span gates have incomplete negative coverage
**Location:** `src/file_runtime.c` `component()` and `blocks()`; `tests/native/compact-main.c`; `tests/native/file-core.c` `compact_transfers`

- **Typed path:** `component()` changed to `transfer_span`, but no native or host test rejects a typed record below `$6000` or crossing `$8000`.
- **Read path:** All invalid-span cases use `BLOCKWRITE`, including `BADMIN` and `BADCROSS`. No test shows a rejected `BlockRead` or typed `Read` leaves the destination unchanged.
- **Weak oversize case:** `count=513` at `$6000` fails the existing `size>65535` check, so it never reaches `transfer_span`.
- **Impact:** A regression in these paths could pass the current suite.
- **Verification:** Add these host and RAM/COM cases, with canaries and `model_read_calls` or `model_write_calls` unchanged where applicable:
  - Typed read and write at `$5FFF` and at a span crossing `$8000`.
  - Block read at the same two bad spans.
  - Block count 65 at `$6000`, which is 8320 bytes.
  - Block count 512 at `$6000`, which is 65024 bytes and reaches the span check.

### 3. Medium: no integration test places live high state inside the alias window
**Location:** `tests/native/compact-main.c`

- **Gap:** The design depends on the module not touching page2 state while `A000–BFFF` is aliased, and on that state being intact afterward. The fixtures do not deliberately put the file block or a data canary in that range. The `high` guard is an ordinary global with no fixed placement in the window.
- **Why it matters:** Inspection supports the design, because only module-local request and staging memory is used during `ldir`. The native proof does not directly exercise the case where a FIB and caller data share the alias offset range.
- **Verification:** Pad the globals, or use an in-bounds `absolute` placement, so the FIB and a canary fall in `A000–BFFF`. Choose a low buffer whose alias offset overlaps the canary. Run multi-chunk read and write in RAM and COM, then verify the FIB state, canary and exact file bytes.

### 4. Low: the bridge-failure branch has no fault injection
**Location:** `copy_data()` in `src/file_runtime.c`; `zf_low_copy()` in `tests/native/file-model.c`

- **Gap:** The host model never returns failure after runtime prevalidation. The `fail(0x1ff,0)` branch and the `break` paths in `transfer()` are therefore unexecuted. Corrupted control memory is out of contract, but this defensive path still claims the existing `FF` behavior.
- **Verification:** Add a model counter that fails the Nth copy. Check first-chunk and later-chunk failures for read and write, including `Actual`, position, the error value, and handle release.

### 5. Low: bridge component boundaries and register preservation are incomplete
**Location:** `tests/native/low-copy.c`, `tests/native/low-copy.asm`

The code is correct by inspection, but these claims are not asserted:
- IX, BC, DE and returned HL preservation. `test_machine` records only IFF, SP and MMU state.
- Rejection of `buffer=0x3FFF`.
- Acceptance of a staging span and a request span that end exactly at `$8000`.
- Rejection of a page1 frame whose `SP+9` crosses into page2.

## Evidence and proof authentication

### 6. Low: host and packaging counts are not hash-bound or enforced
**Location:** `tools/test-file-runtime.py` `host()`; `docs/low-file-evidence.json` `host`

- **Gap:** The doc calls the evidence hash-bound, but `normal_sanitized_assertions`, `compact_sanitized_assertions` and `normal_packaging_rejections` have no capture directory or input hashes.
- **Runner weakness:** `host()` relies only on the process exit status. A build that silently skipped `compact_transfers` would still pass, because no expected count is checked. The returned `compact_file_binding_rejections` value is also a hardcoded `4`, though each trial does assert rejection.
- **Fix:** Write a report containing exact expected counts, source hashes and the binary hash for both host modes. Count the binding trials as they run.

### 7. Low: hash checks show stability during a run, not artifact freshness
**Location:** `tools/test-low-copy.py`, `tools/test-compact.py`

- **Gap:** Both runners hash inputs at the start and compare them at the end. That shows files did not change during the run, not that `build/` was produced from the recorded sources.
- **Fix:** Make the documented gate perform the rebuild as part of one transcript, or record a build manifest binding source hashes to outputs.

## Questions

1. Is page3 acceptance needed outside `zlowcopytest`? If it is test-only, removing it narrows the helper's authority and resolves finding 1.
2. Should the `0x1FF` response for a nonzero bridge status remain in the compact experiment if `FE76` corruption is explicitly out of contract? I recommend keeping it, with finding 4's test.

## Scope classification

Automatic low placement, the allocator, low FIBs and metadata, low Text buffers, unit support, Chain composition, physical hardware and compiler peak pages are documented out-of-scope work. I did not report them as defects in this experiment.

## Review limitations

- I did not run any tests or commands, and I could not recompute any hash or inspect any capture. All pass counts and hash matches are as supplied.
- I did not have `tests/native/file-start.asm`, `tools/test-runtime-profile.py`, `src/compiler_asset.c`, the linker maps, or the SDK and kernel sources.
- MMU port behavior is inferred from the repository's existing code and tests.
- NMI behavior during the page2 alias is unassessed, because the kernel sources were not supplied. `DI` does not mask NMI, so the doc's "interrupts are masked" statement does not cover that case.

### Codex reconciliation

Accepted finding1: validate SP-4 as well as SP..SP+9 in the same retained page before aliasing. Page3 remains useful to the independent component observer; the helper now rejects C003 and accepts C004 after its saves. The production stack remains page1. Native boundary cases also check upper frame crossing and SP/MMU/IFF preservation.

Accepted finding2, with corrected arithmetic: 512*128 is65536 and exercises overflow. Use511*128=65408 and65*128=8320 for span rejection; retain the existing overflow probes. Added typed read/write and block read/write bad lower/crossing spans, unchanged SDK/copy/open counters and byte canaries in the sanitized host model. Added native RAM/COM errors and independent bytewise RAM rejected-read destination oracles.

Accepted finding3: padded native fixture fixes the FIB at A000 and high canary at A0B0, directly overlapping the6000 low buffer's alias. Multi-chunk read/write checks FIB position/size, high bytes and exact ALIAS.DAT bytes in both RAM/COM.

Accepted finding4: model_low_fail_at injects first/second-copy failures for both directions. Verify FF status, Actual and position, unchanged destination tail, write-call count, handle cleanup and independently read disk bytes. Keep the explicit0x1FF fail-closed behavior even though corrupted private control memory is out of contract.

Accepted finding5: native component now snapshots IX/BC/DE/HL, tests staging3FFF rejection, staging7F80..7FFF and request7FFA..7FFF acceptance, and upper stack-frame crossing rejection.

Accepted finding6: host runner freshly compiles each sanitized binary and enforces exact1522/12583 summaries with zero failures, hashing sources including SDK headers, binary and output into reports. Binding trials are counted as actually exercised. The master gate pins the normal packaging count63 and binds its captured log plus sources/build inputs in the manifest.

Accepted finding7: tools/test-low-file.py always configures/rebuilds all tested binaries, including the normal zpascal packaging control, through ZDE before acceptance, binds current source hashes to build outputs, captures commands/status/log hashes, and rejects changes during the gate. This establishes incremental rebuild provenance, explicitly not a clean-build guarantee. Native runner hashes continue to establish stability during individual runs.

Adapted NMI limitation: documentation now says maskable interrupts and explicitly excludes unestablished NMI behavior. No OS/NMI promise or hardware safety is claimed. No installed OS/SDK/emulator changes.

Final gate analysis/low-file-gate/20261105T030015.588685Z-96934/manifest.json passed: native583/787/1712, sanitized host1522/12583 and packaging63, with unchanged source and build artifact hashes. Full capture references and hashes are recorded in docs/low-file-evidence.json. Required follow-up will assess the fixes and remaining risks; no source edits during that review.

## Opus turn 20261105T030358-06f0dd50

Mode: code; read-only

# Opus follow-up: compact-file-access-v1, final pass

**Result:** All seven first-pass findings are resolved, and I found no blockers, regressions, or new correctness defects. The limited proof is sound for its stated contract, assuming the supplied gate results are accurate. One low-severity documentation fix remains, and it doesn't block this experiment. No production allocator, unit, low metadata, Text, or Chain support is established.

## First-pass dispositions

| # | Status | Assessment |
|---|---|---|
| 1. Downward stack room | **Resolved** | The new `SP-4` check rejects `C003` and accepts `C004`. The probe seeds reach those exact post-save SP values. `SP+9` is still checked, and `E` survives both checks. |
| 2. Negative span coverage | **Resolved, with one doc nit** | Host and native tests now cover typed and block reads and writes. Counts 65 and 511 reach `transfer_span`. The arithmetic correction was right: 512 blocks is 65536 bytes and hits the overflow check. See remaining finding 1. |
| 3. Live state in the alias window | **Resolved** | The fixture requires the FIB at `A000` and the canary at `A0B0`, and the 512-byte low buffer aliases `A000–A1FF`. A wrong placement halts before the required `HIGH-ALIAS-OK` marker. Multi-chunk transfers, position, size, canary bytes, and `ALIAS.DAT` bytes are all checked. |
| 4. Bridge fault injection | **Resolved** | First and second copy failures are injected in both directions. The test checks `0x1FF`, `Actual`, position, handle release, write count, destination bytes, and disk bytes. |
| 5. Component boundaries | **Resolved** | The test now covers IX, BC, DE, and HL preservation, staging `3FFF` rejection, and acceptance of staging and request spans ending at `$8000`. It also rejects a return frame crossing into page2. |
| 6. Host and packaging counts | **Resolved** | Host runs enforce exact summaries and record source, SDK header, binary, and output hashes. Binding trials are now counted as they run, and the gate requires the exact 63-check packaging message. |
| 7. Artifact freshness | **Resolved within its stated scope** | The gate hashes sources, rebuilds through ZDE, hashes outputs, runs acceptance, and rechecks both sets. This shows an incremental rebuild transcript, not clean-build reproducibility, and the docs say so. |

## Remaining finding

### 1. Low: the docs overstate the rejected-read byte oracle
**Location:** `docs/low-file-experiment.md`, "Required proof", negative native programs bullet

- **Evidence:** The doc says rejected reads "retain every destination byte" without saying where. `rejected_read_guard` compares bytes only after `run_build`, which is the RAM path. `checked_error` runs the same sources in RAM and COM, but it checks only the error result and location.
- **Impact:** The behavior is likely the same in COM because the module path is the same. The doc still claims a saved-COM byte oracle that the tests don't provide.
- **Fix:** Change the sentence to say bytewise destination checks are RAM-only, while RAM and COM both check the error result. A COM oracle isn't needed for this limited proof.

## Current checks

- **Bridge and runtime:** Direction, alias arithmetic, count and span bounds, and SDK short and error handling remain correct. `_zf_low_copy` remains within the module page, ending below the `$7C00` limit.
- **Size and layout:** The map excerpts match the evidence:
  - TEXT ends at `76B8`, which gives 14008 module bytes.
  - BSS ends at `7907`.
  - The test buffer and request sit in page1.
- **Default runtime:** The normal runtime and file module hashes are unchanged in the evidence, and the host, native, and packaging controls are reported as passing.
- **Evidence consistency:** Spot-checked source and artifact hashes for the changed files agree between the manifest and `docs/low-file-evidence.json`. These include the bridge, tests, runners, compact module, and test binaries.
- **Packaging count:** My count of `tools/test-runtime-profile.py` also comes to 63 checks.
- **Earlier test failure:** The `write(f,n)` oracle change is sound because that text doesn't match `rewrite(f)`. The manifest hash for `compact-main.c` matches the evidence.

## Question

The `.gitignore` additions retain the docs and runners, but I can't see whether `analysis/low-file-gate/...` is retained. If it's ignored, `build_provenance` will point to a manifest and logs that exist only locally. Should those be archived with the experiment, or is the embedded evidence enough?

## Scope classification

The automatic allocator, low FIBs and metadata, low Text buffers, units, Chain/Execute composition, physical hardware, compiler peak pages, and NMI behavior remain out of scope or unproved. The docs now say this explicitly, so I'm not treating these as defects in this experiment.

## Review limitations

- I didn't run anything or recompute any hash, including `manifest_sha256`.
- I didn't have the gate logs or capture directories, so the 583, 787, 1712, 1522, and 12583 results are as supplied. I didn't derive those assertion counts from source.
- I didn't have `image_get`'s implementation, so I inferred that it reads post-run memory from the required `0xa5` result.
- The kernel, SDK, and toolchain sources weren't supplied, so MMU behavior is inferred from repository code and tests.

### Codex reconciliation

Accepted the low documentation finding: clarified that bytewise rejected-read preservation is tested only in RAM, while RAM and saved COM both verify error status and location. This changes no implementation or test result. All seven source/evidence findings remain resolved and the final gate remains valid because none of its source/build inputs changed.

Answered the retention question: analysis/ is intentionally ignored local execution evidence, not a committed capture archive. The retained evidence JSON embeds current test reports and command/log hashes plus the local manifest reference. The experiment document now states this retention boundary and how to regenerate captures.

No unresolved code blocker remains. NMI, hardware and the explicitly excluded production/unit work remain unproved. No further review round requested or performed. The two documentation clarifications followed the final review, so the runner's unchanged-input finalization cannot be claimed for this snapshot. The code reviewed by Opus is unchanged; its no-code-blocker finding applies to that code. The review record explicitly discloses this documentation-only freshness limit.
