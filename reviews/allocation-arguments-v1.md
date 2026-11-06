# Review: allocation-arguments-v1

## Opus turn 20261106T170600-4daa2fd6

Mode: code; read-only

## Review result: `allocation-arguments-v1`

**Do not accept this change yet.** I found no established defect in the production accounting, but the new test has one blocker and one coverage gap.

### Established defects

**1. Blocker: `ARGSMALL` cannot compile, so `SizeOf` suppression is untested**
- **Location:** `tools/test-unit-real.py`, `argument_fixtures()`, `ARGSMALL.PAS`; `src/compiler_expr.c`, `intrinsic()`, `F_SIZEOF` branch.
- **Trigger:** `SizeOf(Over(...))` passes a function call to `SizeOf`, which has no function-call path.
- **Evidence:** every branch rejects this fixture:
  - If `cc_variable(id)` is false, or the lookup finds nothing, the branch ends in `cc_fail(41)`.
  - If `cc_variable(id)` is true, `cc_selection()` stops at `(`, and `cc_expect(')',5)` fails.
- **Impact:**
  - `check(compile(...))` fails, and the inner shared-layout, run and save checks for this case are skipped.
  - The harness cannot pass or reach 905 assertions if that total assumes four valid builds.
  - The test never reaches `cc_invoke` under suppression, so that requirement remains unverified.
- **Fix:** put the call in a suppressed selector, keeping `shared=1`:

  ```pascal
  uses Wide;var A:array[1..1]of Integer;
  begin if SizeOf(A[Over(...)])<>2 then ...
  ```

  This path does reach `cc_invoke` with `cc.suppress` set. Also confirm that `cc_entry_reference()` handles suppressed imported calls correctly, because that code was not supplied.

**2. Medium: the argument fixtures may not detect corrupted arguments**
- **Location:** `tools/test-unit-real.py`, all `ARG*.PAS` cases; `tests/native/unit-real-main.c`, `argument_calls()`.
- **Trigger:** a wrong result takes `then halt`, while the harness accepts only `ram_result[26]==1` and `status==0`.
- **Evidence:** the older `client()` fixtures use `halt` too, but they prove the success path with a `WriteLn` marker. The new fixtures cannot use `WriteLn`, because it forces three pages.
- **Impact:** if `halt` exits normally, as those markers imply, a wrong `Bound`, `Over` or `Inner` result still passes. The runtime half of the threshold test would then show only that the program terminated. `halt` itself was not supplied, so confirm its status behavior.
- **Fix:** make failure produce a distinct runtime error using an operation allowed in shared images, such as dividing by a zero variable. The existing test already expects status 2 from `1.0/X` with `X=0.0`. Keep the expected status at 0, then confirm that a deliberately wrong expected value fails.

**3. Low: the 512-byte boundary excludes some live stack words**
- **Location:** `src/compiler_front.c`, `cc_reserve`; `src/compiler_expr.c`, `cc_index` and binary-operand pushes; `src/compiler.h`, `arguments`.
- **Trigger:** `cc_index` saves the array base, and binary operators save the left operand, without updating `cc.temporaries` or `cc.arguments`.
- **Example:** `Bound(<85 Reals>,A[Inner1(1)])` with a one-parameter function has 514 bytes live, but only 512 are counted. It stays shared, even though the true caller depth exceeds 512.
- **Impact:** this is not an established overflow, because the 2048-byte reserve leaves headroom. It does mean the 512-byte caller bound is approximate, and `ARGPEAK` does not measure the true peak.
- **Fix:** either count these saved words, or document the bound as covering arguments and temporaries only. Then show that the compiler's nesting limit keeps uncounted words within the headroom.

**4. Low: the documentation does not describe the new rule**
- **Location:** `docs/program-pages.md`, "Requirement-based allocation" and "Helper eligibility".
- **Issue:** the diff leaves this file unchanged, so it doesn't say that live caller arguments count toward eligibility. Leaf-unit calls are still described as "2 when statics fit".
- **Fix:** document the rule, and add the new 905-assertion evidence only after the harness passes. Keep the 777 figure as historical evidence or label it that way.

### Requested checks

