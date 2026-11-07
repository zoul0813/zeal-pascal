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

## Opus turn 20261107T083821-90a41f01

Mode: code; read-only

# Follow-up review: unsigned-scalars-v1

**Verdict: I found no established blocker, but this isn't ready to approve.** Finding 1 should be fixed first, and question 1 must be answered because it covers a required safe rejection. I reviewed the supplied sources by reading them and ran nothing. Test results below are as reported.

## Prior findings

| # | Status | Confirmation |
|---|---|---|
| 1 | Fixed | `raw_index` accepts only Integer and Word, with `Mem` and `Port` rejection tests. |
| 2 | Fixed | Both literal paths keep `$8000..$FFFF` as Integer, and the `UHEX` fixture covers the regression forms. |
| 3 | Fixed | `cc_unsigned_case_label` checks both range bounds. |
| 4 | Fixed | Integral typed constants now use `initializer`. This fix introduced finding 3 below. |
| 5 | Fixed | `set_constructor` rejects Word, so the `K_REAL` wrap is gone. |
| 6 | Adapted, accepted | Modular promotion is consistent across the Word and LongWord paths, documented, and tested. |
| 7 | Mostly fixed | The listed cases now reject. Finding 2 below covers a remaining route through negated constants. |
| 8 | Fixed, with a regression | `Chr` and the Char casts work, but the change causes finding 1 below. |
| 9 | Fixed | `import_type` checks Word set bounds as unsigned. |
| 10 | Fixed | The 22 rejection cases and requested runtime boundary tests are present. Question 1 remains a gap. |

The answers to the earlier review questions are consistent with the supplied code, except that the runtime source for the shift claim wasn't supplied (question 3). The shared-stack return path doesn't call the wide engine, and `Succ` and `Pred` retain subrange checks.

## Established defects

### 1. Medium: Word and LongWord implicitly convert to Char
- **Location:** `src/compiler_unsigned.c`, `cc_unsigned_convert`; `src/compiler_unsigned_intrinsics.c`, `F_CHR`
- **Trigger:** `C:=W`, `C:=L`, `C:=40000`, a Word argument to a Char parameter, or `for C:='A' to W`.
- **Impact:** These invalid assignments now compile. Under `{$R-}`, the value silently truncates to one byte. The equivalent `C:=I` still fails with error 44.
- **Evidence:** The `tf!=K_CHAR` exemption applies even when `cast` is 0, and `cc_convert` calls this function with `cast=0`. `F_CHR` relies on that path to get checked narrowing.
- **Fix:** Allow Char targets only for explicit conversions. Give `Chr` a separate checked mode, such as `cast==2`, and replace the `!cast` checks with a `checked` flag.
- **Verification:** Add rejections for `C:=W`, `C:=L`, and `C:=40000`. Add runtime tests that `Chr(W)` and `Chr(L)` raise 0x91 for out-of-range values under `{$R+}`.

### 2. Low: Negated unsigned constants bypass the initializer rule
- **Location:** `src/compiler_front.c`, `cc_constant`; `docs/unsigned-integers.md`
- **Trigger:** `const Big=40000;W:Word=-Big;` or `const L:LongWord=-$10000;`.
- **Impact:** These store 25536 and `$FFFF0000`, although the docs say typed initializers can't implicitly convert negative constants. They also create an inconsistency, since the literal `-40000` is rejected but `-Big` is accepted.
- **Evidence:** The result stays Word or LongWord after negation, so the negative Integer check in `cc_unsigned_initializer` never applies.
- **Fix:** Reject unary minus on unsigned constants in `cc_constant`, or explicitly document modular constant negation.

### 3. Low: Scalar typed constants can replay a shadowing constant
- **Location:** `src/compiler_types.c`, `cc_declarations` and `cc_typed_storage`
- **Trigger:**

  ```pascal
  const Y=1;
  procedure P;
  const X:Integer=Y; Y=2;
  ```

- **Impact:** The declaration validates `X` against the outer `Y=1`, but replay finds the inner `Y=2` and stores 2. The legacy path kept the value 1, so this is a silent production-dialect regression.
- **Evidence:** `X` now stores a source reference instead of its value. `cc_typed_storage` performs the lookup again after the inner `Y` exists. Aggregates already behaved this way, but Integer scalars didn't.
- **Fix:** Store the validated scalar value at declaration and emit it directly, as the existing fallback branch can. LongWord can retain its constant reference.

### 4. Low: Documented ordinal casts reject Boolean and enum types
- **Location:** `src/compiler_unsigned.c`, `cc_unsigned_convert`; `docs/unsigned-integers.md`
- **Trigger:** `Word(True)`, `Word(Red)`, `Color(W)`, or `Boolean(W)`.
- **Impact:** These safely fail with error 44, but the docs say ordinal casts accept unsigned values.
- **Fix:** Support these casts or narrow the documented claim.

### 5. Low: The evidence record and docs are stale
- **Location:** `docs/unsigned-integers-evidence.json`, `docs/unsigned-integers.md`
- **Impact:** Both record 64 decoder checks while the assignment reports 80. Both also say the follow-up is pending.
- **Fix:** After the final rebuild, regenerate the evidence and hashes from the final tree. Don't reuse results from an earlier tree without confirming the input hashes still match.

