# Review: pascal-units-v1

## Opus turn 20261105T060307-18334f95

Mode: code; read-only

# Review of pascal-units-v1 (initial turn)

**Verdict: not ready.** I found no proven defect in the `.ZPU` format, relocation arithmetic or fragment planner. However, the unit compiler can still accept code it should reject, and at least one path can silently produce wrong addresses. Items 1–3 should block completion, subject to the checks noted in items 2–3.

I traced these paths by hand and found no defect:
- ZPU and ZPF offsets, and module and seal record fields
- identity binding, fragment partitioning and reference ordering
- guard PC relocation and map clipping
- layout and image cleanup, and banked near-pointer calls

I ran nothing, and I'm not treating any test result as passing evidence for the current edits.

## Established defects

**1. High: the unit feature gate does not reject everything it must**
- **Location:** `supported()` in `src/compiler_units_object.c`; `cc_lookup_decl` in `src/compiler_front.c`
- **Trigger:** a unit containing `BufLen:=1`, `HeapPtr:=…` or `RecurPtr:=…`.
- **Impact:** the unit authors successfully and mutates runtime state from imported code, although runtime-mutating helpers must be rejected.
- **Evidence:**
  - Only `RT_CBREAK` sets `console_dynamic`. The other `S_SYSTEM` variables set nothing, and `supported()` doesn't check them.
  - `supported()` also ignores `console_used`, `standard_table_operand`, `file_stream_operand`, `keyboard_poll/status/state/probe/enqueue`, `keyboard_console_probe`, `crt_state/row`, `random_state` and `hook_character/echo/open`.
  - The unit path returns before every `cc_*_finish` call.
  - Forward chains that use `cc_address` fail closed, because the chain's terminal value 0 isn't in any fragment. Operands patched without relocation have no such protection.
- **Fix:** have each runtime-dependent feature set an explicit unit-unsupported flag, or use an allowlist. Add negative tests for system variables, `New`/`Mark`/`Release`, `Write`/`Read`, Text, `KeyPressed`, CRT and `Random`/`Randomize`.

**2. Medium: declaring a local routine can fail with a false ambiguity error**
- **Location:** `routine()` in `src/compiler_routines.c`
- **Trigger:** two imported units export `Clash`, and the program declares `procedure Clash;`.
- **Impact:** compilation fails with `UNIT_AMBIGUOUS`, although the docs say local declarations shadow imports.
- **Evidence:** `routine()` calls `cc_lookup()`, which falls through to `cc_units_lookup()`. `cc_new_symbol` was moved to `cc_lookup_decl`, but this call was not.
- **Fix:** use `cc_lookup_decl()` in `routine()`, and add ambiguous and non-ambiguous shadowing tests.

**3. Medium: a unit source and an include can alias one descriptor, leaving page 255**
- **Location:** `cc_units_load` in `src/compiler_units_object.c`; `cc_source_include` in `src/compiler_sources.c`
- **Trigger:** `uses Foo; … {$I FOO.PAS}`, or the same include placed before `uses`.
- **Impact:** the parser reads from page 255, and the include-first order also overwrites the include's live text descriptor.
- **Evidence:**
  - `cc_units_load` rejects only `sources_find(...)==0`. It then calls `sources_reference` with `page=255`, which reuses any existing ID.
  - `cc_source_include` skips loading when `id>0`, then seeks into the descriptor. `get()` calls `buffer_view_get(255,…)`.
- **Fix:** reject any existing ID in `cc_units_load` unless it is already the same fingerprinted unit. Reject includes that resolve to a text-less descriptor.

**4. Low: duplicate interface headings produce an unusable artifact**
- **Location:** `routine(1)` and `cc_units_export`
- **Trigger:** `procedure P; procedure P;` in an interface.
- **Impact:** the second heading matches the forward declaration and adds another `UR_EXPORT`. Authoring succeeds, but import fails with `UNIT_FORMAT`.
- **Fix:** fail during authoring when a routine is already exported, and add a test.

**5. Low: runtime-copy failure may be silent**
- **Location:** `cc_image_page` in `src/compiler_build.c`
- **Trigger:** `cc_runtime_copy` fails while `cc_put` is materializing page 0.
- **Impact:** if `cc_runtime_copy` doesn't set an error itself, `cc_put` drops the write and `pages[0]` is no longer 255. A later `cc_image_page(0)` then succeeds with a zero prefix.
- **Fix:** call `cc_fail(99)` on that branch.

