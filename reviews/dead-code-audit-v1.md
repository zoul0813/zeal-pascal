# Review: dead-code-audit-v1

## Opus turn 20261109T004719-f57cbcd9

Mode: code; read-only

# Dead-code audit review at db23b18

**Overall:** I found no false positive among the 54 listed functions, and nothing supplied disproves those classifications. The main problems are the groups disposition, a warning probe aimed at the likely wrong expression, an overstated data-root method, and scope wording. I ran nothing and rely only on the supplied files.

I recomputed the JSON arithmetic and it matches the assignment: 54 functions and 12851 bytes, split 10264 / 1817 / 567 / 163 / 40.

## Established findings

**1. High: the largest cleanup candidate is a dormant feature, not established junk**
- **Location:** `src/compiler/groups/{groups,late,members}.c`, `src/compiler/routines.c` `cc_block`
- **Trigger and impact:** Treating the 10264 bytes (80% of the total) as deletable could remove the only compiler-side implementation of overlay groups. That would be a product-scope decision made through a dead-code audit.
- **Evidence:**
  - `groups.c` calls this a "dormant group format".
  - `routines.c` says "Direct Memory has no output owner for overlay group files", and rejects `overlay` with error 77 or 12.
  - `AGENTS.md` makes recovered Turbo Pascal behavior the product basis.
  - `runtime/process/overlay.c` is also classified as nonproduction.
- **Fix:** Report this as "unreachable in production, dormant overlay feature". Check `docs/specification.md` before excluding or removing it.

**2. Medium: the W158 probe likely targets the wrong expression**
- **Location:** `tests/native/fragments.c:540,555`
- **Trigger and impact:** "Emitted code delivers 0/1" says nothing about the likely overflow, so it cannot show the warning is benign.
- **Evidence:**
  - By manual count, both lines are the final `check(page_drop(source.page)&&!used);` in `byte_references` and `runtime_references`.
  - The same statement in `metadata_only` does not warn.
  - These are the only two functions with `CF_*<<CF_REFERENCE_SHIFT` static initializers.
  - The fixture implies `CF_LOW8<<shift` is `0x4000`, `CF_HIGH8<<shift` is `0x8000`, and the runtime encoding is `0xC000`. The last two overflow 16-bit `int`, which matches exactly one warning per function.
  - `fragments.h` was not supplied, and SDCC attributing the warning to the function's last statement is an inference.
- **Verification:**
  - Inspect the emitted tables for `0,1,0x4001,255, 1,1,0x8001,255, 2,1,1,255, 4,1,0x4001,255` and `1,0,0xF7C0,0`.
  - Run a minimal probe with a local `static const uint16_t t[]={2<<14|1};` followed by unrelated statements.
  - Test the fix `((uint16_t)CF_x<<CF_REFERENCE_SHIFT)`.

**3. Medium: `scan.py` does not implement the claimed data-address roots**
- **Location:** `analysis/dead-code-audit/scan.py`, the `current` span and `before=asm.split('; Function ',1)[0]`
- **Trigger and impact:**
  - Only text before the first function is scanned as data.
  - `current` is never reset, so tables and `_INITIALIZER` data after a function are attributed to the preceding function.
  - A function-pointer table after a dead function can make its target falsely dead. After a live function, it creates ordinary edges rather than roots.
- **Evidence:** This follows from the script logic. Where SDCC places such data needs confirming in the `.asm` output.
- **Effect on the current list:** `groups.c`, `members.c` and `image.c` show no such tables. `decode.c` was not supplied.
- **Fix:** End function spans at the function's code boundary, and treat all references outside code spans as roots. Then diff the 54-function result.

**4. Medium: function-level conclusions cover only the IDE link**
- **Location:** `scan.py`, `links` from `CMakeFiles/zpascal.dir/link.txt`
- **Trigger and impact:** The four-product closure proves target reachability only. It does not analyze:
  - C functions in `pascal_runtime`, `integer_runtime`, `transfer_runtime`, `key_runtime` or the SYSFILE build
  - assembly routines
  - unused data or BSS
- **False-positive risk if extended:** The `zf_op_*` handlers are deliberately unreferenced and retained through fragment linking, so a naive scan would misreport them.
- **Fix:** State "no finding" only for the 101 IDE objects, and list the other areas as not audited.

