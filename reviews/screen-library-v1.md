# Review: screen-library-v1

## Opus turn 20261106T173225-e392db6f

Mode: code; read-only

# Review: Screen extraction (`screen-library-v1`)

**Verdict: not ready.** There is one blocker and one high-severity regression. I ran nothing and have no Screen build or test evidence, so these conclusions come from reading the code.

## Established defects

### 1. Blocker: TF packaging always crashes
- **Location:** `tools/package-tf.py`, `metadata = json.loads(snapshots['build/zpascal.banks.json', 'build/SCREEN.ZPU', 'build/SCREEN.PAS'])`
- **Trigger and impact:** Every packaging run indexes `snapshots` with a 3-tuple and raises `KeyError`. The script stops before it installs anything, so the requirement to ship the ZPU plus the interface reference isn't met.
- **Evidence:** `snapshots` is keyed by individual path strings. This looks like the edit to `names` being applied to this line as well.
- **Fix:** Use `json.loads(snapshots['build/zpascal.banks.json'])`, then run the packager and confirm `/lib/SCREEN.ZPU` and `/lib/SCREEN.PAS` pass the copy verification.

### 2. High: Chain programs lose all Screen operations
- **Location:** `src/compiler_units_names.c`, `cc_units_imports`; `src/compiler.c` and `src/compiler_expr.c`, the new `#ifndef ZP_UNITS_EXPERIMENT` blocks
- **Trigger and impact:** In production, a Chain compile that uses `ClrScr`, `GotoXY`, `Delay`, `WhereX` or similar has no working path. Without `uses Screen`, the intrinsics are gone (error 41). With it, the import is rejected with error 249. These programs compiled before this change.
- **Evidence:** `cc_units_imports` fails with `UNIT_UNSUPPORTED` when `options->kind==COMPILE_CHAIN`. The chain tests build on `zp_core`, which keeps the legacy generator, so they can't catch this. `docs/screen-library.md` doesn't mention the limit.
- **Fix:** Add fragment-link support for Chain, with a production-compiler test that runs a chained CRT program. If the limit is intended, get that decision explicitly, then document and test it.

### 3. Low: A fallback path failure can hide a valid local unit
- **Location:** `src/compiler_units_names.c`, `cc_units_load`
- **Trigger and impact:** If `sources_path(filename,code_library_directory,fallback)` fails, for example because the combined path is too long, the compile fails with `UNIT_MISSING` before the local file is tried. This breaks the local-first lookup order.
- **Fix:** On failure, set `fallback[0]=0` and continue. Report `UNIT_MISSING` only if the local open also finds nothing.

### 4. Low: `test-screen.py` can test stale or missing binaries
- **Location:** `CMakeLists.txt`, `zscreentest_bin`; `README.md`, "Focused library validation"
- **Trigger and impact:** `zscreentest_bin` isn't an `ALL` target, yet the README says to run the test after `zde cmake`. The script then fails on a clean tree. If earlier outputs exist, it can silently run an old `.bin` and `.zpl` against new sources.
- **Fix:** Add `ALL`, or document the explicit target. Also make the script reject outputs older than the linked `zscreentest` executable.

### 5. Low: The "artifacts unchanged" check omits key Screen inputs
- **Location:** `tools/test-screen.py`, `inputs`
- **Trigger and impact:** The check doesn't hash `build/SCREEN.ZPU`, `libraries/screen-engine.inc`, `tools/screen-library.c` or `tools/package-screen.py`. A changed artifact can therefore still pass, and the evidence report won't identify it.
- **Fix:** Add those four files, plus `build/SCREEN.PAS`, to `inputs`.

### 6. Medium: Required edge cases have no test coverage
- **Location:** `tests/native/screen-main.c`, `tools/test-screen.py`
- **Evidence:** The visible tests don't exercise these paths, although `crt.pas` wasn't supplied and may cover some of them.

| Gap | Test to add |
|---|---|
| Query unwind under `{$I-}` | A pending error makes `WhereX`/`WhereY` return 0 with the stack balanced |
| Caller stack and IX | A failing `GotoXY` or query inside a procedure, with its locals and later arguments checked afterward |
| Stale local unit | A local ZPU with a valid checksum and wrong profile returns 251, with a valid fallback present |
| Non-missing local failure | An unreadable local entry produces an error and doesn't fall back |
| General lookup | A user-authored unit resolves through the library directory |
| Library override | Relative and `/`-rooted command-line library paths both resolve correctly |

## Questions to verify

