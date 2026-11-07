# Review: unsigned-scalars-v1

## Opus turn 20261107T022858-1ff0aff1

Mode: code; read-only

# Review: unsigned-scalars-v1

I found 9 established defects and 1 test coverage gap. **This change isn't ready.** Findings 1–4 can generate incorrect code or break existing programs. I traced these by reading the code and ran nothing, and native QA is still pending.

## Established defects

### 1. High: `Mem` and `Port` accept LongWord and use the temporary's address
- **Location:** `src/compiler_expr.c`, `raw_index`
- **Trigger:** `Mem[L]`, `Mem[L]:=1`, `Port[L]`, or `Mem[$12345]`.
- **Impact:** The program accesses a stack address rather than the value in `L`. Scope requires unsupported wide operations to reject safely.
- **Evidence:** The `cc_integral(expression())` check accepts `K_LONGWORD`. A LongWord factor leaves HL pointing to its 4-byte temporary. `raw_index` then releases that temporary and uses HL as the address.
- **Fix:** Accept Integer and Word only, and add compile-rejection tests for `Mem` and `Port`.

### 2. High: Hex literals from `$8000` to `$FFFF` regress in the production dialect
- **Location:** `src/compiler_expr.c`, `factor`; `src/compiler_front.c`, `cc_constant`
- **Trigger:** These forms now fail:
  - `var X:Byte absolute $C000`, with error 22
  - `Ptr($C000)`, with error 23
  - `FillChar(X,$8000,0)`, `Move(A,B,$8000)`, `GetMem(P,$8000)`, and `StackPtr:=$F000`, through `integer_value`
  - a `Port` value of `$8000` or above
- **Impact:** Valid programs stop compiling, and others change behavior:
  - `I:=$FFFF` raises 0x91 under `{$R+}`.
  - `Write($FFFF)` prints `65535` rather than `-1`.
  - `I<$8000` and `$FFFF div 2` become unsigned.
- **Evidence:** Both paths now classify these values as Word. The consumers above still require `K_INTEGER`. The legacy branch treated the same literals as Integer.
- **Fix:** Keep unsuffixed four-digit hex literals as Integer, or give them a separate literal class that converts to Integer contexts. Add regression tests for each form above. Also check `compiler_inline.c`, which wasn't supplied.

### 3. High: Case labels can compare the wrong value
- **Location:** `src/compiler.c`, `case_constant` and `case_compare`
- **Trigger:** `case W of 70000:`, `case I of 100000:`, `case I of 40000:`, or `case W of -1:`.
- **Impact:**
  - A LongWord label compares against a source-reference ID, so it can match an unrelated selector value.
  - Integer selectors alias `40000` to `-25536`.
  - Word selectors alias `-1` to `65535`.
- **Evidence:** `cc_constant` returns a source reference for LongWord constants. `case_constant` accepts any integral pair and uses the returned value directly.
- **Fix:**
  - Reject LongWord labels.
  - Reject Word labels above 32767 for Integer selectors.
  - Reject negative labels for Word selectors.
  - Apply the same checks to both range bounds.

### 4. High: Scalar typed constants store truncated or unrelated values
- **Location:** `src/compiler_types.c`, `cc_declarations` and `cc_typed_storage`
- **Trigger:** `const I:Integer=100000;`, `const B:Byte=70000;`, or `const I:Integer=40000;`.
- **Impact:** A LongWord literal stores its source-reference ID if that ID passes the signed range check. `40000` stores `-25536`. The legacy dialect rejected these decimal values.
- **Evidence:** Integer and Byte targets skip `initializer`, which contains the LongWord and Word checks. `cc_compatible` now accepts these values, and the following range check treats `value` as a signed integer.
- **Fix:** Send integral scalar targets through `initializer`, or reject these cases in the scalar branch.

### 5. Medium: Set constructors wrap Word to the Real type
- **Location:** `src/compiler_expr.c`, `set_constructor`
- **Trigger:** `R:=[W]` or `R:=[40000]`.
- **Impact:** A set constructor can be typed as Real, so the assignment copies 6 bytes from the 32-byte set temporary.
- **Evidence:** `cc_ordinal` now accepts `K_WORD`, and `K_SET_EXPR+K_WORD` is `0x8000+0x8005`. That wraps in 16-bit `TypeId` arithmetic to `5`, which is `K_REAL`.
- **Fix:** Reject Word-family elements unless a separate set-base encoding is added. Add a test for `R:=[W]`.

