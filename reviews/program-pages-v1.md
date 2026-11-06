# Review: program-pages-v1

## Opus turn 20261106T140750-fab4494c

Mode: code; read-only

# Review of program-pages-v1 (initial)

**Result:** one established defect, one probable compatibility defect that needs a build comparison, and several coverage gaps. I found no established defect in the production allocation path.

## Established defects

**1. High: `tools/test-units.py`, `unit_payload()` rejects every new artifact**
- **Trigger:** the registered `units` run reaches the `unit_payload(directory/'LARGE.ZPU')` and `SMALL.ZPU` checks.
- **Impact:** the tool raises `AssertionError` while building `checks`, before it writes a report. The `units` gate cannot pass, even if the native run succeeds.
- **Evidence:** `compiler_units_object.c` writes `ZPU_ABI`, which is now 3, and `zunittest` uses that code through `unit_core`. The helper still asserts `word(4) in (1,2)`, and the diff does not change this file.
- **Fix:** accept ABI 3, preferably with an exact check for what the fixture produces. Also check `tests/native/units-main.c` and any other ZPU parser for a hard-coded ABI 2; those were not supplied.

## Probable defect, needs confirmation

**2. High if confirmed: `src/native_image.c` may change the runtime profile and make real older units unreadable**
- **Trigger:** a client imports a `.ZPU` actually built at e351a8c.
- **Impact:** if the profile changed, `metadata_valid()` fails with `UNIT_STALE`. That would contradict both "ABI1/ABI2 remain readable" and "profile unchanged". Generated Chain/Execute programs would also embed changed transfer bytes.
- **Evidence:**
  - `transfer_runtime` links `native_image.c`, and no `ZP_CHAIN_ONLY` definition appears for it in `CMakeLists.txt`, so it likely gets the new generic check.
  - `pascal_runtime.profile` depends on `transfer_runtime.zpl` and `transfer_runtime.packed`.
  - The compatibility tests would not detect this. `test-program-pages.py` and `legacy_artifact()` only relabel freshly built artifacts, which already carry the new profile.
- **Limit:** `tools/package-native.py` was not supplied, so I cannot tell which inputs the profile hashes.
- **Verification:** compare `pascal_runtime.profile` and `transfer_runtime.zpl` at e351a8c and at the current tree. Then add a checked-in ABI 2 fixture built at the baseline. If the profile differs, separate the transfer decoder change from the profile or explicitly revise the compatibility contract.

## Missing tests

**3. Medium: kernel cleanup after a shared program exits is never observed**
- **Gap:** the standalone driver stops at `zos_loader_exit`, reads `_page_owners`, and quits. Nothing covers the process after kernel cleanup, when page 2 is mapped twice and the original page 3 has already been freed.
- **Risk:** a double release, leak, or bad parent restore would not show up. The unmodified kernel source was not supplied, so this is unproven rather than a known failure.
- **Fix:** continue past exit and assert that owner 2 has no pages left and ZShell still works. Launch a shared program repeatedly and compare free pages. Also run a known shared image through `exec(EXEC_PRESERVE_PROGRAM)` and `run_saved`. The `unit-real` execution clients all print, so they probably do not cover this path.

**4. Medium: cold-start failure and interrupt preservation are untested**
- **Gap:** pressure tests cover Memory Run only. Nothing forces `pfree` to fail, and nothing asserts the IFF state at the Pascal entry after a cold start.
- **Risk:** the requirements "failure exits nonzero before Pascal code" and "preserves IFF" rest on inspection alone.
- **Fix:** add a harness that enters the startup with a rejected page, with interrupts both enabled and disabled. Assert exit status, IFF, the alias mapping and owned pages.

**5. Medium: the selector is not tested with runtime helpers**
- **Gap:** every expected two-page fixture is silent. The selector does not exclude `keyboard_*`, `interrupt_entry`, `crt_*`, `random_*` or `error_entry`, although `supported()` for units rejects them.
- **Risk:** those helpers may reach the shared layout unintentionally. `compiler_console.c`, `compiler_io.c`, `compiler_crt.c`, `compiler_keyboard.c` and `compiler_inline.c` were not supplied, so I cannot confirm how they set `console_used` or `console_dynamic`.
- **Fix:** add a layout table for `WriteLn`, `ReadLn`, `Str`/`Val`, `Random`, `KeyPressed`, CRT calls, `{$U+}` and `Inline`. Run the two-page cases in RAM and cold. Confirm that `Inline` forces the independent layout, as `supported()` and the docs imply.

