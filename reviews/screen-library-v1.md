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