### 6. Medium: Mixed signed comparisons and division are mathematically wrong
- **Location:** `src/compiler_unsigned_ops.c`, `cc_unsigned_binary`
- **Trigger:** `W>=I` with `I=-1`, `W>-1`, `-10 div W`, or `L=-1`.
- **Impact:** With `W=5` and `I=-1`, `W>=I` is false because `-1` compares as `65535`. `-10 div W` divides `65526`, and `L=-1` is true for `$FFFFFFFF`.
- **Evidence:** Word comparisons flip both sign bits without checking for a negative Integer operand. Word division passes `sign=0` to the helper. Wide paths sign-extend Integer to 32 bits and then use unsigned operations.
- **Fix:** Choose and document one rule. I recommend comparing and dividing by mathematical value, or rejecting non-constant mixed-sign operations. Add boundary tests for either rule.

### 7. Medium: Negative literals and bounds silently wrap
- **Location:** `src/compiler_expr.c`, `factor`; `src/compiler_front.c`, `cc_constant`; `src/compiler_types.c`, `parse_type_body` and `initializer`
- **Trigger and impact:**

| Source | Result |
|---|---|
| `I:=-40000` | Stores `25536`; this was previously a compile error |
| `const L:LongWord=-1` | Stores `65535`, while `LongWord(-1)` produces `$FFFFFFFF` |
| `const W:Word=-1` | Stores `65535` |
| `-1..65535` | Creates the one-value subrange `65535..65535` |
| `-32768..40000` | Creates the Word subrange `32768..40000` |

- **Evidence:** Negative decimals beyond 32768 use modular unary negation. Initializers read the raw `uint16_t`. The Word-subrange check compares raw bounds without checking for a negative lower bound.
- **Fix:** Reject negative decimals below `-32768`, unless the language rule explicitly allows them. Reject negative values for unsigned constants and Word subranges unless an explicit cast is used.

### 8. Low: `Chr(W)` and Char casts always fail
- **Location:** `src/compiler_unsigned_intrinsics.c`, `F_CHR`; `src/compiler_unsigned.c`, `cc_unsigned_convert`
- **Trigger:** `Chr(W)`, `Char(W)`, or `Word('A')`.
- **Impact:** Each form fails with error 44, and the `F_CHR` branch can't succeed. The rejection is safe, but it leaves dead code and limits casts.
- **Evidence:** `cc_unsigned_convert` requires both sides to be integral, and Char isn't.
- **Fix:** Support explicit ordinal casts and `Chr(Word)` with `K_CHAR` range checking, or remove the branch and document the restriction.

### 9. Low: Metadata can define a set of Word that source rejects
- **Location:** `src/compiler_units_metadata.c`, `import_type`, `K_SET` branch
- **Trigger:** A checksummed ZPU has a Word set element with `low=0`, `high=-1`, and `width=0`.
- **Impact:** The importer can accept a set type that the source compiler rejects with error 70.
- **Evidence:** The import check uses signed bounds, while `parse_type_body` has the required unsigned Word check.
- **Fix:** Reject Word elements above 255 using unsigned comparison, and add a decoder test.

## Test coverage gap

### 10. Medium: Native tests don't prove the required safe rejections
- **Location:** `tests/native/unsigned-main.c`
- **Evidence:** `failure()` covers runtime errors, and no current call uses `author_result` with a non-zero expected code. The suite has no compile-rejection cases for LongWord `For`/`Case`, `Read`/`Val`, or unsigned/Real coercion.
- **Fix:** Add rejection cases for those forms and findings 1, 3, 4, 5, and 7. Also add runtime tests for:
  - `$FFFF shr 1`
  - large Word multiplication
  - shifts of 16 or more
  - `Write(L:12)` and `Str(L:12,S)`
  - `A[I]:=L`, where `A` is a Word array

## Questions