| Area | Finding |
|---|---|
| Nested arguments | **Correct within the counted categories.** Inner calls add to the outer total and restore it afterward. `ARGPEAK` reaches 514 at the inner call guard. |
| Suppressed `SizeOf` | **Production logic looks correct.** `cc_require_full_memory()` returns early under suppression, and `cc_invoke` restores the count. Finding 1 leaves this untested. |
| Cleanup | **Correct in `cc_invoke`.** It has no early return, so its `break` path still restores `cc.arguments`. Real-result `cc_reserve(6)` runs after that restore, and `cc_block` now rejects leaked counts. |
| Threshold | **Correct and well targeted.** The final push is checked by the guard before the call. `ARG512` peaks at exactly 512, and its later `Inner` call would catch a failed restore. |
| Threshold cases | `ARG516` crosses the limit earlier, at the 86th argument's hold (510 + 6). `ARGPEAK` provides the 514-byte case. |

### Questions and missing context

1. **`read_block` setup:** `argument_calls()` calls it after `source_close()` and never calls `source_new()`, unlike the other tests. Please provide its definition. Confirm that it works on a closed `Source` and sets the path used by `compile_unit`, `compile` and `save_build`. If not, use `source_new()` first.
2. **ZPU limits:** do import and export support 85 or 86 parameters, and a `var` parameter? `compiler_units` was not supplied.
3. **Initialization and leaf status:** the supplied files don't show where `cc` is initialized, or the code behind `cc_entry_reference()`. Confirm that `cc.arguments` starts at zero, and that unit-internal calls still force the larger layout so the leaf-callee bound holds.
4. **Assertion count:** I can't verify `EXPECTED_ASSERTIONS=905`, because `CODE_BANK_LIMIT` and the total at `HEAD` were not supplied. Recheck it after fixing `ARGSMALL`.

### Limitations

This was a static review of the supplied files. The native ZDE build and unit-real harness are still pending, so bank fit and all runtime behavior remain unverified. Statement-level stack use, including `for`, `with` and `case`, was also outside the supplied code.

### Codex reconciliation

Accepted findings 1 and 2. ARGSMALL now suppresses Over inside SizeOf(A[Over(...)]) and failure branches divide by a zero-initialized Integer, producing a distinct runtime error. The native harness passes exactly 905/905 assertions, zero failures, with RAM and saved runs. Capture: analysis/native-slice/20261106T171014.945703Z-95206/unit-real-report.json. ZDE target build passed, all banks fit; production container 240094 bytes.

Finding 3 adapted: document the threshold precisely as caller-owned arguments plus generated temporaries, not exact whole-program SP analysis. Saved expression operands and statement frames remain outside this selector's estimate. Runtime guards use actual SP and protect stack/heap bounds; the existing conservative helper exclusions and 2048-byte reserve remain. Broader static stack accounting is deferred, and no exact caller stack bound is claimed.

Finding 4 accepted: docs/program-pages.md now documents argument categories, nested restoration, suppression and limits. Original 777-assertion measurement is historical evidence.

Context answers: read_block initializes a closed Source and canonical path (src/output.c). compile core zeroes cc at entry. cc_entry_reference returns directly for imported routines but requires full memory for all local/native calls, including unit-internal calls (src/compiler_bridges.c). ZPU primitive signatures store 16-bit parameter counts and support Real and VAR (src/compiler_units_object.c and compiler_units_names.c); the threshold unit author/import succeeds in target execution. All 905 assertions are reached, so no conditional execution tests were skipped.

## Opus turn 20261106T171340-1f243cc0

Mode: code; read-only

## Follow-up review: `allocation-arguments-v1`

**No blockers remain, and I found no new correctness defect in the allocation change.** Three low-severity follow-ups remain, plus a wider regression run if this is meant to close the milestone.

The supplied report shows 905/905 assertions and zero failures. `compile()` is itself an assertion, so zero failures also means all four `ARG*` programs built. I did not run anything, and I accepted the ZDE build result and bank-fit statement as reported.

### Prior findings

