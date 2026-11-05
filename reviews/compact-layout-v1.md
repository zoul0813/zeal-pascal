# Review: compact-layout-v1

## Opus turn 20261105T011551-1f6c3e44

Mode: code; read-only

# Review: compact-layout experiment

I found no blocker in the normal-runtime path. **Findings 1 and 2 should be resolved before this is used as proof.** Finding 1 is an established binding error, while finding 2 needs `format.asm` and `input.asm` to confirm its runtime impact.

I ran nothing. The test and hash results below are as reported in the assignment, and native execution is still pending.

## Established findings

### 1. High: Transfer assets in `zcompacttest` stay bound to the normal runtime
- **Location:** `CMakeLists.txt` (`transfer_call_data`, `foreach(target zcompacttest ...)`); `tools/package-native.py` (`transfer_call_data`, `validate_transfer_call`, `validate_transfer_assets`)
- **Evidence:**
  - `zcompacttest` links `transfer_test_asset.asm`.
  - That asset embeds `transfer_call.zpl`, which is generated from `pascal_runtime.profile` and from `input_cleanup` at `pascal_runtime.private[18:20]`.
  - `validate_transfer_assets()` does not receive `runtime_name`, so the `--compact-experiment` path cannot detect this mismatch.
- **Trigger and impact:** A compact-compiled program that uses Chain or Execute carries the normal runtime's profile and an `input_cleanup` address in the normal `$77CD–$7EA6` extent. The compact template has no code there, and that interval is now the proof's low-data area.
  - The likely result is a profile rejection or a jump through a stale address.
  - The exact failure depends on `src/transfer_call.asm`, which was not supplied.
  - No fixture covers this. Treating Chain as a separate release gate does not make this binding safe.
- **Fix:** Choose one of these:
  - Generate compact-specific transfer data and assets, and validate them against `runtime_name`.
  - Make Chain and Execute fail closed in compact builds, and add a native test for that failure.

### 2. High: Compact Real failure is proven only for `ZR_ADD`
- **Location:** `src/real.c` (`zr_eval`); `tests/native/compact-main.c`; `tools/test-compact.py`
- **Evidence:** The host loop proves only that `zr_eval` returns `zr_error=255` and `zr_assign=0`. The only native Real fixture is `x:=x+2.5`, which covers the add path through its generated caller.
- **Trigger and impact:** The full engine never returns FF for `ZR_FORMAT`, `ZR_VAL_REAL`, or `ZR_COMPARE`, so callers may not check `zr_error` on those paths. If they don't, the compact engine could produce plausible results:
  - **Val into Real:** `Val` could look like empty-input success, with `code=0` and no assignment.
  - **Real input:** `Read` of a Real could show the same failure pattern.
  - **Real output:** `Write(real)` could print a blank or stale field, because the compact path never resets `zr_formatted[0]`.
  - **Scope of the gap:** This would break the requirement that compact calls fail rather than fake results. Runtime behavior is unconfirmed because `format.asm`, `input.asm`, and `real_adapter.asm` were not supplied.
- **Fix:** Add native `checked_error` fixtures for:
  - Real output
  - `Val` into Real
  - `ReadLn` into Real
  - comparison
  - integer-to-Real conversion
  - `Trunc` or `Round`
  - `Sqrt`
  - one transcendental operation

  Add explicit error handling to any caller that ignores `zr_error`. A compile-time rejection of these operations would be a separate design choice.

### 3. Medium: `compact_runtime.profile` has no file-level dependencies
- **Location:** `CMakeLists.txt`, the custom command that outputs `compact_runtime.profile`
- **Evidence:** `DEPENDS runtime_profile compact_runtime_bin` names custom targets, which provide ordering only. The sole file dependency is `package-native.py`. The normal profile command lists every hashed input file.
- **Trigger and impact:** After a change to `compact_runtime.zpl`, `.private`, or any hashed module, the profile is not regenerated. `package()` should then reject the stale profile, so this fails closed. However, the incremental build stays broken until the profile is deleted.
- **Fix:** List the compact `.zpl` and `.private` files plus the same module, gate, and packed files as the normal command.

