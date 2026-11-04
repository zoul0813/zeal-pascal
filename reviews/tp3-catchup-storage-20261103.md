# Review: tp3-catchup-storage-20261103

## Opus turn 20261103T200407-f2c477c5

Mode: code; read-only

# Review: TP3 catch-up, storage/IDE scope (013e79b → e9003b5)

**Verdict:** I found no blockers. Three medium defects (findings 1–3) are worth fixing before calling this ready.

**Limitations:**
- I ran nothing. Every test result cited below is Codex's reported evidence, not mine.
- I reviewed only the supplied diff and current files. I did not see `compiler_hooks.c`, the finishers, `runtime.asm`, `harness.c` or the native acceptance drivers. Conclusions that depend on them are marked as questions.

## Established defects

### 1. MEDIUM — Failures after a successful disk compile leak the disk owner
**Where:** `src/compiler_job.c`, `compile_request`, the paths after `compile_project` succeeds.

**Trigger:** any of these failures after the compile succeeds in mode 1 or 2:
- `compiler_result_ready(&built)` fails (`goto corrupt`);
- `retain_sources()` fails;
- `cc_tables_end()` fails;
- `compiler_pages_release()` fails.

**What happens:** none of these paths closes `built`. In particular `cc_disk_discard(&built)` is never called. The static `owner` in `compiler_disk.c` still points to the dead stack `Build built`. The scratch fd stays open and `X.$$$` stays on disk.

**Impact:**
- Every later saved compile fails, because `cc_disk_begin` starts with `if(owner)return failure();`.
- The handle and scratch file leak until the IDE exits.
- After a restart, finding 2 then blocks that program's saved compile.

**Fix:** set a flag once compilation succeeds. In the shared failure tail, before `compiler_resources_ready()`, call `cc_disk_discard(&built)`; it is a no-op once ownership has been adopted. Alternatively, call `build_discard_image(&built)` before each `goto`.

**Test:** inject a page-drop failure on the final `compiler_pages_release` in a mode-1 job. Then check that a second mode-1 compile succeeds and that no `.$$$` remains.

### 2. MEDIUM — A leftover `X.$$$` permanently blocks saved compilation with an unexplained error
**Where:** `src/compiler_disk.c`, `cc_disk_begin`.

**Trigger:**
- A reset or power loss during a COM/CHN compile leaves `X.$$$`, or
- `rm` fails and the IDE then quits. `owned` is RAM-only, so it is lost.

**What happens:** `stat` succeeds, so the function returns `ERR_ALREADY_EXIST`. `compile_work` prints only "Compile failed, native error 15", with no path.

**Impact:** saved compilation of that program never works again until the user guesses which file to delete. Refusing to clobber a foreign file is a reasonable safety choice, but the user must be told what to remove.

**Fix (smallest):** expose the scratch path, for example `cc_disk_scratch()`, or let `compile_work` derive it with `path_program` plus `$$$`. Print it on `ERR_ALREADY_EXIST`.

### 3. MEDIUM — Find run-time error cannot locate programs that only fit via disk emission
**Where:** `src/main.c`, `find_saved`, which calls `compile_project(...)` directly.

**Trigger:** use COM/CHN mode on a program that compiles only through the disk workspace. The fixture `large_job_capacity` is an example: code larger than 16 KiB with about nine free pages.

**What happens:** `compiler_disk_requested` is 0, so F always does a RAM compile. That needs image pages 0/2 (plus 1 and a stage page), the map and the tables. The compile fails with error 99, and F reports "Source lookup failed".
- A user who just built and ran the COM cannot find its error position.
- F also loses the Ctrl-U interrupted-versus-failed distinction.

**Fix:** in saved modes, have F compile with `compiler_disk_requested=1` and no `prepare_build`/publish. Discard the result with `build_close`. Alternatively, route F through `compile_request` with a no-publish flag so it gets the same ledger and cleanup.

**Test:** run F against the constrained-memory COM fixture.

### 4. LOW — Dead reference IDs read back as silent zeros after parsing
**Where:** `src/compiler_tables.c` (`cc_references_end`) and `src/compiler_sources.c` (`cc_reference_position` / `cc_reference_seek`).