**6. Medium: limits of the shared layout are not exercised natively**
- **Gap:** the host decoder covers the `$B400` limit, but the compiler selector does not have a native test at that limit. No test forces stack exhaustion with the aliased heap floor.
- **Fix:** add these cases:
  - Programs immediately at and above the selector limit, covering both `Start=0` and an explicit Start, and checking that the emitted flag is accepted by `native_decode`.
  - A deep temporary or Real expression near the limit, asserting runtime error FF with the correct PC and intact statics.
  - Save after Memory Run, then run that image cold.

## Questions

**7. Low to medium: what happens when Execute targets a shared child?**
- Normal entry reaches the cold path after `execute_entry` hands off. In a saved parent that should free the mapped page 3, but no test covers it.
- For Memory Run, I need `transfer.c` or `chain.c` to confirm how the child is entered. If a Memory Run parent can enter a child through `normal`, the child could free a page still recorded in `Build`.
- Baseline `normal` entry would also end in an OS exit, so this path may already be refused. Please confirm with a native test.

**8. Low: do existing test oracles still fit the new layout?**
- `same_build_after_run()` in `test-native.py` requires all three `Build.pages` to be owned after Run. A shared build keeps `pages[2]==255`. Check any caller outside the supplied files.
- Fixed-offset probes such as `0x4004` and `0x400a` are correct for `zptest`. Any equivalent checks in `zoptionalcorpus` must allow for the 3-byte branch.
- `test-optional-real.py` now compares only the engine slice, so unintended generated-code changes in independent-layout programs are no longer detected. Consider a normalized comparison that accounts for the branch, header flag and relocations.

## Reviewed with no actionable finding
- **Cold startup:** the emitted bytes total 70, plus the 3-byte branch for the documented 73. Stack order, IFF handling, the failure exit, the `FA00..FFFF` copy and the jump relocation are correct.
- **Bounds:** the selector and decoder agree on the `$B400` limit, including alignment and the 2048-byte stack reserve.
- **Stack guards:** the translated heap floor works with `check_boundary`, and `recursion_limit` remains above the frontier.
- **Memory Run ownership:** `run_build` allocates two owners, repeated Runs keep them, and allocation failure does not add a page to `Build`. `build_discard_image` frees each page once.
- **Fragment requirements:** ABI validation and per-fragment propagation behave as specified, and only selected fragments propagate. Unit recursion and private calls request independent windows.
- **Demand paths visible here:** heap operations, pointers, `Addr`/`Ptr`/`StackPtr`, `HeapPtr`/`RecurPtr`, `Mem`/`Port`, absolute variables, hooks and External all force the independent layout.
- **Source maps:** PC mappings stay consistent with the leading branch.

## Review limits
This was static review only, and I did not run anything. The pending registered results were not treated as passed.

Besides the files named above, `output.c`, `banks.c`, `compile_limits()` and the kernel sources were also unavailable. Conclusions that depend on them are limited accordingly.

### Codex reconciliation

Initial reconciliation (implemented and verified):

1. Accepted. The initial Python oracle did not recognize the new format.
   Following the user scope correction below, writer, reader and wrapper now
   require the single current ABI1 exactly. The general unit gate passes 404
   native assertions and the wrapper; the final helper gate passes 777.
2. Superseded by the user's explicit instruction during this milestone:
   this project is unreleased; compatibility with experimental ABI versions is
   unnecessary, and the first release will be ABI1. The current unit format is
   reset to ABI1, older-version readers and synthetic compatibility tests are
   removed, and the proposed old-artifact fixtures are not retained. The native
   negative tests still reject unknown versions, mismatched runtime fingerprints,
   malformed metadata and unsupported flag bits. Documentation describes this
   single evolving prerelease format. No compatibility guarantee is claimed.
3. Accepted. The driver now continues every standalone past kernel cleanup into
   the shell reader. Normal startup probes launch twice and compare the free-page
   bitmap. The deliberately DI failing child is observed through exit/cleanup
   once; the unmodified OS preserves its disabled interrupt state into the shell.
   A shared native client runs through run_saved and repeated preserving exec;
   another saved Pascal parent Execute targets it.