**6. Low: a clean single-target build can have ordering problems**
- **Location:** `unit_core` and `zunittest_bin` in `CMakeLists.txt`
- **Trigger:** building only the unit target from a clean tree.
- **Evidence:** `unit_core` compiles the runtime and file assets but lacks `zp_core`'s dependencies on `runtime_profile`, `file_runtime_bin`, `file_gate_bin` and `module_unpack_bin`. `zunittest_bin` is also not part of `ALL`.
- **Fix:** add the same target dependencies, and make sure the test script builds `zunittest_bin` explicitly.

**7. Low: harness failure path closes an unowned Build**
- **Location:** `app_main` in `tests/native/units-main.c`
- **Trigger:** either authoring step fails and jumps to `done`.
- **Impact:** global `b` is zero-initialized, so `build_close(&b)` treats page 0 as owned. This can mask the original failure.
- **Fix:** initialize `b` to sentinel values before the first `goto done`.

## Probable high-priority issues needing unseen code

**8. High, if confirmed: raw workspace addresses can bypass `UR_GLOBAL`**
- **Location:** `src/compiler_inline.c` first, then all global-address emitters
- **Trigger:** a unit uses a workspace variable as an Inline operand, or another path emits a global address directly.
- **Impact:** the address keeps its preferred `0x8000+n` value after linking, so generated code can read or write the wrong memory. Nothing in the unit path detects this.
- **Evidence:**
  - The only `compiler_inline.c` change rejects imported routines.
  - Its comment says recursive slots "retain static workspace addresses".
  - Only `storage_address` and recursion `block()` were converted to `UR_GLOBAL`.
- **Verification:** audit every `cc_op16` that takes a symbol address or adds a folded offset, especially Inline, indexing, record fields, `for`, `case` and `with`. Fail closed for anything unconverted, and add execution tests with nonzero global offsets.

**9. Medium to high, if reachable: unit globals lose initializers, and success can leak a stage page**
- **Location:** `cc_global_put` in `src/compiler_build.c`; `cc_units_write`; the unit return in `compile_project`
- **Trigger:** any accepted unit construct that writes a compile-time global initializer.
- **Impact:**
  - `cc_units_write` always stores workspace as zeros, so the initial value is lost.
  - `separate_globals()` would allocate `primary_stage`.
  - The unit success path returns without dropping `primary_stage` or `stage_page`, and `memset(&cc,…)` then loses those handles on the next compile.
- **Fix:** reject `cc_global_put` in unit mode, and release both stage pages on the unit success path.

## Questions to confirm

1. **`read_block` failure:** does it always set `loaded.page=255`? `cc_units_load` and `sources_ready` call `sources_release(&loaded)` after failure, while `cc_source_include` does not. If the block can stay uninitialized, `source_close` may act on garbage.
2. **`source_close`:** does it preserve `path`, `length` and `revision`? `cc_units_load` reuses them for `sources_reference`. The `build_check` assertion should expose a failure if they change.
3. **Imported routine addresses:** does `cc_routine_address` route imports through `cc_entry_reference`? I need the complete `compiler_bridges.c` to answer this. Callback descriptors and thunks for imported routines are also unverified.
4. **Map handling:** can `cc_mapping` safely run after `cc_sources_finish`, and does it depend on append order? It now appends unit maps at link time.
5. **Deferred page 0:** does any code bypass `cc_put` with `far_get` or `far_set` on `pages[0]`?
6. **Flag collisions:** does anything else use `R_FLAGS` bit 16? `compiler_groups.h` and `compiler_late.h` weren't supplied.
7. **Overlays:** `materialize` skips `cc_groups_scope`. Should `uses` be rejected when overlay groups are active?

## Priority test gaps

1. **Macro-on regression:** run the existing corpus against the `ZP_UNITS_EXPERIMENT` compiler. Deferred prefix and CBreak handling and the lookup wrapper affect ordinary programs, but `zunittest` covers only a small client. Also confirm macro-off bank budgets and behavior.
2. **Unit error positions:** raise runtime errors inside retained units and check file ID and offset with `build_location`. Cover nonzero `Start`, multiple fragments, private routines and saved images.
3. **Import rejection:** test stale source, profile change, corrupt checksum, truncation, oversize, wrong name, non-canonical path, missing file and a qualified miss.
4. **Cleanup and retry:** inject allocation, interrupt and `page_drop` failures through load, plan, emit and close. Include `cc_units_hold` retry on the next compile.
5. **Generated code:** cover workspace access, nested and forward routines, `{$A-}` recursion, typed constants, literals and VAR parameters. Also test every primitive type.
6. **Selection proof:** assert that `Unused` is absent from the image, not just that the client runs.
7. **Self-containment and limits:** run the saved COM with the `.ZPU` and `.PAS` files removed. Test the 32-module and 128-root limits, and record-page exhaustion.