**5. Low: the object graph misses dead paths behind compile-time configuration**
- **Location:** `src/compiler/compile.c`, `compile_project`
- **Example:** With `ZP_UNITS_EXPERIMENT`, the loop skips `i==0` and `i==1`, so `if(!i&&!chain){...cc_runtime_copy...}` cannot run. SDCC cannot diagnose this because `i` is a variable.
- **Wider pattern:** The no-unit configuration behind the test W126 warnings appears to be non-product. The non-`ZP_DIRECT_FILE` paths in `operations.c` need the same classification.
- **Fix:** Add a separate inventory of preprocessor-configured paths.

## Questions needing evidence

**6. Medium: is the IDE decoder dead, or has a validation call been orphaned?**
- `Compiler.cmake` says saved-image checks "also run outside compilation", and `link/image.c` describes a live banked entry. The graph finds no caller of `native_header`.
- Please identify the current saved-image validation path. If it is intentionally elsewhere, correct the comments and keep `decode.c` in `transfer_runtime`. Otherwise this may be a missing check rather than dead code.

**7. Medium: do tools or included assembly create roots outside the graph?**
- `package-native.py`, `package-optional-runtime.py`, `package-runtime-layout.py` and `runtime_objects.py` consume maps or linked outputs, and none were supplied.
- Please grep them for C symbol lookups, and check for `.include`/`.incbin` use of the four `nonproduction_src` files. DependInfo does not establish that those files are unused.

**8. Low: does `cc_global_put` removal strand anything, and why is it unused?**
- Confirm that high-global initialization did not lose its caller by regression.
- `separate_globals` must stay because `cc_place_code` uses it. Check whether `cc.placing` becomes dead.

**9. Low: what does "No warnings suppressed" cover?**
- Please grep project and toolchain flags for `disable_warning`.
- Confirm that assembler and linker warnings were captured.
- Add the build target to each `warnings.json` entry, since `real.c:256` appears twice without attribution.

## Warning classifications checked against supplied source

| Warning | Source construct | Assessment |
|---|---|---|
| `operations.c:83-84` W126 | Else branch of `!MEMORY_MODE` with `MEMORY_MODE` 0 | Benign for `ZP_DIRECT_FILE`. A preprocessor split should leave SYSFILE unchanged. |
| `object.c:145` W110 | First `if(!cleanup_error)` after initialization to 0 | Benign. Replace it with a direct assignment. |
| `io.c:38,116,137` W110 | `if(...)cc_require_real();` | Consistent with a no-unit expansion, but the macro was not supplied. |
| `io.c:50` W110/W126 | `unsigned_field` stays 0 without units | Benign in the test configuration. |
| `pages.c:28-29` W110/W126 | `files` stays 1 without units | Benign in the test configuration. |

I could not assess `real.c:256,356`, `sources.c:58,74`, `files.c:387` or `compact-main.c:31` because those sources were not supplied. The `real.c` warnings should stay open until reviewed, since they are in release builds.

## Recommended staged cleanup

1. **Evidence:** Fix `scan.py`, resolve items 2, 6 and 7, and regenerate the reports.
2. **Warning-only edits:** Change `object.c:145`, the `open_path` guard and the test shift casts. Require unchanged product hashes wherever the emitted code should not change.
3. **Small resident candidates:** Handle `_TEXT` items first (`compile`, `build_locate`, `source_import`, `cc_disk_global_put`), after confirming each has no caller.
4. **Test-only APIs:** Keep bodies available to tests. `cc_fragment_link` is used directly by `tests/native/fragments.c`, so a define on a shared object library could break test targets. `Tests.cmake` is needed to choose the mechanism.
5. **IDE decoder:** Remove `image_compiler` from `zpascal` only after item 6 is resolved.
6. **Groups, only with product approval:**
   - Remove the `cc_groups_scope` call together with the `groups` branch in `new_scope`.
   - Keep `cc_root_operand` and `cc_root_address`, including relocation for nonzero values and `cc_fail(99)` for zero values.
   - Preserve no-op or zero behavior for `cc_root_request`, `cc_root_pending`, `cc_root_bind` and `cc_root_member`.
   - Measure per-bank results, because the savings split across `_BANK14` (7259) and `_BANK16` (3005) and may reclaim no page.

The SDK `kernel_config` copies should stay reported only, with no SDK patch.

## Limitations

- I did not have the emitted `.asm`, `.rel`, `.lst` or map files, or the build logs.
- I also lacked `Tests.cmake`, `Units.cmake`, `Sources.cmake`, the packaging tools, `docs/specification.md`, and most non-listed sources.
- The claims that only test fixtures set `groups`, that the listed APIs are test-only, and that artifacts match the checkpoint remain unverified.
- Numeric entry points remain unmodeled.
- Assembly `Ref` roots keep C targets live even when the referencing assembly is itself dead.
- Byte spans are upper-bound estimates, not guaranteed savings.

