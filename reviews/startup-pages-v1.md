# Review: startup-pages-v1

## Opus turn 20261105T114942-0e18e319

Mode: code; read-only

# Review: startup-pages-v1 (initial turn)

**Verdict:** I found no blocker or high-severity defects, and no product defect in the supplied code. The findings below are test-evidence gaps and low-severity behavior issues. This turn can't support a ready claim because the final native gates are still running, no results were supplied, and I ran nothing.

## Audit results
- **Sparse cursor:** No defect found. `code_load` advances `total` for every bank and seeks absolutely before each retained read. Seek clamping is detected by comparing the returned offset. Restore uses the same offsets as `code_phase_bind`.
- **Masks and slot ownership:** No defect found. The `valid` mask covers bit 15, and all 16-slot arrays are indexed by actual bank ID. Failed frees leave the handle in `code_pages` or `code_phase_pending`, and park can retry it.
- **`_code_release` guard:** The logic is correct. With 6 or more slots remaining, index 4 (BANK5) hasn't been freed, and index 5 can only survive in that range.
    - With today's 15-bank library, `cp #6` only changes behavior on a retry. By then park has already emptied every deferred slot, so the change is defensive and harmless.
- **Loader fd lifetime:** No defect found. `code_phase_fd` is set only by `code_phase_bind`, after all retained banks load. Failed loads close `fd` once and never call banked cleanup.
- **Cold entry points:** No violation in the supplied callers. CMake assigns only `compiler_expr.c` and `compiler_interrupt.c` to BANK6, and `compiler_files.c` and `compiler_io.c` to BANK10. All their exports are compile-time `cc_*` symbols. This audit is incomplete (see Q1).
- **Fixture corrections:** Offset 248 and the 592-byte member framing are arithmetically correct. The group offsets also check out: 68+76=144, +104=248, +60=308. The OWNERS change is still open (see Q2).

## Established findings

### 1. Medium: partial release is never tested, and the release test uses a model
- **Location:** `tests/native/output-host.c`, `code_release` and `initial_library`; `src/startup.asm`, `_code_release`
- **Trigger and impact:** The assignment requires early and late release failure tests. A regression in the release loop, retry path or assembly guard would pass the host suite.
- **Evidence:**
    - The host `code_release` is a hand-written model, and `code_owned>CODE_EXPRESSIONS` is a copy of the assembly guard.
    - `initial_library` sets `fail_at=0` before its explicit release.
    - During failed loads, the single injected fault has already fired before the internal release, so `pfree` never fails during release.
    - `phase_cleanup` bypasses `code_release` entirely.
- **Fix:** Add host cases that fail `pfree` at each descending slot, including below BANK5, then retry. Add a native case that makes a real `_pfree` fail, such as a temporarily invalid `code_pages` entry, then repairs it and re-enters `_code_release`.

### 2. Medium: terminal cleanup with cold holes has no proof that survives kernel cleanup
- **Location:** `tools/test-members.py`, `cleanup_ok`; `tests/native/startup-fail.c`
- **Trigger and impact:** Quitting without compiling is now the normal cold path, with holes at slots 5 and 9 and a retained fd. A leak there could go unnoticed.
- **Evidence:**
    - `cleanup_ok` requires 15 distinct loaded pages, so it covers only the fully restored state.
    - `startup-fail.c` does reach the cold path, but it checks `free_count` after exit.
    - The comment in `test-members.py` says automatic process cleanup "can conceal an application leak".
    - `interactive_checks` wasn't supplied, so it may cover this.
- **Fix:** Add terminal variants that are parked and that hold a pending page. Inspect `_page_owners` at `zos_loader_exit`, and check that the fd closes before OS exit.

### 3. Medium: the supplied evidence doesn't directly measure 11 pages during Run
- **Location:** `tools/test-startup-pages.py`; `tests/native/members-main.c`, `phase_cases`; `tests/native/startup-fail.c`
- **Trigger and impact:** The Run half of the goal rests on code reading. The startup number also comes from the application's own vector, while the user asked for actual RAM savings.
- **Evidence:**
    - `test-startup-pages.py` captures state only at `_code_ready`.
    - `phase_cases` checks an explicit park, then restores before calling `run_build`, so no state is observed during the child run.
    - In `startup-fail.c`, the 3+11 page case exits with status 1, the same as every load failure. It can't show that all retained banks loaded.
