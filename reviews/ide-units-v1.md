# Review: ide-units-v1

## Opus turn 20261105T212251-8ab7c55e

Mode: code; read-only

# Review of `ide-units-v1`: normal-IDE units and native ABI2 checkpoint

**Verdict:** I found one high-severity defect and no blockers. Banked-call ownership, ABI2 flag writing and validation, transient bank 17 handling, and the link-before-Real ordering held up within the supplied code.

I did not run anything. Test results below come only from the supplied reports and assignment text. The prompt has no line numbers for most files, so locations are given by symbol.

## Established defects

### 1. High: Trailing spaces in Work make the new unit stale on first import
**Location:** `cc_units_write_object` in `src/compiler_units_object.c` (`at_word(page,16,cc.sources->root->length)`), `cc_units_load_object`, and `compile_work` in `src/main.c`

**Trigger:** Work has trailing spaces on any line, for example from typing `end. ` in the editor. The user presses C in Unit mode, then compiles a client.

**Impact:** The unit reports `Wrote CALC.ZPU`, but every client gets error 251, "Unit source/profile changed; recompile unit". Recompiling cannot fix it until the buffer is reloaded or the spaces are removed.

**Evidence:**
- `output_store_source` trims trailing spaces while iterating over `s->length`, so the saved file can be shorter than Work.
- Saving does not change `work.length` or the buffer, and `compile_work` then uses `root=work`.
- The writer stores the raw `root->length`.
- The loader imports the trimmed disk file and rejects it when `loaded.length!=cc_unit_module_word(module,16)`.
- `tools/test-ide-units.py` only types `{edited}\n`, so this path is not tested.

`unit_source_fingerprint` is in `src/sources.c`, which was not supplied, so it may have a matching problem.

**Fix:** Store the length and fingerprint of the canonical saved stream, using one helper that shares the trim rule with `output_store_source`. Alternatively, compile the saved disk text and map diagnostics back to Work. Keep error offsets correct for Work in either case.

**Verification:** Add an IDE regression with trailing spaces on both a middle line and the last line. Build a client against that unit, then force a diagnostic in the unit to check the error position.

### 2. Low: Unit mode loses existing error mapping for unreadable sources
**Location:** `unit_request` in `src/compiler_job.c`

**Trigger and impact:**
- If `read_block` fails with no diagnostic code, such as a missing Main file, program mode reports compiler error 90 with the path. Unit mode reports only `Compile failed, native error N`.
- If `capture_source` fails, unit mode can print `native error 0`. The `platform:` path in `compile_request` would have set `ERR_FAILURE`.

**Evidence:** `compile_request` sets `diagnostic.code=90` after this failure and defaults `platform_error`. `unit_request` does neither.

**Fix:** Apply the same two rules in `unit_request`, then add a negative test with a missing Main file.

### 3. Medium (test coverage): The production compiler configuration has no ordinary corpus target
**Location:** `CMakeLists.txt` and `cached_code_pages` in `tests/native/harness-execute.c`

**Impact:** `zpascal` now builds with both `ZP_OPTIONAL_REAL` and `ZP_UNITS_EXPERIMENT`, but the corpus targets cover only one switch each:

| Target | Configuration |
|---|---|
| `zoptionalcorpus` | Optional Real only |
| `zunitcorpus` | Units only |
| `zunitrealtest` | Both switches, but a focused test rather than the corpus |

The double decrement in `cached_code_pages` is therefore never compiled. The ordinary corpus also never exercises the combined deferred-prefix, Real-upgrade, and `unit_cbreak` paths.

The assignment promises a "combined core corpus", but the supplied CMake file has no such target. The 17-case matrix helps, but it is not a replacement for the corpus.

**Fix:** Add a corpus target linked to `unit_runtime_core`, with an execution object that defines both macros. Include CHN cases, because production now always runs `cc_put(RT_CBREAK-0x4000u,...)` for chain images, on an image with no copied runtime prefix.

## Questions that need more context

### 4. Can a unit declare `external` and lose its Real requirement?
**Needed:** `src/compiler_routines.c`