## Review limitations

These files were not supplied, so their behavior is unverified:
- `compiler.c`, `compiler_expr.c`, `compiler_strings.c`, `compiler_console.c` and other feature emitters
- full `compiler_inline.c`, `compiler_bridges.c` and `compiler_recursion.c`
- `compiler_tables.c`, `source.c` and `output_transport.c`
- `compiler_job*` and `package-native.py`

Items 1, 8 and 9 depend most on this code. The missing files could confirm further unannotated global-address or guard-PC emitters, or could show that some paths are already safe.

### Codex reconciliation

1. Accepted. All S_SYSTEM use marks the unit unsupported, including BufLen/HeapPtr/RecurPtr. Heap and StackPtr intrinsic paths also mark it. `supported()` now checks console_used and the remaining optional engine state/forward operands. Fourteen native negative helper cases pass (system variables, New/Mark/Release, Write/Read, Text Assign, KeyPressed, Random/Randomize, CRT and Inline).
2. Accepted. Routine declaration lookup now uses cc_lookup_decl, just like symbol creation. Native two-unit ambiguous lookup rejects; a local function with that same name shadows both imports and executes correctly in RAM/COM.
3. Accepted. Unit source loading rejects any pre-existing source ID; module de-duplication happens before that loader. Includes reject a pre-existing descriptor with page255. Native uses-Vars then include-VARS.PAS rejects with93 and restores pages.
4. Accepted. cc_units_export detects repeated symbol exports and rejects at authoring with UNIT_INTERFACE. Native duplicate-heading negative passes.
5. Accepted. Failed runtime_copy calls cc_fail(99), preserving a compilation error instead of silently accepting a zero prefix.
6. Accepted. unit_core explicitly depends on runtime_profile, file_runtime_bin, file_gate_bin and module_unpack_bin as well as prior runtime/transfer prerequisites. Both test scripts explicitly build their experimental _bin targets. Experimental targets intentionally remain outside ALL. Evidence uses incremental ZDE builds; clean-toolchain reproducibility is not claimed.
7. Accepted. The harness initializes Build sentinel pages before any goto done.
8. Rejected as a current silent-address defect after tracing full compiler_inline.c: cc_inline already marks console_dynamic on line60, so supported() rejects every unit Inline body. The native Inline negative passes. The imported-routine symbolic Inline rejection remains. Audited direct CCS/global address operands across compiler sources: S_CONSTANT scalar immediates are values, S_ADDRESS is deliberately absolute, S_TYPED uses cc_address; variable load/store/address uses storage_address and recursive block pointers use cc_units_global. No additional raw workspace emitter found. Full relevant files are supplied in follow-up.
9. Adapted defensively. cc_global_put has no callers in the current source tree, and typed constants are emitted into code by cc_typed_storage, with ordinary relocations. Nonetheless cc_global_put now rejects unit mode and a common release_stages helper runs on unit success and ordinary success. Unit artifact code+workspace is bounded below16KiB; no accepted unit needs second staging. A native unit with a private typed constant plus Byte/Char/Boolean VAR and Boolean result executes in RAM/COM.

Questions: (1) resident read_block can reject only a name longer than127 before backend initialization; loader and readiness paths use already bounded canonical stored paths. Backend files_read_block always initializes page255 before I/O and cleans failures. (2) source_close sets only page255; length/path/revision remain, as current Build readiness tests also prove. (3) cc_routine_address routes top-level entries through cc_entry_reference, including import fixups; full bridges source supplied. (4) cc_mapping uses only Sources descriptors/length and map storage, not dead reference text; appended unit ranges follow the root code, with native unit fault location verified. (5) no compiler prefix page0 far_get/far_set bypass found; cc_put/cc_image_page own writes. (6) RF flags currently1,2,4,8; imported16 is free, groups/late do not reuse it. (7) overlay parsing is disabled; no live grouped compile invocation exists; Chain imports explicitly reject.

Evidence: docs/pascal-units-evidence.json records279 native unit assertions,862 sanitized host/801 native fragment assertions,1982 macro-on corpus assertions across15 noninteractive families, and normal macro-off64 control/75 routine assertions, all passing. The final unit gate is analysis/units/20261105T063645.253402Z-94272/report.json. Adding380 unused payload bytes leaves the17634-byte COM byte-identical; library/source files are removed before direct SDK execution, which prints the fifth expected marker. Runtime fault maps to FAULTS.PAS and its original statement, and saved execution returns2. All compiler-code captures match and page balances pass. The corpus ran before the final Kinds-only unit harness extension; its actual compiler sources/artifacts are unchanged.