1. **Could `source_replace` read unmapped test text? (Medium)** `load()` now passes test text to it, and `screen-main.c` keeps literals in BANK1. The CMake comment says "caller text stays resident", and the editor test moved its fixtures to `RODATA`, which suggests banked validation can't read them. If `source_validation.c` dereferences `text`, `load()` will fail. In that case, copy the text to retained memory or build the combined source before importing it.
2. **Does any non-macro code depend on `Compiler` field offsets? (Medium)** `#ifndef ZP_UNITS_EXPERIMENT` now moves the fields after `crt_row` by 34 bytes. `image_compiler` and `test_evidence` link into macro-enabled targets without that definition. Check that `src/compiler_image.c` and `tests/native/code-capture.c` don't use `cc`. A safer alternative is to move these fields to the end of the struct.
3. **Is host `cc` available under `zde cmake`?** `package-screen.py` calls it directly, so confirm it exists in that environment. Its `-Werror` flag is also risky: `for(...)cc_emit(...);cc_emit(...);` on one line may trigger GCC's `-Wmisleading-indentation`, which would fail the build.
4. **Do older corpora still pass?** Check `zoptionalcorpus` and `zunitcorpus` for CRT intrinsics, because these targets no longer provide them and have no Screen prefix.
5. **Can `uses Screen;` precede `program`?** `crt.pas`, `colors.pas` and the `SCREEN` case in `test-optional-real.py` all get it prepended. If any starts with a `program` header, the parser must accept that order.
6. **Is `screen-main.c` tracked?** The `.gitignore` additions list only the new `tools`, `docs`, `reviews` and `libraries` paths. Run `git check-ignore tests/native/screen-main.c`.
7. **Does any test expect export flag 1 to be rejected?** That value is now valid, so an older negative test could fail.
8. **Can `Inline` bypass the Addr rejection?** `compiler_inline.c` wasn't supplied. Confirm that it can't reference a flagged import, which would skip the rejection and the `A` setup.
9. **Are unusual library paths handled?** `T:zpascal.zpl` leaves an empty library directory, which silently disables fallback. Relative paths containing `../` are stored unnormalized, so confirm that `sources_path` handles them.

## Contracts reviewed with no defect found

- **Wrapper ABI:** Argument offsets, callee cleanup, IX restore, `HL` query results and `A` preservation through the guard are correct.
- **Guard and source PC:** The 96-byte guard reports the caller's PC, and that PC survives `Delay` and pending-error paths.
- **Range checks:** Full Integer checks run regardless of `{$R}`, and `HL`/`DE` reach `CRT_XY` in the expected registers.
- **I/O unwind:** The `IOCALL` flag patch, `{$I-}` unwind and query result slot match `io_call` and `io_check`.
- **ZPU container:** Header and metadata offsets, export records, ZPF1 records and the FNV checksum match the loader. Export flag validation and the Addr rejection path are also consistent.
- **Relocations and fragments:** Every relocation target stays inside the payload. The `HighVideo` to `NormVideo` reference and shared fragment 0 are expressed as dependencies.
- **Local errors:** Non-missing open failures and stale or corrupt units don't fall back.

## Review limitations

These files weren't supplied, so anything that depends on them is unverified:
- `compiler_fragments.c/.h`, including fragment and reference capacity limits
- `compiler_units.c`, `compiler_inline.c`, `compiler_image.c` and the rest of `compiler_crt.c`
- `output.c`, `sources.c` and `source_validation.c`
- `crt.pas`, `colors.pas`, `tools/test-native.py` and `tools/package-native.py`

### Codex reconciliation

Finding 1 accepted and fixed: tuple indexing corrected. Actual tools/package-tf.py run passed, installed T:/lib/SCREEN.ZPU and SCREEN.PAS with byte verification, preserved existing files/recovery image, launched default IDE and left the installed image unchanged. Evidence: /private/tmp/screen-tf.log and build/tf.json.

Finding 2 accepted: removed the Chain-only unit-import rejection; the existing final fragment linker and image placement already serve Chain output. Added production-compiler saved parent/Screen child test, matching Start=$9000 and End=$FBFF, reversed first-use CrtExit/CrtInit ordering, cursor restoration and a distinct success marker. A first run used a stale test object after a fixture edit during compilation; corrected by editing only between builds, rebuilding, and checking two compile_project calls in generated assembly. Fresh 502-assertion Screen run is in progress. No new Chain semantics are inferred without its execution result.

Finding 3 accepted: an invalid fallback path now disables fallback but still tries the source-local unit. Existing loader opens fallback only for ERR_NO_SUCH_ENTRY. Corrupt local units are tested with a valid-checksum unknown export flag; missing local units fall back to the loaded library directory. A separate authored Tiny unit proves the lookup is generic.