### 4. Medium: The compact validator does not enforce a complete `$6000` bound
- **Location:** `tools/package-native.py`, `validate_runtime(compact=True)`
- **Evidence:** The whole-image check is still `max(memory)>=0x8000`. The `limit` value applies only to `REAL_DATA` and the listed private symbols.
- **Trigger and impact:** Initialized bytes from an unlisted area can land in `$6000–$7FFF` and pass validation, overlapping the proof's data. Uninitialized `_DATA` or `_BSS` content can do the same without appearing in the HEX file. The current 6297-byte payload ends at `$5898`, but that doesn't prove all allocated storage is below `$6000`.
- **Fix:** In compact mode, require `max(memory)<limit`. Also bound every non-absolute `s__*`/`l__*` area, failing on any unexpected non-empty area.

### 5. Medium: The CRT fixture does not prove page 1 was replaced
- **Location:** `tests/native/compact-main.c`, `LOW-CRT-OK` fixture; `docs/compact-layout-experiment.md`
- **Evidence:** The fixture runs `clrscr`, `gotoxy`, `textcolor`, and cursor queries with stdout swapped to `H:/COMPACT.TXT`. Its only CRT text output comes after the final low-memory verification.
- **Trigger and impact:** If any of these operations skips the VRAM mapping, the byte comparison can still pass. The doc's claim that this "exercises actual page-1 VRAM replacement" is therefore not established. CRT text output, the most likely mapped write path, is also not covered between fill and verify.
- **Fix:** Add CRT character output before the verification loop, and assert a screen or VRAM effect from the host. Also confirm that the helpers map VRAM while stdout is swapped.

### 6. Low: `compact-main.c` lacks shared-header dependencies
- **Location:** `CMakeLists.txt`, `OBJECT_DEPENDS` for `tests/native/compact-main.c`
- **Evidence:** It lists only `harness.h` and `compiler.h`, while comparable mains also list `zp.h` and `library.h`. The file notes that this SDK disables depfiles.
- **Impact:** An ABI change in either header can leave a stale test object.
- **Fix:** Add both headers.

### 7. Low: Some report metrics are constants rather than measurements
- **Location:** `tools/test-compact.py`, report fields
- **Evidence:** These values are hard-coded:
  - `saved_prefix_bytes`
  - `execution_pages_released`
  - `automatic_global_or_heap_capacity_gain`
  - `explicit_low_storage_bytes_exercised`
- **Impact:** The experiment was asked to measure usable memory, but these fields look like observations.
- **Fix:** Derive them from artifacts such as saved COM layout and page counts, or label them clearly as contract constants.

### 8. Low: The stepped sweeps miss key integer boundaries
- **Location:** `tools/test-compact.py`, `integer_parity`
- **Evidence:** The step-31 and step-37 ranges miss ±32768, 32767, -32769, and `$FFFF`. Those values are covered only if `oracle.val_vectors()` includes them, and `tools/test-real.py` was not supplied.
- **Fix:** Add these tokens explicitly, along with lowercase and negative hexadecimal forms.

## Checked with no defect found
- **Default runtime:** The `real.c` conditional nesting leaves normal and compiler builds unchanged. Empty `package_mode` also leaves the normal packaging command unchanged.
- **Object filtering:** The filter removes only `runtime_asset`. If the object name differs, duplicate symbols or the bank 8 package check should fail the build.
- **Runtime metadata:** `package()` binds the compact template, private table, profile, and metadata together. Finding 1 is the exception.
- **Low-buffer rejection:** `readable()` rejects `$6000` before any transfer, so the empty `LOW.DAT` expectation matches `blocks()` and `file_runtime.c`.
- **Failed fixtures:** Pascal `halt` paths are caught because each marker must appear exactly twice. The script's final `assert` then fails the run, unless Python runs with `-O`.