Remaining scope: no IDE/worker authoring integration, source-absent import, dependencies/init/exported type graph, Real/Screen extraction, or whole runtime-page saving. Container fuzzing, every limit, and injected failures throughout the unit loader are not exhaustively tested. These are documented limitations of this bounded experimental milestone, not production readiness claims. Initial observed OOM and a mixed-object build were corrected; final evidence uses a consistent core rebuild. The author fixture now creates fresh output files as required; HostFS rewriting an existing artifact had failed after64 bytes, and that failure was caught rather than published.

## Opus turn 20261105T064147-29f6aa15

Mode: code; read-only

# Follow-up review of pascal-units-v1

**Verdict: I found no concrete blocker for the documented experimental subset.** This assumes the supplied evidence is accurate. I checked the summary's arithmetic (the 15 corpus families total 1982), but I ran nothing and couldn't inspect `docs/pascal-units-evidence.json`. This isn't a production-readiness finding.

One correction is needed before closure: the "all primitive kinds" claim overstates the tests (item 1). The widest remaining risk is that positive unit tests barely touch private-implementation features (item 2).

## Prior findings

| Item | Status | Reassessment |
|---|---|---|
| 1. Feature gate | **Resolved for reviewed code** | System variables, heap operations and `StackPtr` assignment now set the reject flag, and `supported()` checks the optional engine state. Unsupplied emitters still rely on negative tests. |
| 2. False ambiguity | **Resolved** | `routine()` uses `cc_lookup_decl()`, and both the ambiguity and shadowing cases are tested. |
| 3. Source aliasing | **Resolved** | Any existing source ID is rejected on load, and an include that resolves to a textless descriptor is rejected. |
| 4. Duplicate headings | **Resolved** | `cc_units_export` rejects them during authoring. |
| 5. Runtime-copy failure | **Resolved** | `cc_image_page` now calls `cc_fail(99)`. |
| 6. Build dependencies | **Resolved, with a limit** | The dependencies were added and the scripts name their `_bin` targets. A clean build is not claimed. |
| 7. Harness Build | **Resolved** | Sentinels are set before any `goto done`. |
| 8. Inline bypass | **Withdrawn for Inline** | `cc_inline` sets `console_dynamic` whenever it isn't suppressed, so `supported()` rejects it. The supplied expression, type, bridge and recursion emitters show no raw workspace-address path. `compiler.c` remains unaudited (item 2). |
| 9. Initializers and stage leak | **Resolved** | `cc_global_put` rejects unit mode, and `release_stages()` runs on the unit success path. |

**Earlier questions:**
- **Q1 and Q2 closed:** `files_read_block` initializes `page=255` first, and `source_close` changes only `page`.
- **Q3 closed:** `cc_routine_address` sends top-level imports through `cc_entry_reference`.
- **Q4 closed:** `cc_mapping` needs only source descriptors and map storage.
- **Q6 closed:** `compiler.h` defines flags 1, 2, 4 and 8, and the supplied group and late headers add none.
- **Q7 closed for this path:** `cc_block` has no overlay parse branch.
- **Q5 closed only for supplied code:** I found no direct `pages[0]` bypass, and the macro-on corpus passing supports this. `compiler_chain.c` and other unsupplied files weren't checked.

## Findings

No blocker is established. I didn't find any high-severity defects.

**1. Medium, evidence accuracy: "all primitive kinds" isn't what the harness covers**
- **Location:** assignment evidence statement; `docs/pascal-units-experiment.md`; `tests/native/units-main.c`
- **Impact:** the claim is stronger than the proof, although tracing the code shows no type-tag or result-widening defect.
- **Evidence:** positive units execute these paths:
  - Integer value parameters and results
  - Boolean value parameters and results
  - VAR Integer, Byte, Char and Boolean

  They don't execute Byte or Char value parameters, Byte or Char results, or the documented untyped VAR parameter.
- **Fix:** add those cases, or narrow the claim in the evidence and docs.

**2. Medium, coverage risk: private implementation features have little positive coverage**
- **Location:** `tests/native/units-main.c`; `docs/pascal-units-experiment.md`
- **Trigger:** any unit using a private routine, a unit-level `var`, or statement emitters in unsupplied `compiler.c`.
- **Impact:** an unannotated global operand would keep its preferred address and could corrupt memory silently.
- **Evidence:**
  - The only private feature executed positively is one typed Integer constant.
  - No unit runs a private helper, transitive call, nested routine or `{$A-}` recursion.
  - None uses `for`, `case`, `with`, arrays, records or strings in unit code.
  - The doc states that private routines use the existing compiler and that recursion keeps its activation ABI.