- **Fix:**
    - Probe `ram_enter` and the saved-program `exec` after a real compile, and assert 11 code pages and empty pending slots.
    - Add a positive fixture showing that 3+11+1 free pages reach the IDE prompt, and that one fewer doesn't.
    - Capture the kernel owner table at a safe point, or show the extra free pages with a real allocator measurement.

### 4. Low: failed first-compile restores keep partial pages, with no native low-memory test
- **Location:** `src/library_phase.c`, `code_phase_restore`; `tests/native/startup-fail.c`
- **Trigger and impact:** The IDE can now start with too little memory for the first compile. A restore can load up to three cold banks, fail on the next, and keep the loaded pages. That leaves fewer pages for block reads and other IDE work.
- **Evidence:** There's no rollback on failure, and the host tests assert the partial result (`phase_owned()==14`). The OOM fixture now stops at 11 pages, so it doesn't reach first-compile exhaustion. Host fault injection covers `palloc`, but not the real IDE path.
- **Fix:** Add a native test that starts the IDE, exhausts pages, and tries Compile. Assert the error, retained Work, and a successful retry after pages are freed. Consider parking after a failed restore.

### 5. Low: Execute from a cold session loads all four deferred banks
- **Location:** `src/library_phase.c`, `execute_native` and `code_phase_return`
- **Trigger and impact:** Running X before any compile moves the IDE from 11 to 15 code pages, even though the compiler was never used. Children still park, so this only reduces free memory inside the IDE.
- **Evidence:** `code_phase_return` calls a full `code_phase_restore`. The compile wrappers now restore on demand, and `library.h` says a failed reload is retried by the next compile, so eager reload isn't needed for correctness.
- **Fix:** Reload only banks that were loaded before parking, or restore lazily at compile time. Update the host oracle at `phase_owned()==15` to match.

### 6. Low: the data growth is avoidable, and the resident budget isn't shown
- **Location:** `src/library_phase.c`, `offsets` and `code_phase_pending`
- **Trigger and impact:** RAM grows by 66 bytes: `pending` +13, `offsets` +52 and `valid` +1. If this data shares page 3 with the stack, as the `_INITIALIZED=0xc000` link option suggests, it reduces stack margin. `pending` also likely adds about 16 initializer bytes.
- **Evidence:** Only 5 of the 16 `offsets` slots can ever be used. The 108-byte growth is in `zpascal.zpl`, the banked library, so it may be code growth rather than cache data. No `zpascal.bin` delta or headroom figures were supplied.
- **Fix:** Derive offsets from `code_sizes` at restore time, or keep only deferred slots. Report the resident size, BANK5 headroom, and data and stack limits from the current map.

## Questions and missing context
- **Q1: Can another retained path reach a cold symbol?** `native.asm`, `zp.h`, `output.c`, `compiler_build.c`, `compiler_artifact.c`, `image.c`, `compiler_compile.c` and the file defining `compile_limits` weren't supplied. Please add a mechanical check that fails if any object outside an allow-listed compile path imports a BANK6 or BANK10 symbol. Also confirm that no assembly hard-codes those `code_pages` slots.
- **Q2: Is the OWNERS oracle independently derived?** It changed from `[1,2,3,4,6,7,8]` to `[1,2,3,5,6,7]`. That isn't just a count correction, because key 5 was added while 4 and 8 were removed. Please supply `late-cases.h` and the late-key enum so this can be checked against the intended case rather than current output.
- **Q3: Do any stale 13-bank assumptions remain?** Please supply a repository search for `CODE_PHASE_FIRST`, `thirteen`, pending-array sizes of 2 or 3, and the specification's memory claims. Also supply `layout_checks`, `allocation_checks`, `bank_lifecycle_checks` and `interactive_checks`.
- **Q4: Does `allocation_checks` depend on `boot_passed`?** That counter is `uint8_t` and can wrap across this loop. Any pass-count oracle needs to account for that, and for the shorter loop.

