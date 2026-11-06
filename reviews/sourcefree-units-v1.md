# Review: sourcefree-units-v1

## Opus turn 20261106T012742-8d1c0128

Mode: code; read-only

# Initial review of `sourcefree-units-v1`

**Verdict:** I found no blockers or high-severity defects in the supplied code. There are two low-severity defects and one medium test-coverage gap, and Question 4 could turn into a real navigation defect. I reviewed by inspection only and ran nothing, so the ZDE build and all tests remain unverified.

## Established findings

### 1. Low: Refused navigation can still pin Main
**Location:** the `c=='r'` runtime-fault branch in `app_main`, plus `diagnostic` in `src/main.c`

**Trigger:**
1. Work is `H:/A.PAS` and holds a dirty client.
2. The file on disk at that path is still the matching source of an imported unit.
3. The client faults inside the unit during a Memory run.

**Impact:** Work is correctly retained, but `main_file` is set to the client path even though navigation was refused. If the user later opens another file, C or R compiles the pinned `H:/A.PAS` from disk instead of Work. That disk text is the unit source, so the wrong root is compiled.

**Evidence:**
- `build_location` succeeds because the disk text matches, so `file` is nonzero.
- `if(file&&!main_file[0])strcpy(main_file,root.path);` runs before `diagnostic`.
- `diagnostic` then rejects the buffer through `sources_matches`.
- `find_saved` already avoids this by setting Main only after `select_error_source` succeeds.

**Fix:** Set Main inside `diagnostic`, only after `located` is finally true and before `edit(1)`. Passing `root.path` is safe because `root` is a local copy that survives `build_close`.

**Verification:** Add an IDE test for this scenario. Assert that `main_file` stays empty and that the Work descriptor and text are unchanged.

### 2. Medium (test coverage): The new IDE navigation branches are not exercised
**Location:** `diagnostic` and `find_saved` in `src/main.c`, `tools/test-ide-units.py`

**Gap:** The IDE fixture covers missing and changed source only. It does not cover these required paths:

| Path | Current coverage |
|---|---|
| Matching source navigates into the unit from a real IDE fault | Native API only, not IDE |
| Same-path dirty Work is refused when disk text matches | None through `diagnostic` |
| `find_saved` same-path refusal | None |
| Dirty Work preserved across a missing-source fault | The fixture's Work is clean |
| Exact Work retention | Only the path and fault result are checked |

The native `sources_matches` assertions check the helper but not the IDE decision. Native positive tests also compile with `interruptible=0`, while the IDE uses `1`, so they cannot answer Question 4.

**Fix:** Add these IDE scenarios:
- Restore matching source, then assert the Work path, the cursor at `Crash:=`, and that the build closes.
- Use a dirty same-path Work over matching disk text, through both Run and Find.
- Compare the Work descriptor and RAM text before and after, as `test-ide-unit-failures.py` already does.

Also add a metadata case where a map offset exceeds a nonzero source length. `BADLENGTH` only exercises 65535.

### 3. Low (test quality): `source_file` overwrites without removing the file
**Location:** `source_file` in `tests/native/unit-real-main.c`

**Trigger:** The second write replaces `H:/MIXED.PAS` with the shorter `changed source` using `O_TRUNC`, without calling `rm`.

**Impact:** `src/output.c` states that the pinned HostFS "cannot reliably truncate in place". The tail of the old text may therefore survive, so the test may not exercise the intended text. The assertion would still pass because the fingerprint differs, so this is a weaker test rather than a false failure.

**Fix:** Call `rm` first and tolerate a missing file, as `seed_unit` does in `units-main.c`.

## Questions that need more context