## Questions to resolve

1. **Do file transfers reject unsigned values safely?** This is the required check before approval. `cc_read` and `cc_val` reject correctly, but `Read(F,W)` goes through `cc_file_transfer` in `compiler_files.c`, which wasn't supplied. The tests cover only console `Read`. Please confirm `Read(F,W)` and `Read(F,L)` on Text files reject, and test or reject `file of Word` and `file of LongWord` transfers. If any path accepts these with 16-bit handling, treat it as high severity.
2. **Can overlays reach the wide engine?** `engine()` emits the helper inline at first use and caches `cc.unsigned_entry`. If that first use is in an overlay, another group could call an address that isn't mapped. The supplied `cc_block` doesn't appear to parse `overlay`, but `compiler_bridges.c` was changed. Please confirm overlays can't reach this path in this build, or place the engine in resident code.
3. **Do large Word shift counts return zero?** The docs promise this, but tests stop at 16, and the `RT_SHL` and `RT_SHR` source wasn't supplied. Please test counts of 256 and 65535 to confirm the high byte of the count is honored.

## Review limitations

- **Missing files:** I didn't have the current `compiler_unsigned_internal.h`, `compiler_files.c`, `compiler_strings.c`, `compiler_groups.c`, `compiler_units.c`, the link code, or the runtime source.
- **Stale diff:** The attached diff predates the follow-up fixes, and its version of the internal header lacks functions the current code calls. I treated `current_files` as authoritative.
- **Multi-unit coverage:** The native tests use one authored unit. Canonical Word re-export through a second unit, and linking two units that each carry an engine, weren't tested end to end. I found no defect in the supplied code, but those paths depend on unsupplied link and dependency code.

### Codex reconciliation

Accepted finding 1: implicit unsigned-to-Char conversions now reject. Chr uses
an explicit checked conversion mode; native rejection and range-error tests have
been added. Accepted finding 3: Integer and Word scalar typed constants freeze
the validated value at declaration, preserving lexical binding. The existing
replay path remains for aggregates and LongWord source references. A nested
shadowing fixture now checks Integer and upper-half Word storage.

For finding 2, documented the deliberate modular negation of already unsigned
constants, rather than changing unsigned expression semantics. A negative signed
Integer initializer still rejects; -Big where Big is Word is modular Word
arithmetic. For finding 4, narrowed the cast claim to Integer/Byte/Word/LongWord/
Char; Boolean and enum casts remain unsupported. Finding 5 awaits the completed
native rebuild and refreshed hashes; no stale evidence is promoted.

Text Read(F,Word/LongWord) follows the existing Text-kind guard in
compiler_files.c and rejects with 66. Added both rejection cases, plus native
file-of-Word and file-of-LongWord write/read round trips. Typed file components
carry their byte width and are copied as records, without a 16-bit numeric
conversion. Memory overlays reject at compiler_routines.c:232; the native unit
build does not support disk overlay execution. RT_SHL/RT_SHR check H before L,
so counts >=256 return zero. Native Word tests now include 256 and 65535.

Host decoder checks pass after the conversion changes; expanded native tests
are rebuilding. Sources changed after this final Opus turn, so this is not a
review approval or finalized milestone. Further review requires the user's
request under the project skill's turn budget.

Checkpoint verification after fixes: production and all regression drivers fit
18 banks. Native unsigned1188, unit/Real1703, initialization623, hardware336 and
fragment934 pass; combined storage286 + allocation57 + cancellation54 pass on
one final harness run. Host reader65980 + loader45, decoder80, references1824,
Word-count229402, generated engine26004 and full sanitized host regression pass.

The Zeal VFS source audit supersedes the earlier retained-close assumption:
zos_vfs_close clears the descriptor entry after every filesystem close attempt.
The unit owner now clears its single active handle while preserving close errors.
This avoids retrying a stale descriptor or blocking later compiles. The exact
read-only kernel excerpt/hash is in reviews/zeal-vfs-close-reference.txt.

The SDK exec argv-array correction affects only test-demo arguments; the final
combined storage run uses the updated driver. Full proof and current hashes are
in docs/unit-storage-evidence.json. This is a tested local checkpoint, not an
Opus-finalized milestone.

Checkpoint verification after fixes: production and all regression drivers fit
18 banks. Native unsigned1188, unit/Real1703, initialization623, hardware336 and
fragment934 pass; combined storage286 + allocation57 + cancellation54 pass on
one final harness run. Host reader65980 + loader45, decoder80, references1824,
Word-count229402, generated engine26004 and full sanitized host regression pass.

The Zeal VFS source audit supersedes the earlier retained-close assumption:
zos_vfs_close clears the descriptor entry after every filesystem close attempt.
The unit owner now clears its single active handle while preserving close errors.
This avoids retrying a stale descriptor or blocking later compiles. The exact
read-only kernel excerpt/hash is in reviews/zeal-vfs-close-reference.txt.

The SDK exec argv-array correction affects only test-demo arguments; the final
combined storage run uses the updated driver. Full proof and current hashes are
in docs/unit-storage-evidence.json. This is a tested local checkpoint, not an
Opus-finalized milestone.