## Questions and missing context
1. **`checked_error` and the count of 111:** Which execution kind does `checked_error` use for each fixture? From `harness-execute.c`, I can reach 111 only if both error fixtures use kind 3 and each of the seven fixtures adds one harness check. That is an inference, and a different kind for the file fixture would change the count. `harness.c` and `harness.h` are needed to confirm it.
2. **Stale host files:** Does `n.execute` start with a clean `hostfs`, and is `n.RUN` unique per run? If not, old `COMPACT.TXT`, `HIGH.DAT`, or `LOW.DAT` files could satisfy checks when execution stops early. `tools/test-native.py` is needed.
3. **Input observer:** Does every keyboard-observer path use the patched `n.input_layout`, or do some read normal-runtime symbols directly?
4. **Uninitialized page bytes:** Does the compiler clear the prefix page beyond `cc_runtime_size()`? If not, up to about 10 KiB of stale page content could enter saved COM files or uninitialized runtime data.
5. **Hard-coded addresses:** Do any unsupplied assembly or compiler files hard-code the normal extents, such as `$77CD` or `$7EA6`?

## Review limitations
I did not have `runtime.asm`, `real_adapter.asm`, `format.asm`, `input.asm`, `transfer_call.asm`, the harness sources, `test-native.py`, or `compiler_crt.c`. As a result, I could not verify the expected source positions, the assertion total, or CRT mapping. Native execution remains unverified.

Production low-memory file access, automatic allocation, units, and banking are out of scope and aren't counted as defects here.

### Codex reconciliation

Accepted 1: the compact target now substitutes proof-only compiler_compile and
compiler_chain objects alongside its runtime asset. ZP_COMPACT_EXPERIMENT rejects
Chain/Execute through COMPILE_PLATFORM_FAILURE and rejects COMPILE_CHAIN before
acquiring resources. Normal objects have the original branches. Three native
compile rejection cases plus cleanup assertions passed; no transfer coordinator
can be generated by those paths. Raw Inline/native calls are explicitly outside
the proof contract. No compact Chain/Execute ABI is claimed.

Accepted 2 as a coverage gap; the suspected unchecked-adapter defect was not
present. real_adapter.asm universally checks zr_error before returning results,
including formatting and Val. Added eight native error probes for output, Val,
comparison, integer conversion, Trunc, Sqrt, Ln, and physical Real input. Together
with Add and low-file failure they verify checked error kind/code/source location
in RAM and saved COM. All passed; assembly error handling needed no change.

Accepted 3: compact_runtime.profile now has explicit file-level dependencies for
its compact template/private table and every other hashed module/gate/packed
asset. Incremental rebuild and packaging passed with that rule.

Accepted 4: the compact whole-HEX limit is now $6000, and every nonempty
relocatable linker area must be a known bounded section. Three host adversarial
cases reject initialized data at $6000, uninitialized BSS there, and an unexpected
area below it without publishing replacement private metadata. Existing fixed
absolute header bytes are checked by the whole-HEX bound; transition scratch
symbols remain their explicitly reserved ABI addresses. Native low storage also
passes bytewise preservation.

Accepted/adapted 5: wrote Q through Con between low-array fill and verification,
checked the resulting cursor, and added an independent supervisor VRAM oracle
for its exact character/attribute cell and ClrScr's blank first cell. This adds
one assertion. The unchanged compiler_crt.c explicitly maps page1 for ClrScr
regardless of stdout redirection; its implementation is supplied in follow-up.
Native oracle and low-data marker passed. Output uses the actual video console,
while marker output remains redirected to the disposable capture file.

Accepted 6: added zp.h and library.h to compact-main.c OBJECT_DEPENDS.

