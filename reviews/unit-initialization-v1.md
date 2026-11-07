# Review: unit-initialization-v1

## Opus turn 20261107T004411-937ab66f

Mode: code; read-only

## Review: unit initialization v1

**Outcome:** I found no established product-code defect in the supplied implementation. I found one established test defect that should fail `zunitinittest`, one suspected test defect that needs `read_block` context, and several coverage gaps. No build or test ran in this review, and I treated the native build and emulator results as pending.

### Established defects

1. **Medium: the empty-body allocation fixture contradicts existing shared-stack behavior**
   - **Location:** `tests/native/unit-init-main.c`, `client("H:/EMPTYUSE.PAS", ..., 0)`, with the `==!full` check in `client`.
   - **Trigger and impact:** `full=0` requires `NATIVE_SHARED_STACK`, but the program calls `writeln`. The assertion should fail even if empty-body root omission is correct, so it cannot prove the intended allocation property.
   - **Evidence:** `layout_helpers` in `tests/native/unit-real-main.c` asserts that `begin WriteLn end.` is not shared-stack eligible. The bare `WriteLn` case does not exercise the literal-argument path, so the native run must confirm this result. Existing shared-layout fixtures avoid output, such as `SHARED.PAS` and `TYPELEAF.PAS`.
   - **Fix:**
     - Keep the `writeln` marker program with `full=1`.
     - Add a separate no-output program such as `uses Empty;var N:Integer;begin N:=1 end.`.
     - Assert that the second program is shared and that `b.pages[2]==255`.
     - Keep the Python `EMPTY` count at 3 if the marker program remains.

2. **Medium: Chain ordering has no direct coverage**
   - **Location:** `src/compiler_compile.c`, `cc_activation_begin` followed by `cc_units_initializers`; `tests/native/unit-init-main.c`.
   - **Trigger and impact:** The code order matches the contract, but a regression in Chain prefix or fixed-entry placement would not fail this suite. That placement is the only path where the new CALLs follow `CHAIN_ENTRY_BYTES`.
   - **Evidence:** All fixtures use `compile()`, `compile_unit()`, saved execution, or `exec`. None use `COMPILE_CHAIN`.
   - **Fix:** Add a CHN fixture that uses an initialized unit. Verify that the program sees initializer results and that the chained activation preserves the existing CRT reset and binding behavior.

3. **Medium: three contract paths lack focused tests**
   - **Location:** `tests/native/unit-init-main.c`.
   - **Trigger and impact:** The implementation appears correct by inspection, but these regressions would currently pass.
   - **Missing cases:**
     - An initializer reached only through an intermediate unit with no initializer, with no exports referenced. `initializer_visit` is intended to handle this, but `Empty` has no dependency.
     - Parser scope failures in an initializer, specifically an undefined forward label (code 40) and a routine `goto` targeting a root label (code 72).
     - `Exit` inside `with` or `for`, because the current `Exit` fixture has neither.
   - **Fix:** Add one positive transitive runtime fixture and focused authoring rejection or runtime cases.

4. **Low: `tools/test-unit-init.py` does not pin the assertion count**
   - **Location:** the `all native assertions pass` check.
   - **Trigger and impact:** `word('_test_passed')>0` would accept a run with an unintentionally reduced assertion path. `tools/test-units.py` and `tools/test-unit-real.py` both pin exact totals.
   - **Fix:** Add `EXPECTED_ASSERTIONS` after the first valid native run.

5. **Low: the new test target has an incomplete dependency list**
   - **Location:** `CMakeLists.txt`, `OBJECT_DEPENDS` for `tests/native/unit-init-main.c`.
   - **Trigger and impact:** The test uses `Sources` and `sources_get`, but `sources.h` is not listed. This SDK has no compiler depfiles, so an incremental build could keep a stale object. `unit-real-main.c` receives this dependency explicitly.
   - **Fix:** Append `${CMAKE_SOURCE_DIR}/src/sources.h`.

### Questions requiring confirmation

1. **Can `read_block` safely load a binary `.ZPU`?**
   - **Location:** `tests/native/unit-init-main.c`, `malformed()`.
   - **Concern:** `SourceImport` tracks CR, trailing spaces, and EOF, which suggests text normalization. `unit-real-main.c` uses a separate binary `read_artifact` for this kind of test.
   - **Possible outcomes:**
     - If NUL or control bytes are rejected, the test fails visibly.
     - If bytes are normalized, `word6` or the blob offsets may become invalid. The fixture could then report `UNIT_FORMAT` before reaching the word46 check, which would be a false pass.
   - **Needed context:** Supply `read_block` and the import implementation, or replace this path with `page_new(PAGE_COMPILER)` plus raw `read()`. The second option is the safer fix either way.

