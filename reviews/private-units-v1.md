# Review: private-units-v1

## Opus turn 20261105T104306-9441efa5

Mode: code; read-only

# Review of private-units-v1

I found no defects in the shadowing fix itself, but there are three test-coverage gaps and one question. No tests or builds ran for this review, and I have no results from the pending rebuild.

## Established findings

### 1. Medium: the forward helper is never called before its body
**Location:** `tests/native/private-unit.pas`, `Forwarded` and `Exercise`

- **Trigger and impact:** A defect that only affects calls emitted between a forward heading and its body would still pass.
- **Evidence:** The only call to `Forwarded` is in `Exercise`, which comes after `function Forwarded; begin ... end;`. `Transform`, `Factorial` and `UnusedSibling` sit between the heading and body, but none of them calls it. The fixture therefore covers the forward heading and split stub/body layout, but not a forward reference.
- **Fix:** Call `Forwarded` from `Transform` or another routine placed before its body. For example, use `case Forwarded(N) of` in `Transform`. Keep the expected results at 46 and 50.

### 2. Medium: shadow coverage only reaches parameter declarations
**Location:** `cc_new_symbol`, `routine()`, `tests/native/private-unit.pas`

- **Trigger and impact:** If `routine()` went back to calling `cc_lookup`, a private routine named after a restricted symbol would be wrongly rejected, and this suite would still pass.
- **Evidence:** The only shadows are the parameters `BufLen` and `ConOutPtr`, which go through `parameters()` to `cc_new_symbol`. No fixture routine has a restricted name, so the `old=cc_lookup_decl()` line in `routine()` never resolves to one. There is also no global variable, local variable or S_ADDRESS shadow.
- **Fix:** Add a private routine such as `function HeapPtr(V:Integer):Integer`, called from `Exercise`. Add a unit-global `var CBreak:Boolean` or similar, plus a parameter that shadows a unit-level `absolute` variable.

### 3. Medium: real-use rejection through the new wrapper is only partly tested
**Location:** `cc_lookup` wrapper, `tests/native/units-main.c`, `unsupported(...)` cases

- **Trigger and impact:** Deleting `role==S_ADDRESS` from the wrapper would likely let a unit use `absolute` storage while every existing test still passes.
- **Evidence:**
  - The 14 negative cases include only three that exercise this wrapper: `BufLen`, `HeapPtr` and `RecurPtr`. All three take the S_SYSTEM branch.
  - No case covers `ConOutPtr`, `CBreak` or an `absolute` variable.
  - S_HOOK has a second gate, because `supported()` checks `hook_variables`.
  - In the supplied code, S_ADDRESS relies on `console_dynamic` alone.
  - No test proves that a shadow leaves a later real use detectable.
- **Fix:** Add these cases, which fit the existing helper:
  ```c
  unsupported("var A:Integer absolute $100;","A:=1");
  unsupported("","ConOutPtr:=0");
  unsupported("","CBreak:=true");
  unsupported("procedure Q(BufLen:Integer);begin end;","BufLen:=1");
  ```
  Update the expected total of 298 to match. The last case confirms that a parameter shadow does not hide the outer symbol in a sibling routine.

### 4. Low: omission of the unused private sibling is not asserted
**Location:** `UnusedSibling`, `tools/test-units.py`

- **Trigger and impact:** If private dead-code elimination regressed, `UnusedSibling` would be linked into the program and the test would still pass.
- **Evidence:** The body-size and byte-identity checks cover only the exported `TileMath.Unused`. `V+222` is also not a distinctive byte pattern to search for.
- **Fix:** Put a unique string literal in `UnusedSibling`. Use `copy_named` to save `SLICE.COM` before `fault_location()` overwrites it, then assert that the marker is absent. The extra check changes the expected total.

## Questions

1. **Should macro-on and macro-off output differ for non-unit shadow programs?** A program declaring `var ConOutPtr:Integer` or `var CBreak:Boolean` no longer sets `console_dynamic` when macro-on, but it still does when macro-off. This looks safe, and the assignment keeps macro-off unchanged. It could still cause byte differences if macro-on output is compared with legacy output. Please confirm that this is intended, or compare one such program under both builds.

## Verified by inspection