4. Accepted. Test-only generated images enter startup with EI and DI, inspect
   IFF2 in the body, and force pfree rejection using an invalid page argument.
   Failure must exit1 before the body. Normal probe bodies restore EI only after
   observing IFF2, so subsequent shell input can run. Kernel/ROM sources remain
   unchanged.
5. Adapted conservatively. Unmeasured formatting, Random, keyboard, CRT and
   interrupt helpers now force three pages. A native table covers all requested
   families (ReadLn compiles without blocking; the existing input matrix executes
   it). Inline already requests the independent layout. The documented table
   makes this limit explicit. Native testing caught that ClrScr has no CRT state
   allocation; checking crt_state missed it. CRT demand is now recorded in
   crt_engine for every operation, and the misleading selector check is removed.
   The complete helper table passes with three pages for every requested family.
6. Accepted. Native boundary cases cover at/above cutoff for explicit and default
   Start, including decoder acceptance and actual Run. A debugger-induced high
   frontier exhausts a shared Real program's headroom and checks FF, the guard PC,
   unchanged statics and process cleanup. Clients save again after Memory Run.
7. Accepted. Saved Execute targets the shared child. Direct Memory Execute is
   refused with21 before retiring the valid Build; the native test checks this.
8. Accepted. Both same_build_after_run callers print and retain three pages;
   their existing oracle remains applicable. The broader corpus exposed a real
   bug: a prepended branch moved SETABI's explicit $8004 machine-code constant.
   Startup now redirects the runtime's checked private entry dispatch, preserving
   every original code/data address. Independent files need no leading branch.
   Full Real executable byte identity is restored as a regression oracle.

Additional test corrections before follow-up:

- The new test called near native_decode from BANK1. It now reuses the public
  banked native_header entry from image.h, with a matching CMake dependency.
  This fixes the test's incorrect bank execution, not production decoding.
- The shared Real fault observer originally read child FC1E while a syscall had
  mapped kernel RAM at C000. It now reads the retained user-prefix guard PC at
  40BE. The native FF exit, exact inline PC, unchanged statics and complete kernel
  cleanup pass.

Verification:

- Implementation checkpoint: 03a0c4f. Baseline: e351a8c.
- Final registered gate: analysis/test-runs/20261106T154254.884024Z/suite.json.
  All four selected checks pass, changed_inputs is empty, and no owned child
  process remains. As a focused run, passed=false/selected_passed=true is the
  runner's explicit full-plan versus selected-plan distinction.
- The 777-assertion helper gate and the entire 17-case optional Real/full-runtime
  matrix pass. All 13 page-measurement cases pass, including both execution paths,
  pressure, bounds, startup failure/IFF, persistent state, transfer and cleanup.
- Six independent earlier gates pass at the pre-CRT-tag snapshot: units 404,
  optional-corpus, ide-units, ide-unit-failures, unit-navigation, optional-chain.
  The exact earlier suite statuses, test-only failures and final reruns remain
  distinguishable in the evidence; this is not a full 76-command final-snapshot run.
- docs/program-pages-evidence.json binds three final reports, the earlier
  regression reports, 304 capture files, source hashes and built artifacts. The
  four primary programs use 32768 bytes instead of 49152; files grow 70 bytes.
  Compiler caching remains 10 cold/15 warm code pages. The production ZPL is
  239941 bytes; all banks fit. Normal OS loading still transiently owns 3 pages.
- No SDK, kernel, emulator, original CP/M artifact, or deployment was modified.
  Full TP recovery and hardware qualification remain unclaimed.

No implementation blocker is known; the required follow-up review is next.

## Opus turn 20261106T155927-bd7a1465

Mode: code; read-only

# Review of program-pages-v1 (follow-up)

**Result:** I found no correctness blocker in the two-page implementation, and all eight initial findings are closed or withdrawn. One low-to-medium kernel question and three low qualification items remain. This is a static review, and I did not run anything. Test results below come from the supplied evidence.

## Status of initial findings