The ordinary test matrix treats `external` as requiring the full runtime, and `cc_units_owner` always clears `unit_real`, even if no bytes were emitted. A requirement raised at declaration can therefore disappear. If it survived, it would still belong to the declaring fragment rather than the reachable caller.

The unsupported-feature tests cover `inline` and `absolute`, but not `external`. If units accept it, a client could link the integer engine while calling opaque native code that expects full Real support. Rejecting `external` in unit mode, with a test, is the smallest fix.

### 5. Does a successful unit request need `cc_tables_end`?
**Needed:** `src/compiler_tables.c`

`unit_request` never calls `build_close(&scratch)` on success, so `cc_tables_end` does not run. The worker ledger reclaims pages, and repeated compiles pass. However, any static table handles could stay stale, and the IDE tests do not assert total `pages_used()` after a unit compile. Confirm that `cc_tables_begin` is safe here, or end the tables explicitly before ledger cleanup.

### 6. Will the pending old unit regression handle the unloaded bank 17?
**Needed:** `tests/native/code-capture.c`

`zunittest` no longer restores `CODE_UNIT_IO`, but `tools/test-units.py` still expects the captured code to match the entire library payload. If capture reads `code_pages[16]` directly, it will see page 255. Treat any failure of `compiler bytes preserved` as a possible consequence of this change.

### 7. Does anything set `cc.real_needed` directly?
**Needed:** The remaining compiler sources

Unit fragment flags are only recorded through `cc_require_real()`. Any direct assignment to `cc.real_needed` would bypass ABI2 tracking.

### 8. Are the new tests part of the normal test path?
**Needed:** `tools/test-plan.json` and `tools/run-tests.py`

The diff does not change `tools/test-plan.json`. Also, `zunitrealtest_bin` is not an `ALL` target, and `tools/test-unit-real.py` does not build it, so the test could run against a stale binary. Confirm that the new tests are registered and that the runner rebuilds this target first.

## Optional enhancements

- **Scratch collisions:** Unit mode shows only `native error 15`. Printing the `.$PU` path would match the existing scratch-conflict guidance, and also helps with orphaned scratch files after a failed cleanup followed by Quit.
- **Signature coverage:** Add tests for `var X:Real`, mixed Integer/Real parameters, and Real parameters in non-first positions. I could not check the caller logic because `compiler_routines.c` was not supplied.

## Review limitations

- **Sources not supplied:** `compiler_runtime.c`, `compiler_tables.c`, `compiler_routines.c`, `sources.c`, `source.c`, `platform.c`, `compiler_disk.c`, and `code-capture.c`.
- **Limited conclusions:** Because of those gaps, I could not fully verify deferred-prefix Real patching, fingerprint behavior, or how unsupported features are rejected.
- **Pending evidence:** The full optional-vs-control matrix, media tests, old unit regression, and combined corpus remain unverified. The ZealFS and EEPROM runs are especially important for `.$PU` naming and capacity.

### Codex reconciliation

1. Accepted: unit mode refreshes Work transactionally through existing load_source after source save. Compilation, artifact fingerprint/length, imported source maps and the editor now use the same saved text. A regression types trailing spaces on an intermediate and final line. Ordinary program mode is unchanged.
2. Accepted: unit_request now reports90 for an unreadable source without a syntax diagnostic and defaults empty platform failures to ERR_FAILURE. A focused missing-Main request checks90, pathname, no build and empty worker ledger.
3. Accepted: the requested CHN comparison exposed late CBreak placement: helper selection saw the empty chain prefix. Moved initial CBreak placement after unit linking/Real selection and before all helper finalization. This preserves the import memory saving and restores the original selection order. zoptionalcorpus now links the production unit_runtime_core and unit_fragment_core, and its execution observer defines both macros. The15-family corpus and the existing Real CHN/parent comparison are required before finalization.
4. Accepted defensively: all External declarations in unit mode are rejected, including private implementation declarations. Added a regression. The previous writer also rejected console_dynamic, but rejecting at declaration preserves a direct diagnostic and avoids depending on that later guard.
5. Accepted: unit_request explicitly build_close()s its scratch Build before ledger cleanup, ending compiler table handles while their owner is still in scope. cc_tables_begin also validates/ends existing handles; successful unit compilation does not export a Build.
6. Checked: code-capture.c already streams unloaded banks from code_phase_fd at validated directory offsets; it does not read far page255. The existing unit regression remains a required check. Its first rerun exposed the old Text-function result diagnostic and a multi-import failure, not code capture corruption. The negative now uses a VAR Text parameter to reach the intended unsupported-unit check. Source binding was moved after I/O-bank release to remove an unnecessary physical page from the verification peak.
7. Checked repository-wide: cc.real_needed is assigned only inside cc_require_real; callers use that helper. Follow-up includes the previously missing compiler/runtime/source files.
8. Accepted: the new unit, IDE and combined-corpus checks are registered in tools/test-plan.json. Focused unit/Real and combined corpus drivers rebuild their native targets through ZDE before observation; ordinary IDE tests use the existing normal IDE build prerequisite.
Optional scratch-path UI guidance and broader mixed/VAR Real signatures are deferred beyond this increment; current primitive ABI tests include raw Real arguments/results. No additional product requirements are inferred.

