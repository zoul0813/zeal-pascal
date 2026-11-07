# Review: unit-dependencies-v1

## Opus turn 20261106T234251-3075dafa

Mode: code; read-only

# Review of `unit-dependencies-v1` (3d5f1d4..263e70e)

**Result:** I found four actionable correctness or regression defects and three lower-severity issues. I did not find a code-bank lifetime, page ownership, artifact-binding or saved-program dependency defect in the supplied code. Those conclusions are limited by the missing files listed at the end.

I reviewed the code statically and ran nothing. Line numbers aren't available in the supplied material, so locations are given by symbol.

## Established defects

### 1. High: valid pointer graphs can be rejected as by-value cycles
**Location:** `src/compiler_units_metadata.c`, `import_type`, in-progress check on `ref`

**Trigger:** A record is first entered through a pointer, then reached by value on a path through another pointer:

```pascal
type
  PHeader = ^THeader;
  PNode   = ^TNode;
  THeader = record Owner: PNode end;
  TNode   = record Hdr: THeader end;
```

**Impact:** The unit compiles and publishes, but every importer fails with `UNIT_FORMAT`, even if it never uses these types. This breaks the invariant that pointer cycles must work.

**Evidence:**
1. `PHeader` is imported first and follows `THeader` with `indirect=1`, leaving `THeader` at status 1.
2. Field `Owner` imports `PNode`, which follows `TNode`.
3. `TNode.Hdr` calls `import_type(THeader, 0)`.
4. The cached entry has status 1, `indirect` is 0, and its family is not `K_POINTER`, so `cc_fail(UNIT_FORMAT)` fires.

There is no value cycle here, because `THeader` only holds a pointer. Status 1 conflates "on the current value path" with "suspended behind a pointer". The existing fixtures don't enter a type through an unrelated forward pointer first.

**Fix:** Separate pointer target resolution from value-layout validation:
- Allocate pointer descriptors and defer their targets through a worklist, or
- Run a two-phase import that allocates all descriptors, then runs a DFS over value edges only.

**Verification:** Add the example above to host and native tests. Also add a mutated true value cycle reached through a pointer, and confirm it still fails.

### 2. Medium: qualified forward pointer targets do not parse
**Location:** `src/compiler_types.c`, `parse_type_body`, `'^'` branch

**Trigger:** `type P = ^Shapes.TPoint;` in a top-level type section.

**Impact:** The declaration fails with a syntax error, although `docs/unit-types.md` says qualification is supported.

**Evidence:** With `allow_forward=1`, the parser skips `cc_named_type()`, stores a reference to `Shapes`, and makes one `cc_next()`. The token is then `.`, so `cc_declarations` fails at `cc_expect(';',1)`. The non-forward path works because `cc_lookup` consumes the qualifier.

**Fix:** In the forward branch, consume and record `Unit.Name`, then resolve it through the qualified path in `resolve_pointers`. Rejecting the form explicitly and documenting that limit is also acceptable.

**Verification:** Test forward and non-forward pointers to qualified types, both in a program and in another unit's interface.

### 3. Medium: authoring accepts variant labels that the loader rejects
**Location:** `src/compiler_types.c`, `field_list`; `src/compiler_units_metadata.c`, `import_type`, `S_VLABEL` branch

**Trigger:** A label has the right family but is outside the tag's range:

```pascal
type T = 1..3;
     R = record case T of 5: (A: Integer) end;
```

`case Byte of 300: ...` triggers the same failure. The declaration can be private or unused.

**Impact:** `compile_unit` publishes an artifact that no client can load, and the diagnostic blames the importer's `uses` clause.

**Evidence:** `field_list` compares only `cc_family`. The importer rejects `value < low || value > high`, and it validates complete type graphs even when they are unused.

**Fix:** Apply the same rule on both sides. If source acceptance should remain unchanged, relax the importer. Otherwise, reject the label during authoring with a located diagnostic.

**Verification:** Add native tests for the Byte and subrange cases. A round-trip semantic check before publication would catch this whole class of mismatch.

### 4. Medium: raw metadata references turn reference creation quadratic
**Location:** `src/compiler_sources.c`, `cc_source_reference`

**Trigger:** Importing any unit whose type graph includes a named field, or materializing a String or Real constant.

**Impact:** After that point, every new source reference scans all existing references. This regresses compile time for the rest of the source, and eager graph validation means an unused private record is enough to trigger it. `uses` runs at the start of the file, so nearly all of a program's references are affected. The cost has no bank-reload or memory symptom, so the listed suites would not expose it.