**What happens:** `cc_references_end` empties the table but leaves `cc.sources->refs` unchanged. Any post-parse use of a valid-looking reference ID passes the `reference>=refs` check, and `read()` returns zero-filled bytes with success.
- Before this change the same misuse failed with 99, because `refs_page==255`.
- Include descriptors also become `page=255` after `sources_finish`, so `cc_source_byte` silently returns 0.

**Impact:** a finisher that lazily emits from a source reference, such as a Real literal or a name, would produce wrong code instead of failing.

**Fix:** set `cc.sources->refs=0` in `cc_sources_finish` so late use fails with 99.

### 5. LOW — Generic names in `compiler_disk.c` have external linkage
**Where:** `src/compiler_disk.c`.

**Problem:** `zero`, `cached`, `dirty`, `placed`, `sealed`, `chain_kind` and `owned` are non-static. With common-symbol linkage (host) they can silently alias another module's global of the same name. On SDCC they risk duplicate-symbol links.

**Fix:** make them `static`.

### 6. LOW — Saved CHN "Run" leaves the disk Build open
**Where:** `src/main.c`, the `'r'` path with `saved_mode==2`.

**What happens:** `compile_work(1)` keeps `build` open (open fd and `.$$$`), then only prints the "Run CHN files with Chain" notice. The scratch file stays open across later `X`/Execute and directory listings until the next invalidate.

**Fix:** pass `running` only for `saved_mode==1`, or call `build_close(&build)` in the CHN branch.

### 7. LOW — Stale or contradictory documentation
- **`tests/native/tp3-fidelity.json`:**
  - `disk_publication` still says "Compilation still builds code in RAM; TP3-10 … remains open" and "old-map invalidation".
  - `startup_phases` says "Validate the full library extent and EOF before startup", which direct reads no longer do.
- **`src/startup.asm`:** the comment on `_code_version` still says "exact library SHA-256".

Correct these so the evidence record matches the code.

## Questions (not established)

### Q1. Does Memory Run reset all mutable runtime-prefix state?
**Where:** `platform.c` `run_build` (persistent image) and the runtime entry.

**Why it matters:** the prefix used to be copied fresh on every Run. Now the program's runtime data persists across runs. Please confirm that the `RT_RAM` entry reinitializes all of:
- heap and recursion pointers;
- the 16-handle file ledger and I/O status;
- input line and typeahead state and `REAL_DATA`;
- CBreak and hook vectors;
- the error namespace.

TP3 behaviour supports typed-constant persistence (1, then 2), not stale runtime state.

**Suggested test:** in the first Run, leave a file open, leave the heap un-Released and reassign ConOutPtr. Assert that the second Run behaves like a fresh Run except for typed constants.

### Q2. Is the disk workspace's layout guaranteed at placement?
**Where:** `compiler_disk.c`, `cc_disk_place`.

**Concern:** the condition `code_base >= PREFIX+globals` is only re-established when some `cc_disk_put`, `get` or `global_put` runs after the last `cc_storage`. If a finisher allocates storage after the last emitted byte and the program has no relocations, publish would read globals from the code area.

**Defensive fix:** call `global_space(code_used)` inside `cc_disk_place`, and fail with 99 if it returns 0.

### Q3. Is there enough stack headroom for disk emission inside deep parser recursion?
`cc_emit` can reach `global_space`, which has a 128-byte local buffer and makes a banked call into `output_write_buffer`/`cc_poll`. Please confirm worst-case depth against the 768-byte `cc_stack_check` margin, ideally with the 40-nested-block fixture in COM mode.

### Q4. How should overlays behave in saved mode?
`cc_members_flush` and `cc_group_close` still require `page_role(cc.image->pages[2])==PAGE_IMAGE`. That can never hold under disk emission. Overlays are deferred, but please confirm that overlay syntax in COM/CHN gives a clean diagnostic rather than a platform failure.

## Checked and consistent
- **Bank map:** `CODE_*` IDs line up with the CMake bank assignments, and resident placement is correct for:
  - `cc_runtime_copy`, `run_build` and `code_read_bank`, which remap page 2;
  - `compiler_emit` and `compiler_disk`.