- **Macro-off:** The `#ifndef` blocks preserve the old flagging conditions. `cc_lookup_decl` still maps to `cc_lookup`, including at the declaration sites.
- **Macro-on real-use logic:** The wrapper keeps the same conditions and the same `cc.suppress` guard. Qualified lookups return early but only resolve imported routines, which have no restricted roles.
- **Lookup call sites:** `cc_new_symbol` and `routine()` are the only `cc_lookup_decl` callers in the supplied code. Both are declaration-only. Use sites in the supplied code still call `cc_lookup`.
- **Fixture values:** `Exercise(5)` is `22+24=46`, and `Exercise(7)` is `26+24=50`.
- **Recursive locals:** If `Factorial` lost its locals, `Saved<>V` might not fire, but the result would change and the 46 check would fail.
- **Regression sensitivity:** The `BufLen` and `ConOutPtr` parameters would fail under the old declaration-time flagging, so the fixture does detect the original bug for parameters.
- **Assertion count:** The new code appears to add 19 checks: 5 in `author_loaded` and 13 in `execute_loaded` on the saved path. That leaves 1 for `execution`, assuming it has one load check. `execution` was not supplied, so the final run must confirm the total of 298.

## Review limitations

- **Missing source:** The declaration parser, including `cc_declarations`, `cc_storage` and `absolute` handling, was not supplied. Neither were `cc_entry_reference`, the `execution` helper or the file, console and CRT statement code.
- **Unaudited lookups:** I could not audit any `cc_lookup` calls in that missing code. A declaration-only lookup there could still flag a shadow falsely. Please provide the declaration parser or list its `cc_lookup` and `cc_lookup_decl` call sites.
- **Compiler behavior:** Correct code generation for the fixture on SDCC and in the emulator remains unproven until the final stable run. Banked code-size effects also remain unverified.

### Codex reconciliation

1. Accepted. Transform now calls Forwarded(N) in its case selector before Forwarded's body appears. Exercise still produces46 and50 in RAM and COM.
2. Accepted. Added a private function named HeapPtr (exercising routine declaration lookup), unit-global CBreak, and unused unit-level absolute A shadowed by HeapPtr's A parameter. Actual calls resolve to those user symbols. The original BufLen and ConOutPtr parameter cases remain.
3. Accepted. Four additional negatives cover absolute A use, ConOutPtr, CBreak, and a sibling's actual BufLen access after an unrelated Q(BufLen) declaration. All reject with UNIT_UNSUPPORTED and restore page counts. The negative count is now18.
4. Accepted. UnusedSibling includes PRIVATE-DEAD-CODE-9B71 in an assigned local string; the host asserts that exact literal exists in PRIVATEOPS.ZPU and is absent from PRIVATE.COM saved before the fault fixture. This is not an assumed side effect of successful execution.
Question: macro-on and macro-off output need not be byte-identical for programs that only declare shadows. Moving accidental declaration side effects to actual uses is intentional in the experimental compiler; byte parity across compiler variants is not promised. Macro-off source conditions are preserved, production zpascal.bin/.zpl rebuild byte-identically, and current macro-off control64/routine75 tests pass. Macro-on ordinary corpus1982 tests pass across15 families.
Declaration parser audit: compiler_types.c calls cc_lookup only at parse_type (resolving a type/value expression), and while binding the target of an explicit absolute alias. These are dependencies on existing symbols, not declaration-name probes. All declaration names go through cc_new_symbol; routines also use cc_lookup_decl. The full declaration parser is supplied in follow-up.
Stable final gate: analysis/units/20261105T104916.884077Z-92888/report.json passes323 native checks with authenticated completion, expected markers, both omission proofs, compiler-byte capture identity, and input/artifact/emulator stability. Initial298 run's stability failure was expected due to the fixture edit during build; it is not used as passing evidence. Final evidence is docs/private-unit-evidence.json. Existing unit formats/emission/linker and production startup allocation were not changed. Native bank bounds remain checked by package-native.py.
No current source defect identified; the added fixture proves concrete private paths, not every possible compound combination. Startup remains13 code pages and zpascal.zpl remains202277 bytes.

## Opus turn 20261105T105536-bb601a43

Mode: code; read-only

# Follow-up review of private-units-v1

**I can't fully confirm that the bounded change has no defects: one low-severity gap remains.** A variant-record tag field can still trigger the false rejection this milestone set out to remove. All four initial findings are resolved, and I found no regressions or new fixture defects. I did not run anything. Results below labeled as reported come from the supplied summary and reconciliation, and the full evidence report was not supplied.

## Established finding

### 1. Low: a variant tag name still counts as a real use
**Location:** `field_list` in `src/compiler_types.c`, `docs/pascal-units-experiment.md`

- **Trigger:** A unit declares a variant tag with a restricted name, even if it never uses it:
  ```pascal
  type R=record case BufLen:Boolean of true:(X:Integer) end;
  ```