**Evidence:** The early exit requires `position > highest_reference`. A raw position has bit31 set, so storing it makes `highest_reference` larger than every ordinary source position. The duplicate scan then runs on each call.

**Fix:** Track raw and source maxima separately, or never let raw positions update `highest_reference`. A duplicate raw reference is harmless because type and export materialization are already cached.

**Verification:** Add a host or instrumented test that imports a raw reference first, then confirms a new ascending source position skips the scan and replayed references still deduplicate.

### 5. Low: stack exhaustion is misreported
**Location:** `src/compiler_units_metadata.c`, `import_type`; `src/compiler_units_graph.c`, `cc_units_graph_load`

**Impact:** In `import_type`, a valid but deeply nested graph on a low stack is reported as "Invalid compiled unit" rather than a resource failure.

**Evidence:** `import_type` combines bad wire IDs and `!cc_stack_check()` into one `UNIT_FORMAT` failure. Every other supplied caller of `cc_stack_check` that diagnoses failure uses `cc_fail(99)`. `cc_units_graph_load` is the exception: it returns 0 with no `cc_fail`. If `cc_stack_check` does not set an error itself, the parser continues from the unit identifier and reports an unrelated follow-on error. Each graph-load frame also keeps `path` and `directory` (256 bytes) alive across recursion.

**Fix:** Use `cc_fail(99)` for stack failure in both places. Consider moving the path buffers into a non-recursive helper.

**Verification:** Test a dependency chain deep enough to hit the stack limit, and check the resulting diagnostic.

### 6. Low: CRT packaging checks the old artifact limit
**Location:** `tools/package-screen.py`, `assert len(artifact) <= 16384`

**Impact:** A `CRT.ZPU` of 16321 to 16384 bytes would package successfully, then fail at load because `ZPU_ARTIFACT_LIMIT` is 16320.

**Fix:** Assert `len(artifact) <= 16320`, or derive the limit from the header.

### 7. Low: metadata bank reloads on every new imported name
**Location:** `src/compiler_units_names.c`, `materialize`, `cc_units_types_ready`; `src/compiler_units_graph.c`, `cc_units_graph_validate`

**Impact:** Compile time on EEPROM or TF storage grows with distinct imported names and modules:
- Every cache miss reloads and drops the metadata bank from disk.
- Graph validation repeats the file-bank and metadata-bank swap per module.
- `cc_fragment_validate` reruns all relocation checks for each `uses` item.

The memory invariants still hold, and cache hits avoid the reload as intended. The evidence does not measure this I/O cost.

**Fix:** Keep the metadata bank resident across one declaration or `uses` clause, and validate once at the end of the clause. If that conflicts with the page budget, measure and record the cost instead.

## Test gaps

- **Type graphs:** add entry through an unrelated forward pointer, cross-unit pointer cycles, and valid graph depth at the stack limit.
- **Graph limits:** add 31 dependencies and 32 loaded modules, one-over-limit cases, and artifacts of exactly 16320 and 16321 bytes.
- **Crafted manifests:** the native test covers only the current-unit cycle. Add a loaded-artifact cycle with a valid checksum, wrong wire order, duplicate names, and an out-of-range origin.
- **Bank frames:** materialize a first-use imported name from inside a file-generator call, such as `Write(F, Unit.Name)`, and assert that the file bank is not lent.
- **Failure paths:** inject `code_phase_unit_link_end` restore failures from type validation, final link and serialization.
- **Cycle diagnostic:** cover the display of code 247 in the IDE, not only `d.code`.

## Questions and context needed

1. **Failed bank restore:** Does any code call into the file bank after `code_phase_unit_link_end` fails? I need `compiler_compile.c`, `compiler.c` and the helper-finish order. Calling that bank while unmapped would be a serious defect, but the supplied files don't show whether it can happen.
2. **`cc_stack_check` behavior:** Does it set a diagnostic itself? This decides whether the silent `cc_units_graph_load` path in finding 5 is real. I need `compiler_tables.c`.
3. **Code 247:** Is it unused by other diagnostic classification? I need all of `compiler_messages.c`, `compiler_job_result.c` and the `COMPILE_*` definitions.
4. **Source table capacity:** Can `sources_unit` hold every documented loaded module, including unused transitive ones? I need `sources.c`.
5. **Raw constants in expressions:** Does any code in `compiler_expr.c` seek a constant reference directly instead of using `cc_constant_literal` or `cc_real_literal`? The native tests cover common paths only.
6. **Reserved words:** Is it accepted that production now reserves `unit`, `interface`, `implementation` and `uses`? `docs/unit-exports.md` documents this, but CP/M-era programs using those identifiers will no longer compile.