- **Ownership handoff:** ownership is handed off in this order: retain sources, then keep map/sources chains, then release the ledger, then adopt.
- **Map bounds:**
  - the `build_location` loop does not wrap at 8191 records;
  - `cc_mapping` stays within `SOURCE_LIMIT`.
- **Buffer chains:**
  - chain versioning goes through `buffer_set`, `far_move` and the head version;
  - `source_move_range` copies in the correct order;
  - `buffer_retire` trims correctly.
- **`compiler_tables`:** bucket arithmetic and the prefix sum (16256) are correct.
- **Native moves:** the `far_move` frame, IX offsets and stack cleanup match the read path.
- **Disk scratch:** the sector slide is overlap-safe, and the `disk_prefix` stack buffer fix is correct.
- **CHN and launcher:**
  - CHN unused-entry restoration reads from the immutable runtime;
  - the launcher checks the version and size tables.

## Missing coverage
- Failure injection after a successful disk compile, for finding 1.
- Pre-existing `.$$$` recovery messaging, for finding 2.
- F under constrained memory in COM/CHN modes, for finding 3.
- A two-run Memory test of runtime-state reset, for Q1.
- COM deep-nesting stack usage, for Q3.
- An overlay diagnostic in saved mode, for Q4.

### Codex reconciliation

Accepted findings 1–7, with these fixes and bounded proofs:

1. Shared compile_request failure tail discards the disk owner before releasing resources. Host fault injection fails the final root release after successful disk compilation and verifies a later saved compile succeeds without leaked scratch.
2. Resident cc_disk_path exposes the existing pathname; IDE reports the exact conflict plus remove/rename instructions. It preserves a foreign file. Native UI coverage caught and fixed diagnostic output clearing platform_error: the caller now captures the error by value before output. Final UI result is in the supplied verification record.
3. F enables disk emission in COM/CHN mode without output preparation/publication, captures interruption state, closes the Build. Host constrained-capacity coverage uses disk lookup; native UI proves the flag at entry, error-position completion, no COM/ZMP dependency and scratch cleanup.
4. cc_sources_finish clears refs after successfully releasing the reference stream, making stale IDs fail.
5. All generic disk cache/owner flags now have static linkage.
6. Rejected CHN Run closes the Build. Native CHN acceptance checks it is invalid afterward.
7. Corrected host build-ID comment and stale disk/startup fidelity descriptions; historical measurements remain labeled as historical.

Q1 adapted: runtime startup already resets heap/RecurPtr/ceiling and module/history ownership. It now also resets runtime-owned IO/error/input cursor/cache/EOF/token and key-parser state. The two-run native regression poisons internal IO/input/heap state and requires reset while typed constants remain 1 then 2. Mutable Pascal globals, CBreak, BufLen and hook pointers are not blanket reset: the manual describes user-mutable values and the observed TP3 Memory contract preserves program data; no original evidence justifies erasing those settings. Terminal file cleanup remains in the existing return gate.
Q2 accepted: cc_disk_place calls global_space(code_used) at the owner boundary.
Q3 tested: native 40 nested begin blocks compile and run through disk emission at the existing C-stack margin. This is a regression proof, not an exhaustive maximum-depth guarantee.
Q4 tested without changing deferred overlay scope: host Memory gives diagnostic77, disk COM/CHN diagnostic12; ownership and scratch clean up. Overlay preparation remains shipped per user decision.

Verification: sanitized host compiler178035/editor4010/sources45809; output349746 across2528 fault/cancellation scenarios; native core2109/0, files483/0, Execute cleanup/rollback pass. Full plan and physical hardware were not rerun. The follow-up verification context records final UI/error-location/CHN and artifact results. No known unresolved established storage defect before follow-up.

## Opus turn 20261103T211033-b0419db5

Mode: code; read-only

# Follow-up review: TP3 catch-up, storage/IDE scope (013e79b → working tree)

**Verdict:** I found no blockers within the supplied scope. Every prior finding is resolved or reasonably dispositioned. Two low-severity issues remain, plus one low documentation inconsistency. I could not execute anything, so all test results cited below come from Codex's verification record.