Accepted 7: saved prefix and globals base now come from saved-image header fields.
Zero automatic allocator gain/page release are labeled unchanged-layout contract
constants; the 8192-byte extent is labeled the fixture contract and verified by
its bytewise loop and marker, not advertised as automatic allocatable capacity.
Peak compiler pages are explicitly unmeasured.

Finding 8 was already covered by the exact existing oracle.val_vectors() token
list (including signed bounds, overflow, uppercase/lowercase and negative hex).
Added an explicit coverage-set assertion to keep those tokens mandatory. The
follow-up supplies tools/test-real.py. Total differential cases remain 3959.

Resolved context questions: checked_error calls execute_loaded(kind=3); all
fixtures add one load check. Native completion requires exactly 249 passing
assertions, zero failed, authenticated supervisor header/stage/done, four exact
success markers twice, exact high-file bytes, empty rejected low file, and
before/after immutable-bank equality with the packaged library. test-native.py
uses a unique timestamp/PID directory and fresh per-run HostFS. All observer
paths use n.input_layout; the alternate template is verified against exact
assembled instruction patterns. compiler_compile.c clears the entire prefix page
beyond the payload in RAM and disk modes. Search found no hard-coded movable
$77CD/$7EA6 addresses in runtime/compiler assembly or C.

The first native attempt exposed invalid overlong source fixture lines and a
completion observer that missed an already-halted supervisor. Fixed the fixtures
to respect the existing 127-byte physical-line limit, and authenticate either
FC00 or FC01 with the actual supervisor header. The final test passed 249 checks;
normal sqrt/Real control passed 37. No stale output was accepted as success.

Verification: clean ZDE build plus incremental rebuild; unchanged production
runtime hash 1da9d420f5598640ed57fa37b099faf09c76fa60dbae31a212b1b341aadf53a3;
6297-byte compact payload versus 16191 normal; 249 native compact checks and
37 normal Real-control checks; 3959 integer parity cases; 59758 full Real host
vectors; three allocation guards; 63 normal packaging/no-publication checks;
Python syntax and diff whitespace checks. Evidence is supplied in
compact-layout-evidence.json with hashes and raw capture locations.

No production allocation, low-file access, banking, Chain composition or units
are claimed. First review changes.diff is empty (read-only). No unresolved
first-pass code finding remains; the follow-up must assess the revised sources.

## Opus turn 20261105T013110-b3e794be

Mode: code; read-only

# Follow-up review: compact-layout experiment

**There are no blockers for this milestone, and all eight first-pass findings are resolved.** The experiment supports the doc's limited conclusion: continue design work, but don't change the production layout yet. Six low-severity items remain, and none can produce a false pass.

I ran nothing. Test results and hashes are as reported in the assignment and `docs/compact-layout-evidence.json`.

## First-pass dispositions

| # | Status | Review result |
|---|---|---|
| 1. Transfer bindings | Resolved for the proof scope | Chain and Execute statements fail at `cc_transfer_statement`, including in suppressed code. CHN output fails before any resources are acquired. `head` stays zero, so these paths can't embed the normal coordinator. See item 5 for the remaining `cc_transfer_finish` question. |
| 2. Real failure paths | Resolved | No fake-result path exists in the supplied adapter. See the Real paths section below. |
| 3. Profile dependencies | Resolved | `DEPENDS` now lists all 10 files hashed by `runtime_profile`. |
| 4. Allocation bound | Resolved | The whole-HEX limit is `$6000`, and non-absolute linker areas are checked. See item 3 for two small test gaps. |
| 5. CRT proof | Resolved | See the framebuffer section below. |
| 6. Header dependencies | Resolved | `zp.h` and `library.h` are now in `OBJECT_DEPENDS`. |
| 7. Report metrics | Resolved | Offsets 132 and 24 match the `runtime.asm` header layout. Constants are now labeled as contract values. |
| 8. Integer boundaries | Resolved | The required tokens are in `val_vectors()` and are now asserted. |

## Areas reviewed