2. **Do other tests still expect initialization to be rejected?**
   - **Concern:** `tools/test-ide-units.py`, `tools/test-ide-unit-failures.py`, and `tools/test-unit-navigation.py` were not supplied. Any fixture that uses `begin` in a unit as a rejection case now has the wrong expectation.
   - **Verification:** Search those tools and the docs for that expectation.

3. **Is an unmapped prefix guard acceptable?**
   - **Location:** `src/compiler_units_graph.c`, `initializer_visit`.
   - **Concern:** The guard and CALL prefix has no `cc_mapping` entry. Errors inside initializers are mapped, but a failure at the prefix guard itself may have no source location.
   - **Decision needed:** Confirm this is acceptable, or map the prefix to the program's `uses` clause or first statement.

### Audit conclusions by requested area

| Area | Result from supplied code |
|---|---|
| **Parsing and scope** | The empty-body probe restores the cursor and re-lexes `begin`. The anonymous symbol has no name and cannot match identifier lookup. Global declarations remain in scope 0, root labels move to the initializer scope, and `cc.scope` and the fragment owner are restored. |
| **Fragment boundaries** | `cc_units_owner` closes the owner-0 span before `UR_INITIALIZER` is recorded, so the entry is at offset 0. The trailing `cc_finish` remains in a later owner-0 fragment. The block `JP` targeting the initializer is an ordinary relocation. |
| **Roots and relocation** | `UR_INIT_CALL` sites use `cc_link`, so they are relocated. They are patched to fragment addresses from the layout and rooted independently of imports. Transitive retention and Real or full-memory flags use the ordinary fragment path. |
| **Initialization order** | Program and manifest modules are stored in load preorder. DFS emits dependencies first, and the visited bit prevents duplicates. The diamond expectations `123` and `132` match this logic. |
| **Bank lifetime** | The new traversal runs from warm `BANK16` code and calls only banked or resident services. It does not need transient unit I/O or metadata pages. Bank-size overflow remains subject to build evidence. |
| **Malformed input** | `word46>k` rejects both out-of-range indices and the globals fragment. Dependency IDs are validated before traversal. Error paths in `initializer_visit` do not mutate state. A checksum-valid artifact can still name a different code fragment, which matches the existing trust model. |

### Review limitations

- **Missing source:** I did not receive `output.c`, `compiler_emit.c`, `compiler_build.c`, `compiler_expr.c`, `compiler_io.c`, `cc_startup_finish`, `cc_runtime_statement`, or `tools/test-native.py`.
- **Unproven runtime behavior:** Stack, bank-fit, `Mark` name resolution, and execution behavior remain unproven until the ZDE build and emulator run complete.
- **Not reviewed:** The referenced `library-exploration.md` contract and `docs/unit-init-evidence.json` were not supplied.

### Codex reconciliation

1. Accepted: keep output-marker fixture in full-window mode; add EMPTYRAM without I/O and assert shared-stack flag and absent third page.
2. Accepted: CHN fixture imports InitChain, whose initializer calls CrtInit/GotoXY/CrtExit and captures WhereX/Y. Parent activates CRT at7,9, then chains. Child observes709, proving activation reset precedes its initializer. The first saved parent run succeeds with libraries present; the second succeeds after deleting the custom unit libraries. Stock CRT was still present in that reviewed run; the follow-up correction deletes it too. No cross-image shared-unit contract was added.
3. Accepted: Bridge has no initializer or called export but retains InitReal's initializer transitively. An observable Track accumulator proves execution. Added undefined label40 and cross-routine goto72 authoring failures. Left initializer now exits from nested With/For and leaves the expected accumulator.
4. Accepted: exact621 assertions pinned and rerun successfully.
5. Accepted: sources.h added to the target object's explicit dependency list.

Questions: read_block is a canonicalizing text reader, so replaced it with page_new(PAGE_COMPILER) and raw SDK read, checked header length before mutations and recomputed checksums. Both invalid indices reject248. Searched IDE failure/navigation/unit tools; none expect initialization rejection (only malformed routine syntax). Added mapping for each prefix guard/CALL to the program token following uses, closing the potential unmapped-error gap.