- **Impact:** The unit is rejected with `UNIT_UNSUPPORTED`, although it only declares a field. The failure is safe, and this is existing behavior rather than a regression. However, the docs now say that declaration lookup no longer commits runtime dependencies and that the conservative rejection is fixed.
- **Evidence:**
  - `field_list` calls `cc_named_type()` before it knows whether the identifier is a tag name or a type name.
  - `cc_named_type()` calls `cc_lookup()`, which resolves `buflen` to S_SYSTEM and sets `console_dynamic` in unit mode.
  - The parser then sees `:`, restores the cursor and treats the identifier as a new field, but the flag stays set. `supported()` then rejects the unit.
  - The same path applies to hook names and to a tag named after an `absolute` variable.
  - The reconciliation audit missed this site because it counted direct `cc_lookup` calls, not the call through `cc_named_type`.
- **Suggested fix:** Save the flag before the probe and restore it once `:` confirms a tag, before the second `cc_named_type()`. That second call still flags a real restricted type lookup.
  ```c
  #ifdef ZP_UNITS_EXPERIMENT
  uint8_t dynamic;
  #endif
  ...
  cc_next();
  #ifdef ZP_UNITS_EXPERIMENT
  dynamic=cc.console_dynamic;
  #endif
  named=cc_named_type();cc_source_save(&tag_cursor);cc_next();
  if(cc.token==':') {
  #ifdef ZP_UNITS_EXPERIMENT
      cc.console_dynamic=dynamic;
  #endif
      cc_source_restore(&tag_cursor);...
  ```
  The `#ifdef` keeps macro-off behavior unchanged.
- **Verification:** Add the `R` type above to the implementation section of `private-unit.pas`. This adds no assertions, so the total stays at 323. Rerun the unit gate, and rerun the macro-on corpus and production byte checks because `compiler_types.c` would change.
- **If deferred:** Narrow the docs to say this rejection remains for variant tag names. The change would otherwise be small, but deferring is reasonable because the failure is safe and the construct is rare.

## Initial findings

| Initial finding | Status | Confirmation |
|---|---|---|
| 1. Forward call before body | Resolved | `Transform` calls `Forwarded(N)` before its body appears. |
| 2. Declaration-site coverage | Resolved for the requested sites | Private `HeapPtr` exercises `routine()`, global `CBreak` exercises a variable declaration, and parameter `A` shadows an S_ADDRESS symbol. |
| 3. Real-use rejection | Resolved | `A:=1` depends on the wrapper's S_ADDRESS branch alone. The hook, `CBreak` and sibling `BufLen` cases are also present. |
| 4. Private sibling omission | Resolved | The host checks that the literal is in `PRIVATEOPS.ZPU` and absent from `PRIVATE.COM`, copied before `fault_location()` overwrites `SLICE.COM`. |

**Macro-on versus macro-off output:** This question is closed. Your disposition makes the byte differences for declaration-only shadows intentional.

## Verified by inspection

- **Fixture values:** `HeapPtr(4)` returns `Transform(4)`, so the results remain `22+24=46` and `26+24=50`. All new shadows resolve to the user symbols.
- **Assertion count:** `execution` has one load check, confirming 19 checks for the private fixture. Four new negatives at 6 checks each, plus the `PRIVATE.COM` copy check, give `298+24+1=323`.
- **Corpus total:** The 15 macro-on family counts in the summary add up to 1982.
- **Other declaration paths:** Variables, types, constants, enum members, labels, parameters and routines all use `cc_new_symbol`. Record fields use `new_field`, and typed-constant field names use `cc_field_lookup`. None of these commits a flag. The explicit `absolute` target and other `cc_lookup` calls in `compiler_types.c` are real dependencies, apart from the variant-tag probe above.

## Review limitations

- **Reported results:** The 323 unit assertions, macro-off controls and byte-identical production rebuild are reported results. I could not check them against the full evidence report.
- **Corpus timing:** Your stated order is sound because only `units-main.c` changed after the corpus run. Applying the fix above would make that corpus evidence stale.
- **Source still missing:** The definitions of `cc_storage`, `cc_entry_reference`, and the file, console and CRT statement handlers were not supplied. I could not audit their lookup calls directly, although they are expected to be use sites.

### Codex reconciliation

Accepted remaining low-severity limitation: the variant-tag type probe calls cc_named_type speculatively and can mark a restricted symbol before recognizing a field name. Deferred rather than expanding this completed two-turn milestone. The contract now narrows the fix to tested parameter/variable/routine declarations and names this remaining false rejection. It is existing fail-closed behavior, not a new unsafe use path. No code or test changes after the follow-up; current323 unit and1982 corpus results remain valid. All four initial review findings are resolved. No additional review round was run, and formal finalization is not claimed after the post-review documentation clarification. Startup RAM work is a distinct next milestone requested by the user's memory question.