| # | Status | Basis |
|---|---|---|
| 1 ABI assertion | Closed | `unit_payload()`, reader and writer all require ABI 1. Unknown versions and flag bits have native negative tests. |
| 2 Old-artifact compatibility | Withdrawn by user | The runtime profile covers `transfer_runtime`, so it changes as expected. `UNIT_STALE` still rejects mismatched units. |
| 3 Kernel cleanup | Closed for child processes | `zos_loader_free_user_pages` scans `_page_owners`, so the alias is freed once and the old page 3 is not freed again. The driver continues into the shell and compares the free-page bitmap across launches. |
| 4 Cold failure and IFF | Closed | The fixtures cover both IFF states and a rejected `pfree`. They check exit status, owned pages and cleanup. |
| 5 Helper eligibility | Closed for the requested families | Keyboard, Random and interrupt state are selector conditions. CRT and `Val` declare the demand directly, and `Str` does so through `RT_STR`. `Inline` sets `console_dynamic`. The native table covers all nine cases, with ReadLn compile-only there and executed in the Real matrix. |
| 6 Limits | Closed | Default and explicit Start cases cover the cutoff, decoder acceptance and Run. The shared guard path and save-after-Run are also covered. |
| 7 Execute into a shared child | Closed | `transfer_dispatch` refuses Memory Execute with error 21 before any owner is retired. Saved Execute into a shared child is covered natively. |
| 8 Test oracles | Closed | Both `same_build_after_run` callers print, so they stay on three pages. Full executable identity is restored for Real cases. |

## Established defects

None.

## Questions and remaining risk

**1. Low to medium: `src/compiler_chain.c`, shared startup when the program is first on the kernel stack**
- **Trigger:** a shared `.COM` runs at `_stack_entries == 0`, for example as the init program or through an override exec from it.
- **Impact:** at this depth, `zos_loader_exit` skips the owner scan and jumps straight to `zos_load_file`. If that path reuses recorded pages, init could load into the page startup already freed. The free bitmap would then still offer that page to `palloc`, giving it two users.
- **Evidence:** lines 343-376 show the separate exit path. The tests cover only children above that level, where owner 2 is observed. Loader lines 1-329 were not supplied, so I cannot tell whether the page is reused.
- **Verification:** inspect `zos_load_file`, or add a native test that exits a shared program at depth 0 and then checks page ownership. Otherwise, document depth-0 launch as unsupported.

## Qualification limits

**2. Low: `docs/program-pages-evidence.json`, `required_checks_passed` is undefined**
- **Issue:** it says `true` beside `registered_suite.passed: false` and `earlier_regressions.selected_passed: false`. The notes explain the difference, but the field name can still be over-read.
- **Coverage gap:** the six regressions ran before the final CRT tag. `test-ide-execute.py` and `test-ide-directory.py` are not in the supplied evidence, even though they are the IDE X Execute and directory workflows.
- **Fix:** list the exact required commands in the evidence. Before calling the full plan qualified, rerun those six and the two IDE tools on the final binaries.
- **Assessment:** the prose in `program-pages.md` matches the evidence and does not overclaim.

**3. Low: `tools/test-unit-real.py`, the 777 assertion count is not pinned**
- **Issue:** the driver only requires `word('_test_passed') > 100`, so dropped cases would still pass. `test-units.py` pins its count at 404.
- **Fix:** pin the expected count, or record it from a checked constant.

**4. Low: `docs/program-pages.md`, "bounds both sides" is slightly too strong**
- **Issue:** the 512-byte check counts `cc_reserve` bytes and `cc.temporaries`. It does not count value arguments that `cc_invoke` pushes outside `cc.temporaries`, including six-byte Reals.
- **Impact:** an extremely long Real parameter list could exceed the intended budget. The runtime guard should still report FF rather than corrupt memory, so the risk is capacity, not memory safety.
- **Fix:** reword this as a bound on reservations, or count argument bytes for imported calls.