The first emulator attempt stopped at Chain's FC00 commit trampoline rather than supervisor completion. Adopted the established CRT driver pattern: inspect the mapped supervisor image, step past commits from other images and accept only authenticated supervisor completion. The corrected run passes621 native assertions and16 Python checks. Production packaging passes all18 bank bounds, launcher15546 bytes, ZPL255407 bytes; no new cold/warm page. Host52 ASAN/UBSAN assertions pass. Regression checks now pass1703 dependency/type assertions,502 CRT assertions,66 IDE checks and51 failure/cancellation/retry checks. See docs/unit-init-evidence.json. No product defect was established by the initial review. The additional older-unit compiler rerun is still building; its result will be recorded in final reconciliation.

## Opus turn 20261107T010256-57d0cf49

Mode: code; read-only

## Follow-up review: unit initialization v1

**Outcome:** All five initial findings and all three open questions are resolved, and I found no new product-code defect. One new build-dependency defect remains. I recommend one low-risk correction, plus one evidence correction and two test additions. This turn was read-only: I ran no builds or tests, and all results below come from the supplied evidence.

### Prior findings

| Item | Status | Assessment |
|---|---|---|
| Empty-body allocation | **Resolved** | `EMPTYUSE` now expects full-window mode. `EMPTYRAM` has no output, and it checks the shared-stack flag and absent third page. Evidence records two execution pages. |
| Chain coverage | **Resolved, one caveat** | `chain_activation` runs an initializer in a real CHN. `cc_activation_place` jumps to the code that clears `crt_active`, and the initializer calls follow it. The `709` oracle depends on `screen-engine.inc`, which was not supplied. |
| Contract-path tests | **Resolved** | `Bridge` covers transitive initialization through a unit without an initializer. `BadInit` covers diagnostics 40 and 72. `Left` exits from nested `with` and `for`. |
| Assertion pin | **Resolved** | The driver and evidence both pin 621 assertions. |
| `sources.h` dependency | **Resolved** | It is now listed in `OBJECT_DEPENDS` for `unit-init-main.c`. |
| `read_block` binary use | **Resolved** | `import_byte` confirms the old path was text-normalizing. `malformed()` now uses raw reads and checks word 6 before changing the artifact. |
| Stale rejection expectations | **Resolved** | The supplied IDE, failure, and navigation tools do not expect initialization to be rejected. |
| Prefix mapping | **Resolved in code, untested** | `initializer_visit` maps each guard and CALL to the token after `uses`. See finding 4. |

### Established defects

1. **Medium: `zunitinittest` does not depend on `screen_library`**
   - **Location:** `CMakeLists.txt`, `zunitinittest`; `tools/run-tests.py`, `native_targets`; `tools/test-unit-init.py`, `fixtures['CRT.ZPU']`.
   - **Trigger:** Run a focused `--only unit-init` build on a clean tree, or after a runtime-profile change. That build requests only `zunitinittest_bin`.
   - **Impact:** The test can fail on a missing `build/CRT.ZPU`. A stale copy can instead make `InitChain` fail with `UNIT_STALE`, which is a false product failure.
   - **Evidence:** The driver reads that file directly. `zunitrealtest`, `zscreentest`, and `zpascal` declare this dependency, but `zunitinittest` declares only `transfer_assets`. A full plan or default build can hide the gap.
   - **Fix:** Add `add_dependencies(zunitinittest screen_library)`, then rerun the focused target through `tools/run-tests.py --only unit-init`.

### Coverage and evidence findings

2. **Medium: the IDE saved-compile path has no initializer fixture**
   - **Location:** `tools/test-ide-units.py`, `UNIT` and `CLIENTS`; `tests/native/unit-init-main.c`, `client`.
   - **Trigger and impact:** The initializer fixtures compile in memory and then call `save_build`. The IDE fixture's `ocq`/`C` path leaves no executable build and uses `Calc`, which has no initializer. A regression in the backward patch of `UR_INIT_CALL` sites during saved compilation would pass this suite.
   - **Evidence:** Saved compile releases its image, as the `saved compile releases image` check shows. `compiler_disk.c` was not supplied, but import call chains use the same `cc_word` patching, so I do not assert a defect.
   - **Fix:** Add a small IDE case that authors a unit with `begin ... end.`, compiles a client through `C`, and runs the standalone COM after removing the unit artifacts.

3. **Low: the library-deletion claim is overstated**
   - **Location:** `tests/native/unit-init-main.c`, `chain_activation` and `app_main`; Codex reconciliation.
   - **Trigger and impact:** The reconciliation says both saved parent runs succeed after library deletion. The first `run_saved` happens before stage 3 deletes `INITCHAIN.ZPU`. `CRT.ZPU` is never deleted, including before the final `exec`.
   - **Fix:** Correct the reconciliation and evidence wording. Preferably delete `H:/CRT.ZPU` before the stage 3 `exec` calls, add `CRT` to the Python deletion check, and update the assertion pin. `INITCHILD.CHN` must remain because it is a legitimate runtime file.