Final verification: unit regression329, focused unit/Real353, combined15-family corpus1982, all without failures. Final IDE tests pass dirty save, intermediate/final trailing spaces, exact diagnostic editor byte, syntax/collision/authenticated Ctrl-U cancellation/retry, TF/EEPROM authoring/import and standalone Integer/Real execution. Real CHN and parent match the full-runtime control byte for byte and execute after compiler/source/library deletion. Final five-case Integer/Screen/Math/External/Transfer matrix passes RAM/saved/ownership/control comparisons with stable inputs; the provisional17-case matrix also passed before these fixes. Final startup layout, library validation, allocator failure cleanup,14-page budget, lifecycle and interactive checks all pass with ten code pages. Host output366841 assertions/2528 scenarios, asset validation, bank audit and23 runner tests pass.

Implementation checkpoint3363bc3 follows native checkpointd538f61. docs/ide-units-evidence.json binds final source, executable, library and report hashes; reopened and verified after creation. All tested final source/build inputs are frozen for the follow-up. No unresolved implementation finding remains known. Full test-plan execution, physical hardware and complete TP4–TP7 semantics remain outside this increment.

## Opus turn 20261105T215535-8b04701e

Mode: code; read-only

# Follow-up review of `ide-units-v1`: checkpoint `3363bc3`

**Verdict: fix finding 1 before finalizing.** It can silently destroy the only copy of a source file, and the fix is a one-word change. The initial findings are otherwise resolved, and I found no blockers. Findings 2 and 3 are test-process gaps, and finding 4 is a minor behavior change.

I did not run anything. Verification status below comes from the supplied reports and evidence index.

## Disposition of initial findings

| # | Initial item | Status | Basis |
|---|---|---|---|
| 1 | Trailing spaces make the unit stale | **Fixed, but introduced finding 1 below** | Compile, length, fingerprint and later import now use the same saved text. The test covers intermediate and final lines. |
| 2 | Missing error mapping | Resolved | `unit_request` now maps to error 90 and defaults empty platform errors. `missing_main` covers the error 90 path, but nothing exercises the `ERR_FAILURE` default. |
| 3 | No combined corpus | Resolved | `zoptionalcorpus` links the production core with both macros, and all 15 families pass with 1982 assertions. |
| 4 | `external` in units | Resolved | All unit-mode `external` declarations are rejected, including private ones, with a regression. |
| 5 | `cc_tables_end` skipped on success | Resolved | `build_close(&scratch)` runs before ledger cleanup and is safe to repeat after a failed compile. `cc_tables_end` does not touch stale `cc.sources` or `cc.error`. |
| 6 | Capture of unloaded bank 17 | Resolved | `code_capture` reads unloaded banks from `code_phase_fd`. |
| 7 | Direct `cc.real_needed` writes | Accepted on the reported check | The supplied sources are consistent with the claim, but I did not have every compiler file. |
| 8 | Test registration | Partly resolved | The tests are registered and rebuild their targets, which creates finding 2. |

## Established findings

### 1. High: Unit compile can erase a clean Work buffer when its file is missing
**Location:** `compile_work` in `src/main.c`, at `load_source(&work,work.path,&d)`