4. **Is `cc.polling` cleared after a successful interruptible compile?** Needed: `cc_seal` or `src/compiler_artifact.c`.
   - The ordinary success path in `compile_project` does not clear it, although the unit and failure paths do.
   - `unit_source_fingerprint` calls `cc_poll`, and `build_location` now reaches it after `compile_request` has returned.
   - If polling stays set, `cc.error` points at an expired stack `Diagnostic`. A nonzero stale code would make the hash return 0, so matching source would be reported unavailable. Keyboard input could also be consumed.
   - The comment in `cc_poll` suggests polling is cleared, but I cannot confirm it. Finding 2's positive IDE test would settle this.

5. **Do any unsupplied callers depend on the old source semantics?** Needed: `src/compiler_units_link.c`, `src/compiler_disk.c`, and the definitions of `cc_sources_error` and `compile_error_path`.
   - Check for any `sources_find` call that expects text rather than a debug record.
   - Check for any assumption that a source record's base offset is zero after `sources_finish`. The new in-place loop leaves it unchanged, which is harmless in the supplied code because `page` is 255.

6. **Do any unsupplied tests pin the changed UI text?** Needed: `tools/test-compiler-messages.py`, `tools/test-error-location.py`, `tools/test-ide-sources.py`.
   - Error 251 has new wording.
   - The old `Source lookup failed. Press Escape` branch is now a cleared diagnostic screen for ordinary programs too, including a PC outside the map. I think this is an improvement, but fixed expectations may need updating.

7. **Do the docs need updating?** No docs are in the diff. Any text saying a client needs matching unit source is now stale, including the comment in `compile_work` that says imports bind matching source.

## Checked with no defect found

| Area | Result |
|---|---|
| Source catalog | `sources_unit` always appends, and the root is found first by input lookup. `sources_finish` can no longer clear the root by path coalescing. |
| Include lookup | `sources_find_input` skips debug records, and repeated includes and root self-includes behave as before. Collision checks still see all paths. |
| Failure cleanup | `read_block` always initializes `loaded` for paths of at most 127 bytes, and `sources_release` handles missing, read and free failures. A failed free is retired into the catalog and returns not-ready. |
| Artifact binding | Path length, NUL and canonical form are validated without file I/O. Checksum and length byte order match between writer and reader. |
| Metadata validation | Profile, checksum, fragment and map checks remain mandatory. Map offsets are now bounded by the metadata length. |
| Macro-off builds | Guards are consistent, and `zlegacypascal` compiles the new `diagnostic` parameter through `(void)file`. |
| Banked calls | New calls pass resident or stack pointers to the permanent bank 3. `build_location` runs in a retained bank and uses the resident `read_block` wrapper. |
| Stale builds | I found no in-IDE path that leaves a Memory build valid across a unit rebuild, because switching modes closes the build. An `X` child can change a `.ZPU` without invalidating the build. This fits the rule that the ZPU is authoritative at compile time. |
| Pass counts | `PASCAL-UNITS-OK == 7` and the new marker counts match the fixture as written. |

## Review limitations

- **Pending evidence:** The ZDE build, all native and IDE fixtures, and the 329 assertion update are still outstanding.
- **Bank capacity:** I could not measure it. Check the `BANK1`, `BANK3`, `BANK7` and `BANK15` sizes, plus cold 10, warm 15 and the fixed 16 KiB prefix, on the final binary.
- **Files not supplied:** `compiler_artifact.c`, `compiler_units_link.c`, `compiler_disk.c`, `compiler_units.c`, `unit_output.c`, `library.h` and the docs. This limits my conclusions on link-time source IDs, disk-mode collisions and sealing.
- **Source removal:** I could not confirm that nothing outside the supplied files still references the removed `sources_fingerprint`. The build will show this.
- **VRAM check:** The `4000 -> 100000` assertion assumes that mapping is already active at the `_video_vram_set` breakpoint. I could not confirm this without the kernel source, so treat a failure there as a possible fixture problem first.

### Codex reconciliation