### Codex reconciliation

Read-only audit reconciliation; no product edits authorized or performed.

1. Accepted and superseded by explicit user steering: overlays are deferred, code must remain, and the feature will be completed later. Keep all overlay/group/late/member code intact, including its current build membership. The 10,264 bytes remain an explanatory dormant-feature measurement, excluded from cleanup recommendations. The non-overlay candidate spans are 2,587 bytes, or 2,547 excluding SDK fallback stubs.
2. Accepted. Minimal signed static initializer reproduces W158; unsigned counterpart does not. Full fixture with unsigned CF_HIGH8/CF_RUNTIME16 shift operands removes both W158 diagnostics; object T and R records and all area bytes/sizes are identical. Both tag tables contain their intended 0x8001/0xF7C0 words. Production units/object.c:182 uses the same signed-left-shift pattern, so fixing unsigned operands is a genuine width-correctness recommendation, not merely suppression. No tag malfunction was observed in current binaries. The explicit boolean rewrite did not remove warnings and is not recommended.
3. Accepted/adapted. The collector now treats directive address references anywhere in the ASM as independent roots, and function area is captured at its actual label. All actual linked C ASM has zero named address-bearing data directives; the candidate set remains exactly 54 / 12,851. Byte extents remain explicitly labeled conservative spans; constant pools may be included. This is a one-off current-link audit, not a complete disassembler or reusable reachability proof.
4. Accepted: function-level analysis is explicitly the 101-object IDE link / 936 C functions. Other generated runtimes, stock-ZPU exports, assembly code and data/BSS are not exhaustively audited. API exports/numeric runtime entry points must not be treated as dead for lacking C callers.
5. Accepted: report includes configured constant paths, including the skipped i==0 prefix arm in compile.c. cc_runtime_copy itself stays: it is called by the live cc_image_page path in build.c:45. Other legacy branches remain for their explicit tests. No dormant overlay code will be changed.
6. Resolved from current source/policy: IDE run_saved in storage/files.c validates build ownership then calls execute_native, following OS opaque-binary execution. transfer/chain runtime still compiles native_decode and calls it in chain_prepare and execute_prepare; the latter recognizes valid Pascal metadata for early retirement but accepts other native executables under the documented OS contract. No lost IDE validation call was found; only the uncalled IDE copy is recommended for removal, and the decoder must remain in the loader/tests.
7. Initial word-boundary search missed leading-underscore spellings; this assertion was incorrect. Final follow-up found package-native.py reads _native_header/_native_decode. Corrected searches and dispositions follow below. Actual linked assembly Ref roots are included. The four nonproduction files are explicitly legacy/test targets; no production .include/.incbin references them. Numeric entry points remain a stated manual-check limitation.
8. Resolved: typed initializer replay in types.c emits payload via cc_emit/cc_put; no caller of cc_global_put or its disk shim exists. cc.placing and separate_globals remain live during cc_place_code and cc_put and must stay. This recommendation removes wrappers, not initializer handling.
9. Clarified: no suppression introduced by this audit, and none found in searched project/SDK CMake, SDK headers and generated production flags. Full saved release and selected-test logs contain C warnings only, no assembler/linker warnings. Warnings now record targets/configurations. Isolated Real builds reproduce W110 at 256/356 for real_runtime, 256 for real_compiler, and none for compact_integer. Bounds in decimal scale/format estimate were reviewed; no arithmetic defect found. Legacy W110 comes from raw=0, unsigned_field=0, files=1 or no-op cc_require_real; naked-ASM address parameter accounts for W85.

The 156 src bodies and all six shipping artifacts still match db23b18 checkpoint evidence. The production root closure has 37 used components; no orphan target was found. Prior regressions are not represented as newly executed. Follow-up reviews report.json, collector changes and isolated probe evidence; no implementation approval is sought.

## Opus turn 20261109T010341-c38cc21f

Mode: code; read-only

# Follow-up review of dead-code audit at db23b18

**Overall:** One material error remains: the report says no packaging tool looks up a candidate symbol, but `package-native.py` looks up two. Removing the IDE decoder as recommended would fail the shipping package step. The other prior findings are resolved, apart from the evidence gap in finding 2 and the test-gating risk in finding 3. I ran nothing; probe and log results below are as reported in the supplied artifacts.