1. **Does a LongWord return need full-memory mode?** `cc_frame_end` and wide `Swap` use `RT_REAL_BUFFER`, but only `cc_unsigned_call` requires full memory. This program may build as shared-stack: `var L:LongWord; function F:LongWord; begin F:=$12345678 end; begin L:=F end.` Please confirm that the buffer is valid and mapped in that mode.
2. **Are the shared Word helpers correct for unsigned values?** The Word path reuses `RT_MUL`, `RT_SHL`, and `RT_SHR`. Please confirm that `RT_SHR` is logical and `RT_MUL` is modular with no overflow failure. The current test only checks `3 shr 1`.
3. **Should Word `Succ` and `Pred` keep range checks?** They now use modular arithmetic and return base Word, including for subranges. Existing ordinals call `cc_check_range(kind)`.

## Review limitations

- **Unavailable sources:** I didn't have `compiler_io.c` or the complete current `compiler.c`, so I can't confirm the LongWord `For`/`Case`, `Read`, or `Val` guards. Widening `cc_numeric` and `cc_ordinal` makes those hidden checks important.
- **Other unavailable sources:** I also didn't have `compiler_inline.c`, `zp.h`, or the implementations of `cc_temporary`, `cc_store_indirect`, `cc_units_owner`, and `cc_invoke`. I relied on their visible usage patterns.
- **Evidence:** The Z80 and decoder results are reported evidence, and I didn't run them. Current native QA must not be treated as passing until the rerun completes.

### Codex reconciliation

Accepted findings 1–5 and 7–10. The compiler now rejects wide raw addresses,
unsafe Case labels, invalid scalar typed constants, Word set constructors,
negative unsigned declarations/bounds and too-negative decimal literals. All
production integral typed scalars use the normal initializer replay. Four-digit
hex literals preserve the established signed Integer bit-pattern behavior;
unsigned masks use decimal values or explicit Word casts. Inline rejects values
wider than its 16-bit operand format. Chr/Char and Word(Char) conversions work.
The decoder validates unsigned Word set bounds. Word Succ/Pred retain subtype
range checks. Added 22 safe-rejection cases and boundary/format/hex/return tests.

Adapted finding 6: the deliberate native rule is modular unsigned promotion,
consistent across arithmetic and comparisons. Word promotes a signed Integer to
its unsigned16 bit pattern; LongWord sign-extends Integer and then operates as
unsigned32. This is now stated with examples in docs/unsigned-integers.md and
covered by W>=I, W>-1 and -10 div W checks. Mathematical signed promotion would
require a wider signed type or rejection rules beyond the approved two unsigned
types. No claim is made that this recovers TP7's exact numeric promotion rules.

Answers: RT_REAL_BUFFER is $4136 in the retained runtime prefix, valid in shared
mode. A dedicated fixture checks a LongWord return and Swap through Hi/Lo
without a wide arithmetic helper or console output. RT_SHR uses SRL H/RR L and
returns zero for counts>=16. RT_MUL is modular shift/add with no overflow error.
Full-width Word arithmetic wraps; subtype Succ/Pred range checks are retained.

Evidence so far: generated Z80 engine 26,004 sanitized assertions; unit decoder
64 sanitized assertions; current native unit/Real 1703 and unit initialization
623 assertions pass. Expanded native unsigned run reached 931 passes and one
incorrect allocation expectation: its return fixture itself used wide comparison
and console output. Corrected that fixture to Hi/Lo/Swap-only validation with no
output, retaining the requirement of shared-stack execution. Final rebuild and
all required regressions must pass before the follow-up/finalization.

ZVB's separately drafted Pascal source and demos are outside this milestone.
They are untracked/ignored and will not be included in the unsigned commit.


Checkpoint verification: all932 native unsigned assertions pass, including the
shared-stack return/Swap fixture through an imported Fixed function. Existing
local routine calls deliberately force full-memory mode, so that fixture uses
an imported unit routine whose fragment has no such requirement. Generated
engine26004 and unit decoder64 sanitizer assertions pass; native unit/Real1703,
initialization623, general unit404 and CRT446 assertions pass, with all driver
checks true. Full host regression passes. Production18 banks fit, launcher15668
and ZPL267550 bytes. See docs/unsigned-integers-evidence.json for hashes.

Unresolved review requirement: Claude reports usage_limit_reached, resets
2026-11-06 23:40 America/New_York. The unsigned follow-up has not run. An
attempted separate creative ZVB documentation review failed before inference
with that limit (zero tokens, no edits). No approval or finalization is claimed.
A local tested checkpoint commit is authorized by the user's staged-commit
request; this record explicitly retains the outstanding review requirement.