## Review limitations
- This covers only the supplied files, and the current tests you plan to send in the follow-up may close some of these gaps.
- I didn't verify runtime behavior, the final link map, or Zeal kernel seek behavior beyond what the code and fixtures show.
- The 366115 host assertions and the initial snapshot are your reported evidence, not results I reproduced.

### Codex reconciliation

1. Accepted: host free failures now cover every logical slot; native RELEASE.TEST repairs a failed Shared slot, overwrites the freed BANK5 loader page with HALT bytes and retries real resident `_code_release`. All pass.
2. Accepted: parked and pending terminal variants inspect kernel owners and fd state before OS cleanup; all pass.
3. Accepted: native `_ram_enter`/`_exec` probes observe11 retained code pages and empty pending slots; positive15-page allocator budget reaches IDE with Work, negative14-page budget cannot.
4. Adapted: real allocator pressure loads one cold bank then fails; source/build descriptors survive and retry succeeds after pressure release. Valid partial pages deliberately remain cached until park; rollback would add ownership complexity without correctness benefit.
5. Accepted: resume_mask restores only the pre-park warm set. Cold Execute remains at11; warm Run retains previous cache on return.
6. Accepted: removed cached offsets; derive from immutable code_sizes. Static data +4 bytes, resident15285 (+217), stack reserve13170, BANK5 headroom4797. `.zpl`202359 (+82), no disk shrink claim.
Q1: Added mechanical linked-object cold import audit and requested surrounding sources; audit passes, no hard-coded cold assembly slots.
Q2: Supplied late enums/cases and compiler providers. Factory symbols keyboard0/file4 generate no encoded references; poll/status/interrupt keys1/2/3 and output/transfer/unpack5/6/7 are the six terms. This derives from the case's factory/member sequence, not its observed output.
Q3: Search supplied in analysis/startup-pages-search.txt; historical milestone claims marked historical and current specification links new startup evidence. Full test-native.py supplied.
Q4: Counter widened uint16; done and failed fields, not pass count, are the oracle.
Verification: startup report and members/unit captures indexed in docs/startup-pages-evidence.json; host366841 assertions/2528 scenarios; members186 positive/193 negative-retry; units323. No known product blocker. Required follow-up pending.

## Opus turn 20261105T124013-ab28f3a8

Mode: code; read-only

# Follow-up review: startup-pages-v1

**Verdict:** I found no blocker, high-severity or product defects, and no regressions from the follow-up changes. All six earlier findings and all four questions are closed, with finding 4 closed by an accepted adaptation. One medium evidence gap should be closed before calling this ready: the supplied gates don't show most cold production IDE workflows running.

I ran nothing. Test results are Codex-supplied, and the raw reports weren't included. The size figures do agree: the bank list plus the 46-byte header totals 202359, BANK5 headroom is 4797, and static end plus stack reserve is 65536.

## Earlier findings

| # | Status | Assessment |
|---|---|---|
| 1. Partial release | **Fixed** | The host now fails `pfree` at every slot. The native retry poisons the freed loader page, so any guard value of 3 or lower would execute HALT bytes when `owned==3`. Guard values 4–6 are all safe. |
| 2. Cold terminal cleanup | **Fixed** | Cold, pending and partial-retry exits are checked against kernel owners at `zos_loader_exit`, before automatic cleanup. |
| 3. Measuring 11 pages | **Fixed** | Probes at `_ram_enter` and `_exec` check the retained set and empty pending slots after a real compile. The 15-page budget reaches the IDE, and exhaustion cases cover the failing side. |
| 4. Failed-restore pages | **Adapted, accepted** | Keeping valid cache pages is safe, and native low-memory retry is covered. The documentation needs a correction (finding 2 below). |
| 5. Cold Execute | **Fixed** | `resume_mask` is captured on park and reset on bind. Partial park failure, repeated park and failed return all remain safe, and compile still restores with `65535u`. |
| 6. Data budget | **Fixed** | Static data grows only 4 bytes: `valid` +1, `resume_mask` +2, `pending` +13, `offsets` −12. Offsets now derive from `code_sizes`, and `bank_offset` is near-called within BANK5. |