## Established findings

**1. High: removing the IDE decoder breaks `zpascal_bin`, and the report's tool-check claim is false**
- **Location:** `tools/package-native.py`, `package()`; `analysis/dead-code-audit/report.json` limitations and `recommended_order`
- **Trigger and impact:** `package()` starts with `symbols['_native_header'] >> 16 != symbols['_native_decode'] >> 16`. If `image_compiler` leaves the `zpascal` link, both symbols disappear from the map and packaging raises `KeyError`. The failure is visible and produces no output, but the recommended step cannot work as written.
- **Evidence:** `zpascal_bin` runs `package-native.py --optional-real`, which calls `package()`. The report nevertheless says "no candidate symbol lookups found in the four core packaging tools", and reconciliation item 7 says the same.
- **Fix:**
  - Correct the report and list this as a build-time consumer outside the reachability graph.
  - Make the decoder step include a packager change: enforce the same-bank check only when the symbols are present, and reject a target that has only one of them.
  - Rerun a literal search for all 54 names, with and without the leading underscore, across all of `tools/` and `tests/`. `package-zvb.py` and the `test-*.py` tools were not supplied and may also consume symbols.
  - Update the comments in `Compiler.cmake` and `link/image.c`, which still describe this entry as active in the IDE.
  - Note in the inventory that `image_compiler` becomes test-only.

**2. Low: the `units/object.c:182` statement goes beyond the supplied evidence**
- **Location:** `report.json`, first `warning_findings` entry
- **Trigger and impact:** The entry combines `object.c:182` with the fixture lines, then says emitted tags are correct and the probe gave identical bytes. Both emitted tables and the T/R comparison cover only the fragment fixture and minimal probe.
- **Evidence:** Line 182 does contain `(CF_RUNTIME16<<CF_REFERENCE_SHIFT)|(cursor.value-0x4000u)`, so the source concern is valid. It is a runtime expression with no logged W158, and `emitted-evidence.txt` has no output for it.
- **Fix:** Report line 182 separately as the same signed-shift pattern, with no diagnostic. Inspect its emitted code, and require identical `object.c` T/R records after the unsigned change.

**3. Low: a definition on a test executable will not gate shared object libraries**
- **Location:** `cmake/Compiler.cmake`, `cmake/Tests.cmake`, test-only API stage
- **Trigger and impact:** `ZP_TEST_COMPILER` is applied to executables that list `banks.c`. It does not reach `unit_runtime_core` or `unit_fragment_core`, which `zpascal` shares with tests such as `zoptionalcorpus` and `zexampletest`. Gating only one side can leave a wrapper in `banks.c` calling a removed `compiler_build_locate`.
- **Evidence:** `zfragmenttest` compiles `src/units/linker/fragments.c` directly, so it can have its own definition. The shared-library consumers need a separate test object or a separate library variant.
- **Fix:** Decide ownership per API after searching `tests/`, then link every affected test target.

## Disposition of prior findings

| Prior item | Status | Basis |
|---|---|---|
| 1. Overlay code | **Resolved** | The report keeps it intact and excludes it from cleanup. The remaining 2,587 and 2,547 byte totals are correct. `docs/native-artifacts.md` still says "pending necessity review", so the report should keep citing the user's direction as the authority. |
| 2. W158 | **Resolved for the fixture** | Emitted tables contain `0x8001` and `0xF7C0`. Unsigned operands remove both warnings. Lines 512 and 544 are the correct source sites. See finding 2 for `object.c`. |
| 3. Data roots | **Resolved** | `scan.py` now records directive references throughout each file. The zero-root result leaves the 54-function set unchanged. |
| 4. Scope | **Resolved** | The report limits function-level claims to the IDE link and lists what was not audited. |
| 5. Configured paths | **Resolved** | `compile.c:96` is correctly described, and `cc_runtime_copy` stays live through `cc_image_page`. |
| 6. Decoder validation | **Resolved, cleanup blocked by finding 1** | `run_saved` uses `build_ready` before native execution. `chain_prepare` and `execute_prepare` keep their own decoder. |
| 7. External roots | **Not resolved** | The nonproduction sources match their `Tests.cmake` owners, but the packaging claim is wrong. See finding 1. |
| 8. Global writes | **Resolved** | Typed initializers emit through `cc_emit` and `cc_put`. `cc.placing` remains live in `emit.c` and `build.c`. Comments that mention a global initializer's write should be updated with the removal. |
| 9. Warning coverage | **Resolved within stated limits** | Warnings now carry targets and configurations, and the report states what was searched. |