## Prior findings, as dispositioned

| # | Prior finding | Disposition | Assessment |
|---|---|---|---|
| 1 | Disk owner leak after a successful compile followed by a failed handoff | Shared `done:` tail calls `cc_disk_discard(&built)` when `!ok` | **Resolved.** On early exits `built` is uninitialised, but the discard only matches `owner==&built`. `owner` is always cleared after a failure, so the call is a safe no-op. A host fault test covers the late page-release failure. |
| 2 | Leftover `.$$$` with an unexplained error | `cc_disk_path()` plus `scratch_conflict()`; error captured by value in `compile_work` | **Resolved for Compile.** See new finding 1 for the Find path and misattribution. |
| 3 | F needs disk capacity | `find_saved` sets `compiler_disk_requested`, distinguishes interruption, and closes the Build | **Resolved.** The scratch file is cleaned up by `build_close`. Host constrained-capacity and native UI checks cover it. |
| 4 | Stale reference IDs read as zeros | `cc.sources->refs=0` in `cc_sources_finish` | **Resolved.** |
| 5 | Non-static disk globals | Now `static` | **Resolved.** |
| 6 | CHN Run leaves the disk Build open | `build_close` in the `saved_mode==2` branch | **Resolved**, and native CHN asserts it. |
| 7 | Stale docs | Corrected | **Resolved**, apart from new finding 3. |
| Q1 | Memory Run runtime state reset | Resets runtime-owned state; keeps Pascal-visible settings | **Accepted.** Details below. |
| Q2 | Layout at placement | `cc_disk_place` calls `global_space` | **Accepted.** See new finding 2 for a narrow residual. |
| Q3 | Stack depth in deep recursion | 40-deep native disk compile passes | **Accepted** as regression evidence, not as a proven bound. |
| Q4 | Overlays in saved modes | Disk COM/CHN reports 12 and cleans up | **Accepted.** Deferred-overlay scope is preserved. |

### Detail on Q1 (Memory Run reset)
`start:` now does all of the following before entering the program:
- clears `io_status`, `error_code` and `error_namespace`;
- runs `input_reset`, which zeroes `input_state_begin..end` inclusive (one store plus an LDIR of `count-1` bytes) and then restores `echo_fd=255`;
- calls `_key_reset` and zeroes `recursion_ceiling`;
- re-derives `heap_floor`/`heap_frontier` and the 0xFE70 module control from the header.

This separates runtime-owned session state from persistent Pascal data, which is what the assignment asked for.

Keeping CBreak, BufLen and the hook pointers is a defensible reading of TP3, given the manual's description of them as user-mutable. The native two-run regression poisons the I/O, input and heap internals and requires the counter to read 1 then 2.

## Remaining findings

### 1. LOW — Scratch-collision guidance can be lost or misattributed
**Where:** `src/main.c`, `lookup_failure()` and `find_saved()`.

**Error is lost when F loads Main from disk:**
- F is used with Main different from Work, which sets `loaded=1`.
- `compile_project` fails at `cc_disk_begin` with `ERR_ALREADY_EXIST` and restores `platform_error`.
- `find_saved` then runs `build_close(&build)` and `source_discard(&work,&source)`.
- `source_discard` reaches `buffer_drop`, then `page_drop`, which sets `platform_error=pfree()=0`.
- `lookup_failure()` therefore never shows the scratch path.

**Message can appear when it is wrong:**
- `lookup_failure()` is also reached from `open_work`, `select_main`, `select_error_source` and others.
- A stale or unrelated `platform_error==15` in those paths prints "Scratch file exists: …" using whatever `scratch[]` last held. That may be an empty string or an old program's path.

**Fix:**
- In `find_saved`, capture `error=platform_error` right after `compile_project` and pass it explicitly.
- Call `scratch_conflict()` only from the Compile and F paths, not from `lookup_failure()`.
- Optionally, have `cc_disk_begin` set a "collision" flag that is cleared on the next begin, and gate the message on that flag rather than on the error number.