## Reviewed with no actionable finding
- **Startup redirection:** `ld hl,(0x4014)` becomes `ld hl,entry` only for shared images, and the entry address includes the placement delta. The patch runs in both RAM and disk compilation, after the Real patch. Evidence matches the 70-byte growth for all four baseline programs.
- **Runtime table:** `RP_PROGRAM_ENTER` is index 25, matching the packager's order and the test's offset of 50. The packager checks the four dispatch bytes, and `pascal_runtime.zpl` keeps its pinned hash.
- **Kernel mapping:** each syscall re-reads the live page 2 and page 3 mappings, so the alias survives `pfree`. Keeping the stack in the high alias also suits the syscall path, which temporarily remaps page 2.
- **Selector limit:** the C test's limit arithmetic matches `cc_startup_finish`, including the 58-byte difference between the 128-byte allowance and the 70-byte stub.
- **Other ownership paths:** Memory Run still uses two owners, and repeated Runs recompute the heap floor. Saving after Run writes only the image length.

## Review limits
I could not inspect the reports, logs or decision record themselves, only the hashes and summaries supplied. Loader lines 1-329, `compiler_files.c` and `transcendental.c` were not provided. Actual Real helper stack depth is therefore unmeasured here, and `Eof`/`Eoln` selection on standard input is unconfirmed.

### Codex reconciliation

Follow-up reconciled; no source or supplied context was changed after this turn.
The result qualifies normal IDE/shell child execution, not every kernel launch
mode or every possible call-frame capacity.

1. Confirmed and documented as an unsupported launch mode. Read-only inspection
   of the full pinned loader (SHA256
   b597fda6384869ce1e970859670d0a8691af95c2dd69bb91020032f3656f3a6e)
   shows zos_load_file / _zos_load_file_checked reusing _allocate_pages, while
   depth-0 exit bypasses zos_loader_free_user_pages. Therefore a two-page binary
   must not be installed as init or entered by override exec from init. Normal
   shell/IDE children, preserving exec and saved Execute at nonzero depth remain
   the tested supported paths. For depth-0 use, select an existing nondefault End
   such as FBFE to retain three independent pages and avoid the freeing stub.
   No kernel or SDK customization was attempted. This scope restriction is
   reported to the user and is a qualification limit, not a claim of safe init
   support.
2. Adapted by defining the evidence field here. required_checks_passed means this
   milestone's final selected commands all passed, with unchanged inputs:
   - python3 tools/test-native-host.py --sanitize
   - python3 tools/test-unit-real.py
   - python3 tools/test-optional-real.py
   - python3 tools/test-program-pages.py
   Their exact interpreter/arguments/status are already enumerated in
   registered_suite.results. The six earlier positive regression commands are
   separately enumerated in earlier_regressions.results_retained; that earlier
   suite remains recorded as failed overall. No full-plan or final-snapshot
   IDE X/directory qualification is claimed. The generic field name can be made
   more explicit in a future evidence-schema cleanup without changing these
   frozen results.
3. Deferred, nonblocking test-maintenance improvement. The current native report
   and capture are hash-bound with exactly 777 assertions, zero failures and
   test_passed==test_index. The reusable driver's >100 floor is not a permanent
   count-regression guard. Pinning 777 (and updating it intentionally as new cases
   are added) remains follow-up test maintenance. No test case was waived here.
4. Accepted capacity qualification. Read the documented 512-byte per-side bound
   as a bound on cc_reserve plus cc.temporaries, not a complete accounting of
   every pushed value argument. Very long imported argument lists can remain
   eligible for two pages and then exhaust that smaller stack; the runtime guard
   reports FF before overlap. Counting all argument bytes for automatic fallback
   remains a future improvement. A nondefault End forces three pages when that
   additional capacity is required. No guarantee of unchanged capacity for
   arbitrarily long calls is made by this milestone.

Additional read-only clarification: compiler_files.c cc_file_function routes
Eof/Eoln on standard Input through cc_standard_reference and the file-call engine.
That engine binds file_entry, standard_table_operand and file_stream_operand,
which the selector excludes. This supports the conservative classification; no
new native standard-predicate qualification is claimed. Real helper reservations
remain the existing 256-byte contract; exact worst-case C stack depth has not been
measured by this milestone.

All eight initial findings are closed or withdrawn. The follow-up found no
established production defect. The remaining init-mode restriction, large-call
capacity limit, assertion-count maintenance and broader unrun plan are explicit
qualification limits; there is no blocker for the measured child-process scope.
The implementation remains 03a0c4f, the final four-check gate remains passing, and
all published source/build/capture hashes were rechecked before finalization.