## Warning review with newly supplied source

- **`real.c:256`:** This is `powers[magnitude/4]`. Callers limit the magnitude to 38 at most, so the index stays within the 10-entry table.
- **`real.c:356`:** This is the signed `estimate` division. For `E` from 1 to 255 it yields -39 to 38, and the next line clamps the low end to -38.
- **Variant results:** `real_compiler` excludes line 356 and `compact_integer` excludes both sites, which matches the probe results. I found no arithmetic defect, though I did not establish SDCC's internal reason for emitting W110.
- **`sources.c:58,74`:** Both conditions depend on `raw`, which stays 0 without units. The classification is correct.
- **`internal.h:245`:** The no-op `cc_require_real` macro confirms the classification of the `io.c` warnings.
- **`compact-main.c:31` and `files.c:387`:** `compact-main.c` was not supplied, so W85 remains unverified. I did not independently verify the `files.c` line number.

## Recommended staged cleanup

1. **Correct the report:** Apply finding 1 and repeat the literal symbol search.
2. **Fix diagnostics:** Change the tag shifts, `object.c:145` and the naked-parameter warning. For `open_path`, split on `ZP_DIRECT_FILE` without changing `ZP_FILE_HOST`, whose `MEMORY_MODE` definition takes precedence. Require unchanged emitted bytes where no code change is intended.
3. **Remove unused global writes:** Rebuild and check `_TEXT` and `_BANK7`.
4. **Gate test-only APIs:** Follow finding 3, then link all test targets.
5. **Remove the IDE decoder:** Change the packager in the same step, keeping `native_decode` in `transfer_runtime` and tests.
6. **Measure results:** Report per-bank changes and refresh stock Unit provenance if it changes.

Overlay code and SDK sources stay out of scope.

## Limitations

- I did not have the build logs, the production `.asm`, `.rel` or `.lst` files, or the probe outputs beyond the supplied excerpts.
- `linked-candidates.json` records hashes for only two products, so the six-artifact match remains a reported result.
- Tests other than the supplied fragment fixture, and some packaging tools, were not available.
- Numeric entry points, assembly routines, data and BSS, and non-IDE runtime functions remain unaudited at function level.
- Byte spans are upper-bound estimates, and no page savings are established.

### Codex reconciliation

Final findings accepted; factual report corrections locally checked after the two-turn limit. No product source or artifact was changed.

1. Corrected. A new literal/identifier search covers all 54 candidate names in plain C, leading-underscore and bank-alias forms across tools/ and tests/. It confirms package-native.py:430 consumes _native_header/_native_decode. It also finds check-unit-banks.py:134 requires native_header AND compile_limits, and test-runtime-profile.py:111 mutates _native_decode to test the packager bank guard. These are now explicit build/test consumers in report.json. The decoder is runtime-unused in the IDE, not freely removable under the current build contract. Its future removal must update the packager guard, bank-policy checks and mutation fixture in the same stage; both-present entries still require the same bank, and half-present pairs must be rejected. Other literal matches such as "failed", "position", "word" and "frozen" are unrelated field names/oracle helpers, not map roots. external-references.json and symbol-literal-consumers.json retain the search evidence. No decoder exclusion was performed.
2. Corrected the report to separate fixture and production claims. Fixture object-byte/relocation identity covers only the fixture. Inspected production Unit writer assembly at the exact source line: low byte is retained, high byte has add #0xc0 followed by or #0xc0, implementing the intended 16-bit subtract-0x4000 / OR-0xc000 pattern today. No W158 was logged at this production site. It still deserves an unsigned operand for standard 16-bit C correctness; comparison of the complete rewritten production object is explicitly required during implementation and has not been claimed here.
3. Accepted: per-API test-support objects or object-library variants are required for shared-core convenience APIs. Executable compile definitions do not change existing object-library bodies. The plan now requires every affected test link to pass and explicitly coordinates both build_locate wrapper layers and compile_limits bank policy. This is implementation planning only.

All overlay code remains intact under the user's explicit instruction. No SDK patch is proposed. All candidate bytes remain code-span estimates, not guaranteed savings or freed RAM pages. Function-level runtime/stock-unit/assembly/data coverage remains a stated audit limit. No additional Opus turn was requested; this is a locally reconciled audit, not a final approval of a removal patch.