## Earlier questions

- **Q1, cold reachability: answered, with a residual limit.** The audit fails closed on the expected provider set. Every listed import is compile-phase code, and `native.asm` touches only slot 2 (`_code_pages+2`). See limitations for what it can't show.
- **Q2, OWNERS oracle: resolved.** I traced the symbol order independently:
    - `KEYBOARD` (0) and `FILE` (4) are created by requests only and have no references.
    - `KEY_POLL`, `KEY_STATUS` and `INTERRUPT` (1–3) come from direct operands.
    - The member-end chains create 5–7.
    - That gives `[1,2,3,5,6,7]`, matching `references==3` before member end and `member.lates==6`. The list holds symbol indices, not `LATE_*` enum values.
- **Q3, stale 13-bank assumptions: resolved.** The only remaining `thirteen` hits are unrelated code (`compiler_expr.c`) and a historical ledger entry. The historical specification text is append-only and superseded by the new section.
- **Q4, `boot_passed`: resolved.** The counters are now `uint16_t`, and the oracle reads `boot_done` and both `boot_failed` bytes.

## Remaining findings

### 1. Medium: most cold production IDE workflows have no supplied results
- **Location:** `docs/startup-pages-evidence.json`, `tools/test-startup-pages.py`, `tools/test-native.py`, `tools/test-tp3-open-items.py`
- **Trigger and impact:** Component targets define `ZP_TEST_COMPILER` and restore every bank at startup, so only production `zpascal` runs cold. A missed cold call would map page 255 and crash only in that build.
- **Evidence:**
    - The startup tool runs only the layout, library, allocation, lifecycle and interactive groups, plus the budget case.
    - The supplied evidence doesn't show the `files`, `paths`, `full-media` or `user-break` cases.
    - It also doesn't show the Options, Execute, directory and logged-drive IDE suites, or the changed `test-tp3-open-items.py` oracle.
- **Fix or verification:** Run those suites against the final hashes, or add their existing report paths to the evidence index. This isn't a defect finding: the linked-object audit makes a failure unlikely. The goal is runtime evidence for the path users now take by default.

### 2. Low: the partial-cache limitation is understated
- **Location:** `docs/startup-pages-experiment.md` ("Parking releases the cache"), and the limitation text in `docs/startup-pages-evidence.json`
- **Trigger and impact:** After a failed compile, the valid partial banks are in `resume_mask`, so they reload when the child returns. Parking frees them only for the child's lifetime. A successful compile or quit is what ends the state, not parking.
- **Fix:** Reword the limitation to say that. No code change is needed.

### 3. Low: the bank audit isn't bound to the evidence
- **Location:** `analysis/startup-pages-bank-audit.json`, `docs/startup-pages-evidence.json`
- **Trigger and impact:** The audit report has no build hash, and the evidence index doesn't reference it. A reader can't prove it examined the tested `zpascal.map` and objects, which `AGENTS.md` requires for findings.
- **Fix:** Record the map and object hashes in the audit report, then index it with the other evidence.

Two comments are also stale: "declared bank offsets" above `code_phase_bind`, and "only while a loaded compiler phase exists" above `code_phase_cleanup`. Fix them only if those files are next touched.

## Question
- **Do all captures share the same inputs?** The units capture (`122539`) starts about five minutes before the startup and members captures (`123036`). Please confirm their recorded input hashes match, or state exactly which inputs differ.

## Limitations (not product defects)
- **Partial compiler cache:** after a failed compile, valid loaded banks can stay allocated. This is deliberate and safe, and it only affects low-memory sessions.
- **Audit granularity:** the allow-list is per file and covers direct imports, so it doesn't prove which functions are reachable. The files defining `compile_limits` and the bodies of `compiler.c`, `compiler_routines.c`, `compiler_strings.c` and `compiler_crt.c` weren't supplied. `compile_limits` is reachable from the cold Options screen, so finding 1's Options run should cover it.
- **Scope of the savings:** this reduces allocated RAM by 32 KiB. The library grows 82 bytes and the resident image 217 bytes. There's no standalone runtime reduction, Real/Screen extraction or IDE unit authoring.
- **Evidence level:** these are incremental ZDE builds on the pinned emulator and ROM, not clean-build, hardware or full TP3 conformance proof.