1. Accepted. Main pinning now occurs inside `diagnostic` after successful source selection. The native IDE navigation capture `analysis/native-slice/20261106T022037.555561Z-47112/unit-navigation-report.json` passes all 15 checks, including empty Main after missing-source, same-path Run and same-path Find refusal.
2. Accepted. The new registered `tools/test-unit-navigation.py` covers exact statement navigation through Run and Find, full dirty Work descriptor and RAM-text retention, same-path refusal, and Main behavior. Its observer records pre-probe and settled descriptors separately because entering the editor intentionally resets cursor and previous. Each operation compares the prior settled descriptor with the next pre-probe descriptor. BADMAP now gives a nonzero source length below its map offset and must be rejected.
3. Accepted. `source_file` removes the old fixture before writing. The Real native fixture passes all 378 assertions.
4. Resolved. `cc_seal` clears `cc.polling` in `src/compiler_artifact.c` before successful compilation returns. Positive interruptible IDE Run and Find both navigate to the exact statement. The follow-up includes this file.
5. Resolved by tracing callers. Parser include lookup alone needs text-only resolution and now calls `sources_find_input`. Output collision checks retain `sources_find` over every record. Source IDs remain independent through link maps. The follow-up supplies `compiler_units_link.c`, `compiler_disk.c`, `compiler_units.c`, `compiler.c`, `unit_output.c` and `library.h`. Borrowed pages are cleared by record ID. A base offset is used only by text access while pages remain owned.
6. Resolved. All three IDE source-repair variants pass, including exact include locations and startup ownership of ten code pages plus Work and the three execution pages. The fresh native error-location fixture passes 45 assertions. The compiler-message driver passes all numeric/text diagnostic cases and the menu summary. Its old summary probe used an ignored Space command and sampled before a repaint. It now returns from Options to paint the summary, then captures it at the next clear. The publication-fault subtest was also obsolete after the disk compiler replaced transaction directories with an owned `main.$$$` file. It now authenticates the SDK rm veneer, injects read-only-drive error 18 for one syscall, restores the owner pathname before returning, verifies retained scratch bytes and primary error 1, and proves successful retry. The final registered diagnostic run passes with unchanged inputs. Error-location and parameter prerequisites are now registered, with existing runner regression coverage extended to both.
7. Accepted. The unit-authoring comment and `docs/ide-units.md` describe authoritative ZPU artifacts, optional source navigation, explicit rebuilding, and independent authoring paths.
8. Accepted. The Real fixture includes BADMAP, length 65535, and profile mutations with recomputed checksums. All are rejected in the passing 378-assertion run.

The corrected Integer wrapper passes 404 assertions with zero failures through the normal entry point. Real units pass 378 assertions, the combined corpus passes 1982 assertions, and the full 17-case matrix and standalone CHN checks pass. IDE unit authoring, TF/EEPROM publication, cancellation/retry, and the registered navigation fixture pass. The ZDE build fits all 16 KiB banks, with resident 15335 bytes and ZPL 238600 bytes. Cold startup uses ten code pages and the warmed compiler uses fifteen. The generated runtime prefix is fixed at 16 KiB. Implementation checkpoint df586e0 contains the tested source and fixture changes. `docs/sourcefree-units-evidence.json` binds 16 qualifying reports, binaries, source inputs and 126 capture files. Earlier failed focused runs are retained with their dispositions. The full product plan and physical hardware qualification remain open. The preceding `ide-units-v1` record remains historically unfinalized. This distinct review covers the source-independent import milestone and its supplied interactions.

## Opus turn 20261106T025205-daba4abb

Mode: code; read-only

# Follow-up review of `sourcefree-units-v1` at checkpoint `df586e0`

**Verdict: I found no correctness blockers and no high or medium findings in this increment.** All initial findings are resolved in the supplied source. Three low residual items remain: two evidence-record gaps and one workflow trap. None requires a binary change to finalize this bounded increment.

I reviewed by inspection and ran nothing. Test results below are the supplied claims, not my observations.

## Prior findings