Finding 4 accepted: zscreentest_bin is now ALL; script rejects bin/zpl older than the linked IHX. Finding 5 accepted: hashes now cover SCREEN.ZPU/PAS, native author, emitter and packager as well as existing compiler, test and tool inputs.

Finding 6 partially accepted: added I- pending-error WhereX/WhereY inside a local procedure with checked local values and a success marker, RAM and saved execution. Existing crt.pas tests argument order, mapper/IFF/cursor restoration and other native state. Dedicated stale-profile/permissions and relative/rooted library override variants are deferred; loader validation of stale media is unchanged and fallback is explicitly restricted to missing-file errors. Absolute command-line override and default installed path are tested. No claim of exhaustive path/media permutation coverage.

Questions: source_replace is resident and restores mappings after banked helpers; the first native run passed 500 assertions including all source loads and complete framebuffer/query/source-location tests. src/compiler_image.c and tests/native/code-capture.c do not access cc, so conditional Compiler offsets do not cross those objects; Build is unchanged. Host cc is installed in ZDE and successfully built the 3572-byte Screen artifact; GCC's misleading-indentation warning was fixed with braces. Inline rejects all imported routine references (src/compiler_inline.c), including flagged exports. CRT/Colors fixtures have no program header, so prepending USES is legal; Lighthouse places USES after its program header. screen-main.c is not ignored and will be staged explicitly. No existing test rejects export bit0 specifically. sources_path canonicalizes ../ via path_resolve; drive-relative T:file is not an initial supported canonical unit search form.

Verified: production ZDE build and all bank bounds; zpascal.zpl 236209 bytes, Screen.ZPU 3572 bytes, startup code-page count unchanged. Host sanitizer suite passed. Updated 905-assertion unit/Real harness passed with Screen imported. Focused production Screen/MATH IDE matrix and full-runtime controls passed with stable inputs. The earlier source-retention assertion compared to its pre-USES fixture; fixed to compare the actual supplied bytes. The fresh Screen oracle result will be reconciled before readiness is declared.


### Follow-up execution failure and independent reconciliation

The required follow-up invocation failed with Claude API 429,
`usage_limit_reached` (five-hour account limit; reported reset 13:40 America/New_York).
Its local run is `.review-workflow/runs/20261106T175513-4445ab73`.
No follow-up review text was produced; no review completion or readiness is claimed.
The runner and restricted permissions were preserved. No automatic retry or extra
review round was attempted. A future recovery must use the same review ID and
explicit new-session recovery per the runner guide.

Further investigation identified the apparent live Chain failure as an observer
bug: `transfer_native.asm` deliberately commits at FC00, the same logical address
as the test supervisor stop. The prior test accepted that breakpoint without
checking the resident image. The corrected observer qualifies the supervisor's
first 32 bytes before accepting completion and steps past the child trampoline.
A separate standalone observer already ran the saved parent and child successfully,
printed SCREEN-CHAIN-OK, and passed its page-inventory/foreign-page checks. The
final combined oracle is being rerun with both live and standalone execution.
Only test code changed after the failed follow-up; product sources are unchanged.
Remaining untested path/media permutations are listed above.


Final independent verification: the image-qualified observer passes exactly
502/502 assertions with zero failures, including live banked Chain execution;
the separate standalone parent/child run also passes after compiler/library
files are removed. No live Chain product defect remains. TF install/default
launch, focused Screen/MATH IDE matrix, 905-assertion unit/Real regression and
host sanitizers pass. Evidence: docs/screen-library-evidence.json. The build graph
includes zscreentest_bin in ALL and its standalone supervisor dependency.
Only the follow-up review remains blocked; this is a tested checkpoint, not an
attestation of completed review. No raw Claude account/session output is tracked.

## Opus turn 20261106T185117-ee8f73ce

Mode: code; read-only

# Final review: `screen-library-v1`

## Verdicts

| Checkpoint | Verdict |
|---|---|
| **Allocation** (`f7d96b8`) | **APPROVE WITH NONBLOCKING NOTES** |
| **Crt extraction and rename** (`f11eb41`, `f2092d8`, docs clarification) | **APPROVE WITH NONBLOCKING NOTES** |

**The evidence establishes no blockers, and no extra code review round is needed.** This is a static review: I ran nothing and relied on the supplied reports for test results. Questions 1 and 2 should still be closed before calling examples or legacy-style programs release-ready, because the supplied evidence doesn't cover them.

## Prior findings