### 2. LOW — Silent placement failure is possible without polling
**Where:** `src/compiler_disk.c`, `cc_disk_place`.

**Problem:** if `global_space()` fails while `cc.polling==0`, it records no `cc.error->code`. Placement then returns without setting `placed`, and `cc_place_code` continues. Its later prefix and hook writes go through `cc_disk_put` instead of the final-image mapper.

**Why it is not live today:** every disk-mode caller (`compile_request` and F) passes `interruptible=1`.

**Fix:** in `global_space`, call `cc_fail(99)` regardless of `cc.polling`, or assert that disk mode implies polling.

### 3. LOW (documentation) — Error-location count is stale in the fidelity record
**Where:** `tests/native/tp3-fidelity.json`, `verification.native_error_location_passed`.

**Problem:** it still says 28, while `reviews/tp3-catchup-verification.txt` records 37/0 for the final run. `native_sources_passed` (65) and the handoff count (944) also predate this follow-up.

**Fix:** update these fields, or label them as earlier-run values.

## Specifically checked for this follow-up
- **Cross-bank caller stack and job lifetime:**
  - `CompileFailure` and `built` stay on the BANK14 caller frame for the whole synchronous call. `compiler_invocation` is cleared before return.
  - The dangling `cc.image` left after adoption is never dereferenced. `failure()` and `global_space()` touch `cc.error` only while polling, and `cc_seal` clears `cc.polling`.
- **Module C stacks:**
  - File and transfer C code now runs on its own module-tail stack (`ld sp,#0x8000`, with the code limit validated below 0x7C00 or 0x7800).
  - The nested FileCleanup relay copies its 6-byte job to 0xFE68 and runs on SP=0xFC80. This is consistent with `worker_end<=0xFE68` and with transfers being blocked in Memory mode (the 0xFE78 check).
- **IRQ preservation:** the supplied PIO excerpt pushes onto the current stack and remaps only page 3. All the new stacks (page 1 module tail, and FC00–FC7F in page 3) are therefore safe while the ISR runs. The saved-SP slots are not re-entered, because `zf_dispatch` defers stream callbacks until after the prefix is restored.
- **Runtime string scratch addresses** (0xFD66–0xFD78 and 0xFE85/87): within `concatenate`, `hold_string`, `prepare_text`, the set helpers and the heap helpers, no live overlap occurs during any single helper invocation. Semantic correctness of these helpers belongs to the runtime reviewer.
- **Observer layouts:** `BUILD_BYTES=54` matches the current `Build` and `Sources` layouts. The IDE/CHN observers read the correct offsets (+5 length, +9 code_bytes, and the 5-byte closed-Build signature).

## Limits
- This covers only the supplied diff, current sources, tests and kernel excerpts.
- File-runtime and transfer semantics, String256 code generation and Execute reclamation belong to the runtime reviewer. I relied on the supplied kernel evidence for those.
- There is no exhaustive stack-depth proof.
- The full test plan and physical hardware were not rerun, per the verification record.

### Codex reconciliation

1. Accepted and fixed. Removed scratch guidance from generic lookup_failure. F captures platform_error immediately after the compile, only for a platform compile failure, and passes it explicitly after releasing a separately loaded Main. Native UI regression now includes Work different from Main plus a foreign Main.$$$. No generic open/path failure can invoke scratch guidance.
2. Accepted and fixed in the responsible capacity guard: global_space reports cc_fail(99) irrespective of keyboard polling. It is a compiler-emission/placement operation, never used by final publication after the compile lifetime. cc_place_code now stops immediately if placement sets an error. Host regression constructs a nonpolling disk placement with insufficient static capacity and requires99, unplaced state and clean owner release.
3. Corrected native_error_location_passed to37 and explicitly labeled earlier944 handoff/65 source counts as pre-review evidence.

These small source changes follow this storage scope's second turn, so its previous input fingerprint is stale and is not finalized/reused. Final source files, this reconciliation and test evidence are also supplied to the still-pending runtime follow-up, including its existing disk/ownership cross-boundary context. No third storage turn is requested or claimed. Verification is recorded in reviews/tp3-catchup-verification.txt. No established finding is intentionally left unfixed.