**Trigger:**
1. Work is clean and non-empty.
2. Its file is deleted or renamed, or its media is changed. A Pascal `Erase`, an `X` Execute child, or a media swap could do this.
3. The user presses C in Unit mode.

**Impact:** RAM holds the only remaining copy, and that copy is silently replaced with an empty buffer marked clean. Compilation then fails against the empty source, and the old text cannot be recovered.

**Evidence:**
- `load_source` calls `read_source(...,required=0)`.
- For a missing file, that path runs `import_begin` and `import_finish`. This commits an empty source and retires the old page.
- `zp.h` documents `reload_source` as the required replacement where "missing/error leaves the current Work intact".
- The auto-save only protects dirty Work, and no final test deletes the file while Work is clean.

**Fix:** Call `reload_source(&work,work.path,&d)` instead. The existing error branch already reports the failure and retains the source.

**Verification:**
1. Add an IDE regression that opens the unit and leaves it clean.
2. Delete the file from a host checkpoint and press C.
3. Assert that `work.length`, the text, and `work.page` contents are unchanged, a native error is shown, and no scratch or `.ZPU` file appears.

### 2. Medium (test process): New drivers rebuild inside a run that freezes `build/`
**Location:** `tools/test-plan.json`, `tools/test-units.py`, `tools/test-unit-real.py`, `tools/test-optional-corpus.py`, and `inputs()` in `tools/run-tests.py`

**Trigger and impact:**
- `zunittest_bin`, `zunitrealtest_bin` and `zoptionalcorpus_bin` are not `ALL` targets, so a normal build does not produce them.
- `run-tests.py` hashes `build/*.bin`, `.zpl`, `.map` and similar files before the run and again after it.
- On a tree without those outputs, the files appear mid-run. The run then ends as `inputs-changed` and exits 1, even if every test passes.
- The three drivers are different commands, so `dependencies()` does not order them. Under the default `--jobs 4`, they can run `zde cmake` in one build tree at the same time.

This does not affect the product, and no full plan run is claimed. It does affect the first full run that uses this registration.

**Fix:** Build these targets once before `frozen=inputs()`, or add them to the default build, then remove the rebuild from the drivers. Failing that, serialize the drivers and exclude their outputs from the freeze check.

**Verification:** Run `run-tests.py --only 'units' --only 'unit-real' --only 'optional-corpus'` from a tree without those outputs. Expect `passed-focused`.

### 3. Medium (verification): Final-binary coverage has two gaps around the CBreak move
**Location:** `tools/test-plan.json`, `docs/ide-units-evidence.json`, and the reordered finalization in `compile_project`

**Evidence:**
- The CHN comparison in `tools/test-optional-chain.py` found a real ordering bug, but that test is not in the plan. The 17-case `tools/test-optional-real.py` is not in the plan either.
- CBreak now moves before helper finalization. The final binary reran only five of the 17 cases against the control. The cases not rerun include `READINT`, `READREAL`, `TEXTINT`, `TEXTREAL`, `HOOK`, `RANDOM` and `TEXTWRITE`, which cover other helper paths.

The corpus and the CHN comparison reduce this risk, and the assignment states the limit accurately. It is still a real gap around the exact change that caused the CHN discrepancy.

**Fix:** Add the CHN test to the plan. Since finding 1 changes the binary anyway, rerun the full 17-case matrix on the rebuilt output and bind that report in the evidence index.

### 4. Low: Every successful unit compile resets the editor position
**Location:** `compile_work` in `src/main.c`

**Trigger and impact:** The refresh runs even when Work is clean or has no trailing spaces. `import_finish` installs a new source with `cursor=0`, and `editor_forget()` clears the editor state, so the next edit starts at the top of the file. The refresh also briefly needs a second set of source pages, so low memory can block a unit compile that would otherwise fit.

**Fix (optional):** Save `work.cursor` before the refresh, then restore it clamped to the new length. Skipping the refresh when Work has no trailing spaces would also avoid the extra pages, but it needs a scan. Otherwise, document the cursor reset in `docs/ide-units.md`.