### Allocation
| Finding | Status |
|---|---|
| `ARGSMALL` could not compile | Resolved. The 905 assertions passed with no failures. |
| Wrong results could pass | Resolved for fixture design. The deliberate mutation control remains deferred (allocation note below). |
| Uncounted operands and frames | Accepted as a documented limitation. |
| Missing or stale documentation | Resolved by the labelling and limits in `docs/allocation-arguments.md`. I did not see the current `docs/program-pages.md`. |
| Wider regression evidence | Resolved by the reported sanitizer, 13-case and 17-case runs. |

I re-read the final `cc_invoke`, `cc_reserve` and `cc_block` changes and found no new defect:
- Arguments are counted and restored on every path, including suppressed calls.
- The 512/514/516 boundaries match the fixtures.
- The eligibility change only moves programs toward three pages.

**Allocation note:** the mutation control for Integer `div` failure remains deferred, and the line-length assertion in `argument_fixtures()` still runs before the substitutions. Neither blocks approval. Fix both the next time those fixtures change.

### Crt extraction
| Finding | Status |
|---|---|
| TF tuple crash | Resolved. The current code indexes correctly and the TF report passes. |
| Chain import rejection | Resolved. The rejection is gone, and the 502-assertion oracle plus the standalone run pass. |
| Fallback failure hiding a local unit | Resolved. See finding 2 for the reverse case. |
| `ALL` target and stale binaries | Resolved. |
| Unhashed inputs | Resolved. |
| Missing edge tests | Partly resolved. The remaining gaps are the accepted stale-profile, permissions and path-override limits. |

I traced these paths and found no defect:
- Wrapper ABI, guard handling and source PC
- `{$I-}` unwind and query result storage
- ZPU header, export and fragment layout, plus checksum
- Relocation targets, the Addr rejection and export flag validation
- Macro-conditional `Compiler` layout across linked objects
- Reported size arithmetic (240094 − 236209 = 3885)

The artifact hashes in `reviews/final-library-verification.json` match `build/tf.json`.

## Established findings (nonblocking)

**1. Medium: the final TF report does not show that a user file was preserved**
- **Location:** `build/tf.json`, `tools/package-tf.py`.
- **Trigger and impact:** the base card held no preservable file content, so the preservation check passed without comparing any file. "Preservation" therefore means the check ran, not that a user file was shown to survive. `retired` paths are also deleted regardless of content, so a user's own `T:/lib/SCREEN.ZPU` would be removed.
- **Evidence:** the report has `preserved_existing_files: 0`. Both retired files were present in that run.
- **Mitigation:** a recovery image is written whenever a previous card exists, and only such a card can contain those files.
- **Fix:** run once with a seeded user file such as `T:/lib/USER.TXT`, and require the count to be at least 1.

**2. Low: a long source directory blocks the installed library fallback**
- **Location:** `src/compiler_units_names.c`, `cc_units_imports`.
- **Trigger and impact:** `sources_path(filename,directory,path)` fails if the local candidate path is too long. The compile then stops with `UNIT_MISSING` before `cc_units_load` can try the fallback, even when a valid installed `CRT.ZPU` exists. This mirrors prior finding 3 in the opposite direction.
- **Fix:** treat an unrepresentable local path as missing and try the fallback, or document the limit.

**3. Low: the verification record will be stale after this review**
- **Location:** `reviews/final-library-verification.json`, `limitations`.
- **Issue:** it still says "Review follow-up unavailable until account quota resets."
- **Fix:** update it when recording this result. Keep the historical 429 note in the docs.

## Questions to verify

**1. Do other shipped sources depend on the removed intrinsics? (Medium)**
- Only `lighthouse.pas` gained `uses Crt`. `tools/test-examples.py` then comments that line out for the legacy target, so that test cannot detect a production break.
- The supplied evidence shows no production compile of Lighthouse. Any other example using `ClrScr`, `GotoXY`, `Delay` or `WhereX` would now fail with error 41 in production.
- **Verify:** search `examples/` for all 15 Crt names, then compile Lighthouse with `zpascal`.

**2. Do `zoptionalcorpus` and `zunitcorpus` still pass? (Medium)**
- The initial review's question 4 was never answered in the reconciliation, and the final evidence does not list either target.
- Both now lack the CRT intrinsics and have no `uses Crt` prefix.
- **Verify:** run both, or record why they are out of scope.

**3. Does a Chain child start a fresh session when the parent has an active CrtInit? (Low to medium)**
- `docs/screen-library.md` claims it does, but the test parent `SPARENT` never calls `CrtInit`.
- `runtime.asm` clears `crt_active` at `start`, and the activation and transfer code was not supplied. If a child bypasses that reset, its first `CrtExit` would restore the child's all-zero snapshot.
- **Verify:** add a parent that calls `CrtInit` and then chains to a child that calls `CrtExit` first.