| # | Prior item | Status | Basis |
|---|---|---|---|
| 1 | Refused navigation pinned Main | Resolved | `diagnostic` now pins Main only inside the final `if(located)` block, after source selection succeeds. `root` is a caller-owned copy, so it survives `build_close`. |
| 2 | IDE navigation branches untested | Resolved | `test-unit-navigation.py` covers exact Run and Find locations, dirty descriptor and RAM text, and same-path refusal. Removing the guard in `find_saved` would change the cursor and enter the editor, so the test would detect it. |
| 3 | `source_file` overwrote in place | Resolved | It now calls `rm` before `open`. |
| 4 | `cc.polling` after success | Resolved | `cc_seal` clears it before returning 1. The unit and failure paths also clear it, so `cc_poll` cannot dereference an expired `cc.error`. |
| 5 | Unsupplied callers | Resolved | Only the parser needs text-only lookup. `do_save_build` keeps `sources_find` across all records. Link maps use `module+6` source IDs. `sources_finish` clears by ID. |
| 6 | Pinned UI text | Resolved as reported | The fixtures were updated. The new diagnostic screen handles an empty path safely: `located` short-circuits before `file=255` is used. |
| 7 | Stale docs | Resolved | `docs/ide-units.md` and the `compile_work` comment describe authoritative ZPUs and optional source. |
| 8 | Metadata map bound | Resolved | The offset check uses the metadata length. BADMAP uses nonzero length 1, and length 0 and values above `SOURCE_LIMIT` are rejected. |

## Residual findings

### 1. Low (evidence): the index does not bind every report's captures, and one `passed` value looks mislabeled
**Location:** `reports.optional_matrix`, `reports.final_registered_diagnostics`, and `capture_sha256` in `docs/sourcefree-units-evidence.json`

**Trigger and impact:** An auditor relying on the claim that the index "binds all reports and capture hashes" can verify the 17-case matrix report, but not its underlying captures.

**Evidence:**
- `optional_matrix` has no `capture_directory`, and `capture_sha256` has no entries under `20261106T020545.015887Z-32915`. The 120 listed entries come from 14 other capture directories.
- `final_registered_diagnostics` records `"passed": true`. In `tools/run-tests.py`, `report['passed']` equals `full_acceptance`, which is false for any focused run, and the evidence says the full plan was not run. If that suite was focused, its `suite.json` should say `passed: false` and `selected_passed: true`. I could not read that file.

**Fix:** Add the matrix capture hashes, or state why that driver has none. Record the suite's actual `status` and `scope`, and keep `selected_passed` as the qualifying field. This changes no product binary.

### 2. Low (evidence): the review record cites a superseded navigation capture
**Location:** Codex reconciliation item 1 in `reviews/sourcefree-units-v1.md`

**Trigger and impact:** Item 1 cites `20261106T015612.899772Z-21984` as the passing navigation run, but the evidence index binds `20261106T022037.555561Z-47112`. A reader following the review record lands on a capture the index does not hash.

**Fix:** Cite the bound run, or label the earlier one as superseded.

### 3. Low (workflow): after navigating into a unit, C in Unit mode compiles the client
**Location:** `compile_work` in `src/main.c`, `unit_request` and `cc_units_header`

**Trigger:**
1. A client faults and the user navigates into the unit, so Main is pinned to the client.
2. The user fixes the unit, selects Options → U, and presses C.

**Impact:** `path` is still `main_file`, so the unit-mode refresh is skipped and the client is compiled as a unit. It fails with "Unit interface/implementation mismatch". `diagnostic` then replaces Work with the client. Dirty Work was saved first, so no text is lost and no artifact is published, but this is the natural repair path.

**Evidence:** `test-unit-navigation.py` runs `m\n` to clear Main before continuing, so the sequence is untested. `docs/ide-units.md` does not mention it. This behavior predates this increment, so it is not a regression.

**Fix:** Document clearing Main before rebuilding. Alternatively, in Unit mode compile Work instead of Main, with a fixture. That second option changes the binary and would need requalification, so it fits better in a later increment.