## Question

**Can Work contain a Ctrl-Z byte?** `import_byte` stops at byte 26, while `output_store_source` writes it. If the editor can insert that byte, the refresh drops all text after it from Work and marks Work clean. The next Save would then truncate the file. `line_byte` is not in the supplied sources. If it allows byte 26, this needs a guard or a test.

## Verified in the changed paths

- **Source binding:** `bind_source` runs in warm bank 9 after bank 17 is released. It validates path length and canonical form, and uses the unchanged stale checks.
- **Evidence index:** The source and binary hashes I could cross-check match the supplied reports. The corpus family totals sum to 1982.
- **Ordinary builds:** `zptest` hashes are unchanged across both checkpoints, which fits these changes being limited to unit builds.

## Review limitations

- **Sources still not supplied:** `compiler_emit.c`, `source_internal.h`, `source_validation.c` and `tools/test-native.py`.
- **Report binding:** The focused IDE, failure, unit/Real and CHN reports carry no input hashes of their own. Only the evidence index links them to the final build.
- **Rebuild impact:** Fixing finding 1 changes `zpascal`, so the IDE, media, failure, matrix and startup evidence needs rerunning and new hashes.
- **Scope of claims:** This remains emulator evidence only. It does not establish hardware behavior, a full test-plan pass, or full TP4–TP7 equivalence.

### Codex reconciliation

1. Accepted and fixed in485c9b2: required reload_source replaces load_source. The native regression deletes a clean nonempty Work file before C, confirms no unit/scratch is created, and authenticates far_get's mapper/bytes to compare the exact RAM source and complete Work descriptor before/after failure. It passes. The same final IDE also passes dirty source/BAK, trailing spaces, exact error cursor, collision, writer-entry Ctrl-U, retry, TF/EEPROM and standalone tests. Cursor is preserved across successful refresh, clamped to the saved length.
2. Accepted: the orchestrator computes selected native prerequisites, builds them once under its checkout lock before freezing inputs, and only then starts parallel read-only drivers. Removed all three in-driver builds. Build processes now use the existing owned process-group cleanup and signal handling, so cancellation reaps the build before releasing the lock. All24 host runner tests pass, including absent-output creation/freeze and actual SIGTERM child-reaping checks. The final process-ownership version passed the real fresh-output run: sanitized host prerequisite, unit329, unit/Real353 and combined corpus1982, with status passed-focused and zero changed inputs. Its build began with all fifteen fixture outputs absent. The tested runner checkpoint is afd27a7.
3. Accepted: full optional-Real matrix and Real CHN/parent comparison are registered in the74-command/148-expanded-check plan. All17 matrix cases pass on the final rebuilt IDE against the full-runtime control, including the previously omitted Read/Text/hook/random/output paths. The final CHN and parent remain byte-identical and run after source/compiler/library deletion. Final startup and IDE/media/failure evidence was rerun; ZPL238798 bytes, resident15335, ten cold/fifteen warm code pages.
4. Accepted: preserve/clamp Work's cursor on successful refresh instead of resetting it to zero. Failed required reload leaves the whole descriptor and exact source text intact.
Ctrl-Z question resolved without a new guard: source_internal.h line_byte rejects26 (and CR), and editor/source replacement validation uses that shared invariant. Disk imports retain the pre-existing TP-style EOF convention; the editor cannot introduce the questioned byte.

Review limitation: both permitted Opus turns were used. The follow-up fixes and runner lifecycle correction are verified by Codex, but were not independently re-reviewed; the review workflow is intentionally not finalized and no unchanged-input completion is claimed. Another review turn requires an explicit user request under the project skill. No remaining implementation finding is known. Full product-plan execution, physical hardware and full TP4–TP7 compatibility remain unclaimed.


Final record: docs/ide-units-evidence.json was regenerated after checkpoints
485c9b2 and afd27a7 and reopened to verify every indexed source/build/report hash.
Final IDE capture binaries match the indexed production build. All17 matrix
cases, final IDE/media/failure/clean-Work retention, CHN and startup checks pass.
The independent review workflow remains incomplete because final corrections
postdate the last allowed review; this record does not assert finalization.