**Real adapter paths**
- `real_evaluate` checks `real_status` before any result reply, and format, Val, compare and scalar replies all come after that check.
- `input_number` and `val_field` both go through `val_dispatch`.
- The unchecked integer-promotion `_zr_eval` calls are harmless because the final operation is never 15, so it still returns FF.
- Compact `zr_eval` tests the operation before the length, so empty `Val` into Real also fails.

**Assertion count**
- I derived 249 from the source: 19 direct checks, 70 from five successful executions, and 160 from ten kind-3 errors.
- This also answers the earlier `checked_error` question: it always uses kind 3.

**Framebuffer proof**
- Cell `$4286` matches row 8, column 6, and `$5286` is its attribute.
- ClrScr is the only operation in the fixture that remaps page 1, and `write(Con,'Q')` uses the console driver.
- The proof still holds. ClrScr's writes would destroy the prefix without the remap, and the low-array check after it would fail if page 1 weren't restored.
- The final VRAM sample shows only the last state, so it can't separate the RAM and COM runs. Both runs use the same emitted helper, so I treat this as a proof limit.

**Completion observer**
- Stale results are excluded: each run uses a fresh directory, and `mkdir(exist_ok=False)` rejects reuse.
- An exception during input stops the remaining commands, so the final counters never appear and the check fails.
- Pascal `halt` is also caught natively because it reports kind 4, not 1.

**Object filtering and default runtime**
- The filter removes exactly three objects. A mismatch would produce duplicate symbols.
- Normal builds are unaffected by the new conditionals.

## Remaining low-severity items

1. **`tools/test-compact.py`: delivery checks are weaker than `test-native.py`**
   - **Trigger:** `execute()` catches exceptions, so a nonzero emulator exit at `quit` after a complete final frame is swallowed. The script doesn't read `input-error.json`.
   - **Fix:** Require that file to be absent, require four `input-lines.json` receipts, and compare the startup and final `c000` mappings as `CoreConsole` does.

2. **`CMakeLists.txt` and `docs/compact-layout-experiment.md`: stale claims**
   - **Evidence:** The CMake comment says "replacing only its immutable runtime bank", but two compiler objects are now replaced too. The doc says Chain behavior is unchanged, then says Chain is rejected.
   - **Fix:** State that the compact target uses a compact runtime bank and two fail-closed compiler objects. The reconciliation note also overstates coverage: exact source location is checked in RAM only, while the saved COM checks the exit status.

3. **`tools/package-native.py` and `allocation_guards`: validator test gaps**
   - **Evidence:** The area regex requires a leading `_`, so a hand-written `.area NAME` would escape it. The negative test also accepts any `ValueError`, so an unrelated failure could satisfy it.
   - **Fix:** Match all area names except `.ABS.`, and assert the expected error message for each trial.

4. **`tests/native/compact-main.c`, `physical_cell`: interrupts stay enabled**
   - **Trigger:** Page 1 is remapped without the `DI` and IFF restore that `crt_enter` uses. The window is three instructions, and the harmful outcome is a spurious failure or test-only misbehavior.
   - **Fix:** Disable interrupts around the read and restore the caller's state.

5. **`src/compiler_chain.c`, `cc_transfer_finish`: one path is not substituted**
   - **Question:** It can still embed the normal assets if `cc_root_pending(LATE_ROOT,LATE_TRANSFER)` is true. Whatever sets that flag isn't in the supplied files, and I found no reachable path.
   - **Fix:** Confirm no such path exists in the compact target, or fail there under `ZP_COMPACT_EXPERIMENT`.

6. **`CMakeLists.txt`, `compact_compile`: build-definition mismatch**
   - **Question:** `zp_core` defines `ZP_IDE_PATHS`, but `compact_compile` doesn't. The two substituted files don't visibly use it, and the target linked, but `zp.h` and `compiler.h` weren't supplied.
   - **Fix:** Confirm no shared declaration depends on it, or add the definition for parity.