## Review limitations

- `compiler_compile.c`, `compiler_expr.c`, `compiler_tables.c`, `sources.c`, `tests/native/units-main.c` and the evidence captures were not supplied.
- Code-bank and memory conclusions come from the visible call paths, and I did not check the reported test counts against capture files.
- The comments in `compiler_routines.c` ("primitive parameters only") and `compiler_units_object.c` ("primitive signature") are also out of date. They have no runtime impact.

### Codex reconciliation

1. Accepted: defer pointer targets to an append-only worklist, distinguishing pointer edges from by-value recursion. Host/native fixtures cover the reported graph, true value cycles and cross-unit recursive aliases.
2. Accepted: parse qualified forward targets and resolve through normal qualified lookup. Program and interface fixtures cover forward/nonforward forms and reject a local record used as a qualifier.
3. Adapted: preserve existing descriptive variant-label source semantics; remove only importer range rejection. Byte300 and subrange5 now round-trip source-free and execute from saved programs.
4. Accepted: raw references neither scan ordinary references nor update their high-water mark. Real source-reference host instrumentation proves ascending fast paths and replay/context deduplication.
5. Rejected diagnostic claim: compiler_tables.c cc_stack_check already sets99, and cc_fail preserves the first diagnostic. Split the checks for clarity; host injected failure verifies99. Recursive path-buffer reduction is deferred optimization.
6. Accepted: CRT packager enforces16320 bytes, matching invocation binding reservation.
7. Deferred deliberately: keep transient metadata rather than another resident page. A metadata cache miss reloads the metadata payload; graph checks swap file compiler and metadata. This preserves ten cold/fifteen warm code pages and avoids previously observed diamond resource failures. No elapsed-I/O benchmark is claimed.

Verification: 1703 target assertions,502 CRT assertions,52 ASAN/UBSAN host assertions,66 IDE checks and51 failure/cancellation/retry checks. The first-use Write(F,Literals.Answer) test succeeds while file generation is active; its expected Text close byte is0x1A. See docs/unit-review-evidence.json for captured results and hashes. The remaining broader suggested graph-limit/failure-injection fixtures are not claimed as implemented.

Context answers: compiler_compile.c gates failed phase restore directly to cleanup before file helper finish; retained-bank cleanup does not call the unmapped file compiler. In-generator materialization never lends its active file bank. Stack checks set99. Code247 has a unique explicit message, below platform254/interrupted255. sources.c supports255 slots versus32 modules plus root, subject RAM. compiler_expr.c uses literal helpers for raw constants. Production unit/interface/implementation/uses reservation is a documented deliberate Zeal extension; legacy mode is unchanged.

Follow-up pending: assess fixes and newly supplied context; initialization remains outside this milestone.

## Opus turn 20261107T002556-9027901d

Mode: code; read-only

# Follow-up review of `unit-dependencies-v1`

**Result:** I found no product-code blocker, and the fixes for findings 1–4 and 6 hold. Finding 5's diagnostic claim was my error, and the deferral of finding 7 is acceptable.

One verification problem should be closed before this milestone is called fully verified: `tests/native/units-main.c` still expects behavior this milestone removed.

This is a static review. I ran nothing and did not inspect the capture directories.

## Dispositions

| # | Status | Assessment |
|---|---|---|
| 1. Pointer cycles | **Resolved** | `UT_VISITING` now marks only the current value path, because pointers leave the DFS and are bound later. `bind_pointers` re-reads `UH_USED`, so pointers appended during binding are handled. It uses each record's owning module, so cross-unit aliases bind correctly. The fixtures cover the reported graph, a true value cycle behind a pointer, and a recursive external alias. |
| 2. Qualified pointers | **Resolved** | Forward and inline forms both work. Replay rejects a local `R.NotAType`: if no qualifier was consumed, `token_start` still equals the stored position, so it fails with 42. `^Kinds.;`, a missing member, and a non-type member also fail. |
| 3. Variant labels | **Resolved** | Removing the importer range check matches the existing parser, and no supplied code depends on a label being in range. Structural checks remain. |
| 4. Raw references | **Resolved** | Raw references skip the scan and no longer raise `highest_reference`. Ordinary replay and context deduplication are unchanged. Raw references can repeat, but materialization caches them, so the growth is bounded. |
| 5. Stack diagnostic | **Rejection accepted** | My claim was wrong. `cc_stack_check` already calls `cc_fail(99)`, and `cc_fail` keeps the first diagnostic. Neither the `UNIT_FORMAT` misreport nor the silent `cc_units_graph_load` path was real. Splitting the check is still a clearer form. |
| 6. CRT limit | **Resolved** | `package-screen.py` now asserts 16320. |
| 7. Metadata reloads | **Deferral accepted** | The tradeoff preserves the ten-cold and fifteen-warm page budgets. The cost remains unmeasured, and the record correctly makes no timing claim. |