4. **Low: prefix mapping has no direct assertion**
   - **Location:** `src/compiler_units_graph.c`, `initializer_visit`; `tests/native/unit-init-main.c`.
   - **Trigger and impact:** Removing the new `cc_mapping` call would not fail any supplied test.
   - **Fix:** In `fault_location`, call `build_locate(&s,&b,0,&position)` before the runtime fault. Assert that `position` is the offset of `begin` in the root source.

### Verification still required

- **Older unit target:** `zunittest` remains pending. Do not report it as passing until its result is recorded.
- **Macro-on suites:** Evidence does not report `test-optional-corpus`, `test-optional-real`, `test-optional-chain`, `test-unit-navigation`, or `test-program-pages`. These matter because `cc_block` changed in macro-on builds, its recursive frame grew by `SourceCursor`, and shared-window selection is affected. They should pass, or be explicitly recorded as not run, before acceptance is claimed.
- **Runner tests:** `tests/test_runner.py` and `tests/test_native_chunks.py` were not supplied. Run them because `tools/test-plan.json` changed.
- **Chain mutation check:** Temporarily swap `cc_activation_begin` and `cc_units_initializers`, then confirm the `709` fixture fails. This proves the oracle discriminates order without depending on `screen-engine.inc` by inspection.
- **Evidence refresh:** A `CMakeLists.txt` or test change alters recorded hashes, so regenerate `docs/unit-init-evidence.json` after any correction.

### Questions

1. **Should the Chain limits be stated more explicitly?** `library-exploration.md` says not to claim that units and legacy Chain compose. The current doc is close, but it should say that each CHN initializes its own image-local unit copy and that parent unit state is not visible to it.
2. **Does `docs/specification.md` need a readiness update?** `AGENTS.md` requires scope tracking there, and this diff does not touch it. The file was not supplied.

### Review limitations

- **Missing source:** I did not receive `compiler_disk.c`, the `cc_mapping` implementation, `cc_startup_finish`, `libraries/screen-engine.inc`, or `src/main.c`.
- **Unavailable capture data:** Results and hashes come from `docs/unit-init-evidence.json`. I could not inspect the referenced capture directories.

### Codex reconciliation

1. Accepted: zunitinittest now depends on screen_library and the SDK supervisor target needed by the added mutation test. The focused orchestrator (`tools/run-tests.py --only unit-init`) builds and passes independently.
2. Accepted: the IDE's Calc fixture now owns private Bias initialized from Adjust; Plain requires this initializer. All66 IDE checks pass, including streamed saved compilation, standalone COM execution without source/libraries and TF/EEPROM saved compilation.
3. Accepted: corrected the earlier wording distinguishing the first pre-deletion run from the later post-deletion run. The final focused fixture now deletes CRT.ZPU as well as custom unit sources/artifacts before standalone execution. INITCHILD.CHN remains an intentional runtime file.
4. Accepted: direct build_locate at PC0 asserts the program's begin token before the initializer fault. The focused suite now pins623 assertions and17 Python checks.

Additional checks: the older unit compiler passes404 assertions and13 Python checks. Navigation passes15 checks. The focused orchestrator passes all selected work including the host ASAN/UBSAN gate (179718 front-end/identity/allocation,104+98 raw-key,4010 editor and45809 source assertions),24 runner tests and6 chunk tests. It initially exposed obsolete plan totals76/150; current totals78/152 now pass. Full optional corpus/Real/Chain and program-pages native suites were not rerun. This is focused qualification, not full acceptance or hardware qualification; macro-off front-end behavior was exercised by the sanitized host prerequisite.

Chain ordering: strengthened the initializer to call CrtExit before taking its own CrtInit snapshot, so the borrowed active flag matters. The driver rotates the four-byte CRT-reset instruction behind the following guard/CALL in a disposable saved CHN; no product source or shipped artifact is modified. The mutated child reliably returns runtime status2 rather than reaching its success marker, while the correct image passes. This demonstrates the oracle discriminates order.

Questions: documentation now explicitly states each CHN initializes its own image-local unit copy and cannot see parent unit state. The ignored local docs/specification.md readiness log was updated throughout; its large pre-existing research is intentionally not staged. Evidence hashes have been refreshed in docs/unit-init-evidence.json.

No remaining established product-code defect or unresolved blocker. No product compiler source changed after the follow-up. Build dependencies, test fixtures and documentation changed to resolve its findings, so the workflow is left unfinalized rather than reusing an unchanged-input completion attestation. Both authorized Opus turns are complete; these final corrections were verified locally, without an extra round.