## Intentional proof limits (not defects)
- Raw Inline and native code are outside the safety contract.
- The negative fixtures have no normal-runtime control. FF in namespace 2 is also the stack and heap error code, so these tests rely on the checked source position and the host engine check to identify the cause.
- Only ClrScr, GotoXY, TextColor, cursor queries and Con output were exercised. ClrEol, InsLine, DelLine and CrtExit were not.
- Physical hardware, compiler peak RAM and an allocator-based capacity measurement remain unproven.

## Blockers
**This milestone:** none.

**Before any production layout change** (out of scope here, and already recorded in the doc):
1. Define mapping-aware low-memory file transfers or staging.
2. Audit file control blocks, paths and numeric destinations.
3. Define symbolic placement and allocator ownership.
4. Define a compact Chain and Execute ABI, including shared-global compatibility.
5. Define how a program selects the runtime it needs, since nothing does this automatically today.

## Review limitations
`compiler_late.c`, `compiler_groups.c`, `compiler_real.c`, `machine.asm`, `zp.h` and `compiler.h` weren't supplied. I also couldn't compare the hashes in the evidence file with the supplied bytes, and I didn't inspect the raw captures.

### Codex reconciliation

Accepted remaining items 1–6 and implemented the recommended hardening. The
exact final source changes are after the second read-only turn; no third Opus
turn was requested. This is not an Opus verdict on the exact final inputs, and
runner completion must not be claimed or reused.

1. Native delivery now requires no input-error.json, exactly four expected
physical-line receipts in the right modes, and equal startup/final supervisor
page3 mappings. The final emulator run passes those additional host checks.
2. Corrected the CMake comment to identify all three object substitutions and
clarified that only ordinary targets preserve Chain behavior. The experiment
report now distinguishes exact RAM error-source navigation from saved exit-status
checks. Historical first-pass reconciliation describes the earlier supplied state;
these corrections govern the final claim.
3. The allocation-area matcher now accepts all non-whitespace area names, not
only underscores. Added an unprefixed PLAIN area case and require the exact
expected ValueError text in each negative trial. Four guards pass; no replacement
private metadata is published on failure.
4. The independent VRAM read now saves IFF2 with LD A,I, disables interrupts,
restores page1, and conditionally restores EI before returning the byte. The
native framebuffer and low-data proof passes with this hardened observer.
5. cc_transfer_finish also fails closed under ZP_COMPACT_EXPERIMENT if either an
ordinary transfer head or deferred transfer root is present. It cannot emit the
normal coordinator through that path. Ordinary compiler code is unchanged.
6. Added ZP_IDE_PATHS for compact_compile definition parity. The current
substituted sources do not use path.h directly, but matching the owner definitions
avoids a latent declaration mismatch if includes change.

Final verification after these changes: ZDE rebuild succeeds; compact run passes
249 native assertions, zero failures, with all nine host delivery/identity/output/
file/bank/input-hash checks true. Normal sqrt/Real control passes 37 assertions,
zero failures. Integer differential cases3959 and allocation guards4 pass in the
same final compact run; the normal packaging/no-publication suite passes63.
The full Real host suite passes59758 vectors; real.c is unchanged by the final
hardening. Production runtime SHA-256 remains the exact pre-experiment value.
All final native report source/artifact hashes and local links were verified.
Both Opus changes.diff files are empty as expected for read-only review.

No unresolved code finding remains for this limited experiment. Exact-final-text
Opus review/finalization remains outstanding because supplied inputs changed
after the last permitted turn; any extra review needs the user's request under
the project workflow. No further approval was inferred or requested. Results
remain experimental and do not authorize production layout changes, low-memory
file buffers, allocator changes, Chain composition, banking or units.

The final VRAM sample observes the last state; it does not separately sample the
RAM and COM screens. Both executions independently preserve every low-array byte
and their markers; the same generated helper is used. Full hardware, compiler
peak RAM, and automatic capacity remain unproved as documented.