## Questions

None block finalization.

## Behavior noted as safe

- **Matching text only in Work:** `map_source_ready` reads disk only. If Work holds matching text but the disk file is missing or changed, navigation is refused. Work is retained and code and PC are still shown.
- **Stale ZPU after a source edit:** The client runs old ZPU code and shows the display-only failure, with no hint to rebuild. This matches the documented rule that rebuilding is explicit.
- **External ZPU changes:** An `X` child can replace a `.ZPU` without invalidating a Memory build, as accepted in the initial review.
- **Failed scratch removal:** After the printed primary error, a cleanup failure is not reported. It is retried on the next compile. If the user quits first, the leftover file triggers the existing scratch-conflict message on the next compile.

## Checked with no defect found

- **Source identity:** `sources_unit` appends independent records, `find_source` skips debug records only for text lookups, and `sources_matches` compares by ID. Ordinary and include records still return 1.
- **Lifetime:** Missing, read, allocation and free failures all pass through `sources_release` safely. A failed free stays reachable through `buffer_retire`. The uninitialized-block path in the `read_block` wrapper needs a path over 127 bytes, which metadata validation rejects.
- **Banking:** `build_location` uses only retained banks 3, 7 and 13 plus resident wrappers, so it works while compiler phases are parked.
- **Same-path Work:** Run and Find both compare against Work before any path that could reload it or move its cursor.
- **Macro-off builds:** Guards are consistent, and `zlegacypascal` compiles the new `diagnostic` signature.
- **Layout arithmetic:**
  - The 17 banks sum to 238550 bytes, and the 50-byte header gives 238600.
  - 65536−52527 = 13009.
  - 238798−238600 = 198.
- **Evidence counts:** There are 16 reports, 120 capture entries and 15 navigation checks.

## Documented scope limits, not findings

- Fixed 16 KiB generated prefix and execution-page allocation
- Primitive-only export scope
- Non-atomic sequential publication
- Physical hardware qualification
- Full TP4–TP7 recovery
- The unrun complete product plan

## Review limitations

- I did not recompute any SHA-256 value or read any report, capture or `suite.json`.
- `compiler_fragments.c`, `platform.c`, `optional-real` drivers and the `path.c` resolver were not supplied, so I took their behavior from the callers.
- Minor test gaps remain, with no defect shown: a checksum-valid non-canonical ZPU path, and source-record exhaustion at 255 entries.
- This is the milestone's one permitted follow-up. Any later fix would need verification by Codex, and another Opus turn would need an explicit user request.

### Codex reconciliation

1. Accepted. The evidence index now includes the matrix report's shared capture directory and all six journals from its optional and control sessions. It binds 126 capture files in total. The final registered diagnostic entry records the suite's actual `status`, `scope`, `passed`, `selected_passed` and `full_acceptance` values. Qualification uses `selected_passed` for that focused run. Its raw `passed` is false because the full product plan was not selected.
2. Accepted. The initial reconciliation now cites the bound navigation run `20261106T022037.555561Z-47112`. The earlier passing probe is superseded.
3. Accepted as documentation for the existing workflow. `docs/ide-units.md` now instructs users to clear Main with M and an empty filename before rebuilding the unit in Work through Options → U. The compiler retains its reviewed behavior. The existing navigation fixture already exercises clearing Main. A change to Unit-mode root selection is deferred to a later increment because this behavior predates the source-independent import milestone.

Codex verified every evidence hash after these corrections and compared all source and test inputs with the final review snapshot. They match checkpoint df586e0 exactly. All required bounded checks pass. No code or test edits followed this review. The evidence index, the IDE workflow documentation and the implementation-progress note changed after the last review turn, so unchanged-input workflow finalization is not claimed. Both permitted turns are consumed. The documented Main-clearing step, full product-plan run, hardware qualification and full TP4–TP7 recovery remain open scope limits. No further Opus turn was requested.