| Finding | Status | Assessment |
|---|---|---|
| 1. `ARGSMALL` could not compile | **Resolved** | `SizeOf(A[Over(...)])` reaches `cc_invoke` under suppression. Its 516 counted bytes cannot force full memory, and `cc_units_call()` emits nothing. The shared-layout assertion would catch a suppression failure. |
| 2. Wrong results could pass | **Mostly resolved** | Failure branches no longer use `halt`, and the fixtures distinguish argument order and the `VAR` value. Finding 2 below covers the missing negative control. |
| 3. Uncounted stack words | **Acceptable as adapted** | Documenting the limit is reasonable for a conservative selector. Runtime guards can make this a reported failure, but they don't provide the extra stack a three-page image would have. A selected program that exceeds shared capacity may now fail with an overflow. Reaching that case appears to need roughly 1000 uncounted bytes, so I would not block on it. |
| 4. Missing documentation | **Partly resolved** | The new rule is documented well. Finding 3 below covers the stale text. |

**Prior questions:** the new context closes these:
- `read_block` initializes a closed `Source`.
- `compile_project` clears `cc` with `memset`.
- Unit-internal calls set `ZPU_FULL_MEMORY` through `cc_entry_reference`.
- Export records store 16-bit parameter counts and support `VAR` and Real parameters.

### Remaining findings

**1. Medium, verification only: wider regression evidence was not supplied**
- **Location:** `src/compiler.h`, `src/compiler_front.c`, `src/compiler_routines.c`.
- **Issue:** this change touches shared compiler paths, but the supplied evidence covers only the build targets and unit-real.
- **Risk:** code generation should stay the same, and the eligibility change only moves programs toward the larger layout. A `Compiler` layout change or the new `cc_block` check could still affect other paths.
- **Next step:** this isn't a code defect, but rerun `program-pages`, the host sanitizer check and the Real matrix before calling the milestone ready.

**2. Low: the new failure path has no negative control**
- **Location:** `tools/test-unit-real.py`, `argument_fixtures()`.
- **Issue:** the suite proves a runtime fault for Real division (`1.0/X`), but the fixtures depend on Integer `1 div AFailure`. `RT_DIV` was not supplied, and the reconciliation doesn't report a deliberately failing run.
- **Impact:** if Integer division by zero doesn't fault, wrong results could still pass.
- **Next step:** temporarily change one expected `42`, then confirm that both Memory Run and saved execution fail.

**3. Low: `docs/program-pages.md` still has stale or absolute claims**
- **Stale figures:** it still says the harness "passes 777 assertions" and gives a 239941-byte container. The reconciliation reports 905 assertions and 240094 bytes.
- **Over-strong bound:** "bounds both sides of supported leaf-unit calls" conflicts with the new statement that saved operands and statement frames are not fully counted. Limit it to counted arguments, temporaries and callee reservations.
- **Unverified guard claim:** "report overflow before writes" is absolute, but `RT_GUARD` and the runtime helpers were not supplied. For example, `cc_call(RT_PUSHSTRING)` guards zero bytes before a push of up to 256 bytes, unlike the bridge's `bridge_guard(257)`. Either confirm this against the runtime or narrow the wording to the guarded paths tested.

**4. Low: the line-length assertion runs before the fixtures are rewritten**
- **Location:** `tools/test-unit-real.py`, `argument_fixtures()`.
- **Issue:** the assertion checks the text before `AFailure` is inserted. By my count, the first `ARGSMALL` line grows from 109 to 126 characters, so it passes today with one character to spare.
- **Fix:** apply the substitutions first, then assert on the final fixture text.

### Limitations

Runtime assembly, `cc_stack_check` and the statement compiler were not supplied. That leaves the guard behavior, the maximum uncounted stack depth and statement-frame use unconfirmed. `libraries/screen-engine.inc` and `tools/screen-library.c` were outside this review, as assigned.

### Codex reconciliation

No new code blockers. Host sanitizers passed (tools/test-native-host.py, 179718 frontend checks plus supporting suites). The 13-case program-pages run and complete 17-case Real comparison both passed with stable inputs; results are recorded in docs/allocation-arguments-evidence.json. Verification-only finding 1 accepted. Finding 2: Integer division by zero is established by runtime implementation RT_DIV and existing native corpus; a deliberate mutation control is deferred and not claimed. Finding 3 adapted: docs/allocation-arguments.md explicitly labels old figures historical and records the precise non-exact selector limits; guards are not claimed to prove unlimited static stack eligibility. Finding 4 deferred: final generated lines pass the current 127-character limit, but the pre-rewrite assertion should move when these fixtures next change. No additional review round requested or performed.