- **Fix:** add one executed unit that covers these features, including an exported routine calling a private helper with an unused sibling. Otherwise, narrow the documented subset. This is the highest-value remaining check.

**3. Low: shadowing a restricted name falsely rejects a unit**
- **Location:** `cc_lookup_decl` in `src/compiler_front.c`; `cc_new_symbol`
- **Trigger:** a unit declares a parameter or local named `BufLen`, or shadows a name such as `ConOutPtr`.
- **Impact:** the unit fails with `UNIT_UNSUPPORTED`, even though it never uses the runtime symbol. This fails closed.
- **Evidence:** `cc_new_symbol` calls `cc_lookup_decl`, which sets `console_dynamic` when it finds the outer symbol. The same pattern applies to `S_ADDRESS` symbols.
- **Fix:** set the unsupported flag in the `cc_lookup` wrapper, based on the returned symbol, rather than during declaration lookup.

## Questions

1. **`IOResult`:** does `RT_IORESULT` clear runtime status? `intrinsic()` calls it without setting a reject flag. If it clears status, a unit can change the client's I/O state and the call should be rejected. `runtime.asm` wasn't supplied.
2. **Evidence order:** does the retained evidence hash-bind the corpus and final unit run to the same compiler objects? The reconciliation says so, but this review can't confirm it.

## Future work, not blockers

- **Trust-path tests:** add single-byte corruption, mid-file truncation, wrong unit name and profile mismatch cases. The current bad-artifact test is a 3-byte file.
- **Fault injection and limits:** cover allocation, interrupt and `page_drop` failures through load, link and close, plus `cc_units_hold` retry and the 32-module, 128-root and record-page limits.
- **Language paths:** test a qualified procedure statement, `Addr` of an import, a nonzero `Start`, and fault locations across several imported files.
- **IDE authoring:** `output_store_source` and the importer strip trailing spaces, but `unit_source_fingerprint` hashes the root buffer as supplied. If edited Work text keeps trailing spaces, its artifact can look stale after save. Authoring should compile the saved canonical text.
- **Performance:** `build_ready` rereads every unit source and allocates temporary pages. Measure this cost before IDE use, along with the per-byte module accessor cost.

## Review limitations

These sources weren't supplied:
- `compiler.c`, `compiler_strings.c`, `compiler_real.c`, `compiler_io.c`, `compiler_files.c`
- `compiler_console.c`, `compiler_crt.c`, `compiler_keyboard.c`, `compiler_chain.c`
- `platform.c`, `runtime.asm`, `tools/package-native.py`

I rely on the reported negative tests for those paths. I also couldn't inspect `reviews/pascal-units-v1.md` beyond the supplied reconciliation. This completes the second review turn, so any open items should be reported as unresolved unless you request another round.

### Codex reconciliation

1. Accepted evidence correction. Positive unit tests execute Integer/Boolean value parameters/results and VAR Integer/Byte/Char/Boolean. Byte/Char value/result and untyped VAR paths are not tested. The experiment document now states the exact combinations rather than implying all combinations. The original assignment's broader phrase is superseded by this correction; assertion counts and results are unchanged.
2. Accepted coverage limitation, deferred to the next unit verification step. No concrete incorrect operand was identified. The current experiment proves source authoring, imports, omission, VAR/value calls, typed constants, unit fault locations and standalone execution; it does not establish private/nested/transitive/recursive compound implementations. The contract now says so explicitly. Existing ordinary-program corpus coverage is reported separately.
3. Accepted known conservative rejection, deferred. Declarations that shadow restricted runtime names can set the feature flag through declaration lookup. This fails closed and does not invalidate the executed subset. Documented as a known issue; no source changes were made after the second review.
Questions: runtime.asm io_result lines374–382 reads and clears shared I/O status and compatibility bytes. This fixed public runtime service needs no per-program helper generation and remains intentionally available, with its ordinary shared-state semantics; the experimental dependency gate is not a purity policy. This distinction is now explicit in the contract. Evidence binding: both unit and macro-on corpus reports retain compiler source hashes and executable/library hashes. All compiler source hashes agree across those runs; only the later Kinds fixture extends tests/native/units-main.c, which is not compiled into zunitcorpus. Its unchanged code/library artifacts are independently retained and authenticated. Full evidence is in docs/pascal-units-evidence.json.
No further review round was run. Two read-only turns completed; the follow-up found no concrete blocker for the documented experiment. The coverage correction and limitations documentation were edited after the follow-up; code and test results are unchanged. Formal workflow finalization is not claimed for those updated document inputs. The outstanding coverage and false-rejection items remain explicit next work; this is not production readiness.