### Codex reconciliation

Accepted follow-up findings. No product defect found, but readiness remains open pending expanded production workflow evidence. Existing files and user-break workflow runs pass. The paths workflow has one failing COM-publication assertion requiring investigation; the other checks pass. Options, Execute, directory/logged-drive, TP3 open-items and full-media checks are running. No additional review turn is requested or claimed.

Documentation correction: valid partial compiler cache can persist across a failed compile. Parking releases it only during child execution; resume_mask reloads it on return. The earlier wording “Parking releases the cache” and “until parking” understates that persistence. This correction is recorded here; reviewed context files have not been edited after the follow-up. Update those passages when the next documentation change is reviewed.

Audit binding: analysis/startup-pages-audit-bindings.json records SHA-256 for the audit report, normal zpascal map, audit script and every scanned production object. The existing evidence index still needs that link in a subsequent reviewed update.

Capture consistency: 126 shared input hashes (normalized absolute/relative paths) compared across startup, members and units; differing common inputs: []. All recorded currently existing input hashes in those reports also match current files. The earlier timestamps do not represent source drift.

Two stale source comments are deferred until those files are next touched, as requested. This milestone is not finalized and no additional review rounds have been used.

#### Final Codex qualification (2026-11-05)

The remaining follow-up evidence gap is closed. Final native startup report
analysis/startup-pages/20261105T131044.823354Z-94298/report.json passes every gate with 11 startup code pages and 32768 bytes saved.
Production files, paths, full-media, user-break, Options, Execute,
directory/logged-drive and TP3 capacity workflows all pass. Their paths, report
hashes and check results are retained in docs/startup-pages-evidence.json.

The previously failing COM path case was a stale oracle: saved compilation
releases Build image/map cache. The COM existed with a valid native header and
source/media behavior was correct. The corrected oracle checks the released
cache and actual COM. Save has no overwrite-confirmation prompt. The old
full-media existing-file Save enters the already documented pinned-OS growth
fault; the corrected bounded fixture uses a new path so C/R/S fail at creation
and preserve media plus dirty source. This does not qualify the excluded OS
fault or modify third-party software.

Directory screen sampling now visits supported Options/Return for menu refresh
and observes real character VRAM before clear. The ownership oracle checks every
unchanged kernel owner/bitmap bit and accounts for the exact absent Build
window and restored compiler handles after Run, including reuse of a parked
compiler handle by the image. The entire 55-checkpoint physical workflow
passes, including 75-entry pagination, drive selection, descriptor closure, cached
Run, source retention and pre-kernel-cleanup terminal release.

Accepted low documentation finding: partial compiler cache is parked during
child execution and restored on return, so parking alone does not permanently
end the partial cache state. Both experiment and evidence text now state this.
Accepted audit-binding finding: the audit hashes its map, normal executable,
library, scanned objects, assembly inputs and audit script. The full passing
audit and its 94 input bindings are embedded in the committed evidence index.

The sanitized host gate was refreshed with source and binary bindings: 366841
assertions across 2528 scenarios pass. Prior 186 positive/193 negative member
assertions and 323 unit assertions retain matching product-source hashes. The
shared observer module changed only full_media_checks and default_path_checks;
older independent workflows do not call them. The final startup gate has no
source/tool/artifact drift.

No product source changed after the Opus follow-up: all 29 reviewed source files
are byte-identical. Test fixture and documentation corrections followed that
review; no additional turn was run and exact-final-input workflow finalization
is not claimed. The two stale source comments remain deferred until those
files are next touched, as suggested. No unresolved product defect remains for
this bounded startup-memory stage; full conformance, hardware, standalone
runtime compaction and Real/Screen extraction remain outside it.