## Initial questions

- **Failed bank restore:** Resolved for the supplied paths. Link and write failures jump to `failed` before any file helper runs. After a failed `uses`, the token stays on the unit identifier, or the error is already set, so `cc_block` cannot reach statement generation.
- **`cc_stack_check`:** Resolved. It sets 99 itself.
- **Code 247:** Resolved. It has its own message and is distinct from 254 and 255.
- **Source capacity:** Resolved. `sources.c` allows 255 entries, which covers 32 modules plus the root, subject to RAM.
- **Raw constants in expressions:** Resolved. `compiler_expr.c` uses `cc_constant_literal` and `cc_real_emit`, and `cc_source_seek` rejects a stray raw position.
- **Reserved words:** Accepted as a documented Zeal extension. Legacy mode is unchanged.

## Remaining findings

### 1. Medium: `units-main.c` still expects a rejection this milestone removed
**Location:** `tests/native/units-main.c`, stage 3; `src/compiler_units.c`, `cc_units_export`

**Trigger:** Run `tools/test-units.py`, which is listed in `tools/test-plan.json`.

**Impact:** The suite should fail, or its result is unknown. The review evidence reports no `zunittest` result, and `units-main.c` is absent from the diff.

**Evidence:** Stage 3 expects `UNIT_UNSUPPORTED` for:

```pascal
unit BadUnit;interface procedure P(var X:Text);implementation procedure P(var X:Text);begin end;end.
```

The `primitive()` check that rejected this is gone:
1. `cc_units_export` now records the export.
2. `repeated_heading` matches the Text type.
3. The empty body sets no flag that `supported()` rejects.
4. `author()` therefore sees success where it expects failure.

`docs/unit-types.md` now lists file types as supported, so the test expectation appears stale rather than the code being wrong.

**Fix:** Change the case to a positive one, or use a body that really is unsupported. Then run `tools/test-units.py`, record its result, and check it for old header or export offsets. I can't assess that script because it wasn't supplied.

### 2. Low: documentation still reports old status
**Location:** `docs/unit-types.md`; `docs/unit-exports.md`

- `docs/unit-types.md` still reports 1,595 target and 39 host assertions, although it also links the new evidence.
- `docs/unit-exports.md` still says Opus review is pending.

**Fix:** Update both documents. The older evidence JSON files can stay as historical checkpoints.

## Remaining risks, not demonstrated failures

- **Page exhaustion during materialization:** A first-use name inside the file generator can't borrow the file bank. If no page is free, the compile fails safely, but the error is a platform failure rather than 99.
- **Dependency recursion margin:** The 768-byte stack check may be smaller than one graph-load call chain. By my estimate, reaching it needs far more nested units than the page budget can hold, so I don't consider it reachable today.
- **Imported file types:** Native tests cover `SizeOf(TBin)`, but not file operations on imported file types. `cc_compatible` handles these types through `capacity`, but `compiler_files.c` wasn't supplied, so I can't confirm the rest of that path.
- **Coverage gaps:** Exhaustive graph-limit, crafted-manifest and restore-failure tests remain open, as you recorded. The record does not claim them as coverage, and I found no failure tied to them.

## Review limitations

- `tools/test-units.py`, `compiler_files.c` and the capture directories were not supplied.
- The reported counts and the `RAWCONST` re-evaluation are taken from your evidence, not reproduced.

### Codex reconciliation

Accepted both follow-up findings. Changed only the stale Text export test expectation to success and refreshed the current documentation. The older unit acceptance suite now passes all404 native assertions and13 Python checks (docs/unit-review-evidence.json); its header reader still uses valid offsets. No production source changed after the follow-up. Product-code review found no blocker. Imported file operations and exhaustive limit/fault-injection cases remain disclosed test gaps, not established failures. Metadata reload remains a deliberate unmeasured I/O tradeoff (current payloads: metadata13818 bytes, file compiler14914, unit I/O15401).

The workflow remains unfinalized because the supplied test and documentation inputs changed to address this follow-up. The two Opus turns are complete; these small test/documentation corrections were verified locally without an extra review round. Initialization is a separate subsequent milestone.