**4. Is the IDE comparison current for `uses Crt`? (Low)**
- The supplied optional-real report (17:46) predates the rename evidence (18:15). `zpascal.bin` and `zpascal.zpl` have identical hashes in both, so the compiler is unchanged.
- That IDE matrix did not visibly run against the final `CRT.ZPU`, though the native oracle covers this path. Rerun it if you want end-to-end IDE evidence on the final artifact.

## Accepted limitations
- Compiler startup still retains ten code pages.
- The 512-byte threshold estimates arguments and temporaries, not every operand or frame.
- This is not a full TP4–7 CRT implementation.
- There is no exhaustive permissions, stale-profile or path override matrix.
- Internal names such as `screen-engine.inc`, `test-screen.py` and `ZP_SCREEN_LIBRARY` remain. These are cosmetic and do not ship a Screen alias.

## Review limitations
These files were not supplied, so anything depending on them is unverified:
- `compiler_activation.c`, `transfer.c`, `compiler_runtime.c`
- `tools/test-native.py`, `tools/package-native.py`
- The `examples/` tree
- `docs/program-pages.md` and the evidence JSON files under `docs/`

### Codex reconciliation

Accepted both APPROVE WITH NONBLOCKING NOTES verdicts. No product source changed after this review; no extra round is needed. The final integrated approval covers allocation f7d96b8 and extraction/rename f11eb41/f2092d8, plus the reviewed documentation clarification. Earlier quota-blocked status paragraphs are historical and superseded by this successful follow-up.

Established finding 1 accepted as a verification limitation: the final TF report has preserved_existing_files=0, so it demonstrates stock installation/removal, image recovery and launch, but not survival of a seeded user file. Such a preservation experiment is deferred; no stronger preservation claim is made for this checkpoint. The rename deliberately retires the two old stock paths regardless of content; backups mitigate this, and custom files using those exact retired stock names would also be removed.

Established finding 2 accepted/documented here: an unrepresentable source-local candidate can stop lookup before installed fallback. This near-127-character path edge is a nonblocking known limit; changing lookup behavior is deferred to a later milestone. No claim of exhaustive path/permissions coverage is made.

Established finding 3 adapted for audit integrity: reviews/final-library-verification.json is the immutable as-supplied verification snapshot and retains its then-current quota note. Current status is recorded in this decision record and docs/crt-final-approval.json, which explicitly supersedes that old status. Keeping review inputs unchanged allows the runner to attest precisely the reviewed snapshot.

Question 1 closed: a case-insensitive search of every examples/**/*.pas and *.inc found CRT operation names only in examples/lighthouse/lighthouse.pas. A fresh production zpascal IDE session compiled that exact source, with uses Crt, successfully in both Memory and saved-program modes; an authenticated valid Memory Build and generated native Lighthouse.COM were checked. Report: /private/tmp/crt-final-lighthouse.log and its retained capture.

Question 2 disposition: tests/native/main.c has zero references to all fifteen removed CRT names and no references to CRT.PAS or COLORS.PAS. Both corpus scripts run this shared main in the same fifteen noninteractive groups; the dedicated CRT tests are separate. Thus those corpora do not need an implicit Crt prefix. Neither complete corpus was rerun in this final approval pass; no fresh complete-corpus qualification is claimed. Current targeted proof remains 502 CRT assertions, 905 unit/Real assertions, prior sanitizer/measurement checks and the production IDE checks.

Question 3 answered statically: src/compiler_activation.c:cc_activation_begin explicitly emits XOR A / LD (RP_CRT_ACTIVE),A for Chain activation, independent of prefix start. This establishes fresh-session behavior by implementation. The special parent-CrtInit-before-Chain runtime permutation was not added; this remaining coverage gap is nonblocking.

Question 4 closed: reran tools/test-optional-real.py --case SCREEN --case MATH with the final CRT.ZPU. Both production and legacy controls pass, Real output identity and omitted integer engine checks pass, and inputs remain stable. Report: /private/tmp/crt-final-ide.log.

Allocation notes retained: deliberate Integer div negative mutation control and final-text line-length assertion placement are deferred, as approved. Compiler startup remains ten pages; the argument selector is not exact whole-program stack analysis; full TP4–7 CRT conformance is not claimed. Verification reports and current approval summary are in docs/crt-final-approval.json. No blockers remain for these checkpoints.
