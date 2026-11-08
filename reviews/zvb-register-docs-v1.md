# Review: zvb-register-docs-v1

## Opus turn 20261108T060021-aa9e8bcf

Mode: code; read-only

# Review: zvb-register-docs-v1, against f084eca

I found no blockers. There are three medium findings and five low findings. No test was run by me; every pass/fail statement below comes from the assignment text.

## Established defects and risks

**1. Medium: the statement bank may be missing after a failed reload (`src/library_phase.c`, `reloadable()` and `code_phase_unit_finish_begin`/`_end`)**
- **Trigger:** `code_phase_unit_finish_end()` (or the failure branch's `restore(...)`) fails to reload `CODE_STATEMENTS`. Causes include `palloc`/seek/read failure or a later `pfree` error. The bank is then left at `code_pages[CODE_STATEMENTS]==255`.
- **Impact:** before this change, `CODE_STATEMENTS` was eagerly loaded and never dropped. Code in other banks may therefore call statement-bank functions without checking residency first. Candidates are `CODE_IDE`, `CODE_EDITOR`, `CODE_COMPILE` and `CODE_UNITS`, for example the next Run/compile or error-location recompilation. After a failed reload, such a call jumps into an unmapped or reused page.
- **Evidence:**
  - `tools/check-unit-banks.py` proves only that nothing calls lent banks during the lending interval.
  - The host test proves that `code_phase_restore()` can bring the bank back.
  - Neither proves that every later entry into `CODE_STATEMENTS` is dominated by a successful restore.
  - The `library.h` comment ("No phase page is executable after a failed reload… the next compile retries it") was written for deferred banks, which callers already treat as optional.
- **Fix or verification:** extend the call-graph checker with a reverse closure of all callers of `CODE_STATEMENTS` functions outside the bank itself. Show that each such caller is reachable only after a checked `code_phase_restore()` or an equivalent call. Alternatively, have the compile entry fail closed when `code_pages[CODE_STATEMENTS]==255`. Add a host case: fail `finish_end` at the statements reload, then show the next compile path restores the bank before parsing.

**2. Medium: the register guide omits interrupt atomicity for the shared bank selector (`docs/zvb-registers.md`, "Non-banked and banked registers"; same text in `tools/generate-zvb-hardware.py`)**
- **Trigger:** a user follows the raw `SavedBank := Port[ZVB_CONFIG_DEV_IDX]; … Port[ZVB_CONFIG_DEV_IDX] := SavedBank;` example with interrupts enabled.
- **Impact:** the project's own code treats the selector as interrupt-sensitive:
  - CRT's `crt_enter` performs `LD A,I; PUSH AF; DI` before saving or setting `$8E`.
  - ZVB routines use "atomic peripheral I/O" Inline sequences.

  If an OS interrupt handler touches `$8E` or the text peripheral (keyboard echo, cursor), the documented raw sequence can read or write the wrong device, or the handler can see the CRC bank selected. "Coordinate access when … interrupt handler shares" does not tell users that the OS itself may be such a sharer.
- **Fix:** state explicitly whether supported Zeal OS interrupt handlers read or select `$8E` and the `$A0..$AF` ports.
  - If they do, show the Inline DI/restore pattern, or recommend high-level routines for multi-step banked access.
  - If they do not, say so and cite the evidence, since CRT's DI then needs a different justification.

**3. Medium (verify): new test and evidence files may be ignored by git (`.gitignore`)**
- **Context:** the file is a default-deny whitelist. The new entries cover `ZVBHW.INC`, the tools, the docs, `unit-reference-evidence.json` and the review file. They do not list:
  - `tests/native/zvb/HWVALUES.INC`
  - `tests/native/crt-guide.pas`
- **Trigger and impact:** if earlier whitelist patterns only cover `*.PAS` under `tests/native/zvb` (or no `tests/native/*.pas` pattern exists), a fresh checkout lacks these files. `tools/test-zvb.py --case SMOKE` would then raise on `read_bytes()` of `HWVALUES.INC`, and the CRT guide check could not run.
- **Missing context:** `docs/unit-reference-evidence.json` is whitelisted but was not supplied, so its contents could not be reviewed.
- **Verify:** run `git check-ignore -v tests/native/zvb/HWVALUES.INC tests/native/crt-guide.pas` and add whitelist entries if needed.

**4. Low: the guide does not explain register overlap inside the sound bank (`docs/zvb-registers.md`, sound table)**
- `ZVB_PERI_SOUND_SAMPLE_FIFO/DIV/CONF` share `$A0..$A2` with `FREQ_LOW/FREQ_HIGH/WAVE` inside bank 3.
- The guide's statement that "identical numeric addresses are intentional" explains only cross-bank overlap.
- Readers need to know that `ZVB_PERI_SOUND_SELECT` chooses voice registers versus sample registers. This matches the existing `SoundSampleTable = 128` mask.
- **Fix:** add one sentence to the generator template naming the select value for sample access.

**5. Low: the CRT guide's claim that "documentation examples" are tested is broader than the supplied evidence (`docs/crt-library.md`, verification paragraph)**
- `tests/native/crt-guide.pas` exercises the routines. It does not contain:
  - the `HelloScreen` listing;
  - the `{$I-}` `WhereX`/`IOResult` example.
- The register-guide snippets (CRC bank read, `InVBlank`, `Byte(ScrollY)` / `ScrollY shr 8`) are not shown compiled anywhere either.
- **Fix:** compile these snippets, extracted from the docs as `first_bar_source()` does for FirstBar, or narrow the wording.

**6. Low: the "named equals literal" output check mixes in uses order (`tools/test-zvb.py`, check `named ports match literal ports with no output overhead`)**
- `USES.PAS` (`uses ZVB, Crt`, literal ports) is compared with `ORDER.PAS` (`uses Crt, ZVB`, named ports).
- If uses order ever affects the image, the check fails for an unrelated reason. Equality also proves two properties at once, so a failure could not be attributed.
- **Fix:** compare a named/literal pair that has the same uses clause, or document that order-independence of the output is part of the asserted property.

## Questions and qualifications

**7. Low: source for the latch and commit claims (`docs/zvb-registers.md`)**
- The guide states that a low-byte raster read latches the high byte, and that the high-byte write commits scroll and sound-frequency pairs.
- It also says the SDK header "describes those permissions".
- Please cite the RTL or header lines (for example `reviews/zvb-rtl-reference.md`). The only current native test is a scroll readback in `GFX.PAS`, whose result is still pending.
- If scroll registers prove not to be readable on the RTL, the `GFX`/`ORDER` save-and-restore pattern and the docs both need adjusting.

**8. Question: interface growth for clients**
- 133 additional exported identifiers enlarge every `uses ZVB` client's symbol and interface import.
- Please confirm in the follow-up that Snake (Com-file mode), FirstBar and SPRITES compile with unchanged headroom. Snake's Memory-mode limitation is already marginal.

**Wording nit:** in `docs/crt-library.md` (Colors), "cursor's foreground/background nibbles are inverted" describes a nibble swap (four rotates of A). "Swapped" avoids a bitwise-NOT reading.

## Areas checked with no findings

- **Lexical compactor (`tools/zvb-source.py`):**
  - Quoted strings are emitted verbatim, including the doubled `''` escape.
  - Comment delimiters inside strings are respected, and apostrophes inside comments are skipped.
  - `{$…}` and `(*$…*)` directives are preserved; `{ $…}` is correctly treated as a comment.
  - SPDX notices are kept.
  - A multi-line comment collapsing to a single `' '` can join lines, but this is token-safe and covered by the 127-character guard.
- **Generator and value checks:** uppercase-key collisions are asserted, and function-like macros are skipped by the regex. The C cross-check is independent of the Python evaluator.
- **Counts:** 64 ports = 5 + 14 + 10 + 12 + 6 + 12 + 5, plus 69 other constants, totals 133.
- **Record sizes:** the 5, 11 and 12 byte records match the field types.
- **CRT routine table versus `screen-engine.inc`:** the following all match the documentation:
  - `WhereX` returning 81 on pending wrap;
  - colors 15/7;
  - the 0x38 control mask;
  - mode-0-only checks;
  - `Delay` skipping the mode check;
  - `ClrScr` resetting scrolling.
- **Phase lending:** `park()` still drops only `CODE_DEFERRED` banks. `bind()` and `restore()` handle the newly reloadable bank consistently, and the `drops[:4]` checker update matches the emitted order.

## Review limitations

- I did not see:
  - the SDK headers;
  - `ZVBIO`/`ZVBGFX`/`ZVBPACK`/`ZVBDECODE` sources;
  - `package-screen.py`;
  - the CRT guide harness;
  - `unit-reference-evidence.json`;
  - the callers of `CODE_STATEMENTS` code.
- Native USES/ORDER/SMOKE/GFX/FirstBar/Snake results are pending.
- Physical-board behavior is unqualified.

### Codex reconciliation

1. Accepted. Tracing the reverse call graph found two genuine non-parser entry paths: IDE option bounds and diagnostic cleanup, plus public saved-image validation in the same bank. Moved bounds and source-error services into the existing retained compiler_sources support area; moved image_compiler entry+near decoder together into retained BANK11. No logic change. Compiled checker now walks all 79 reverse parser ancestors and permits entry only through compiler_compile_project; resident compile_project, compile_request and compile_unit check code_phase_restore first (banks.c supplied for follow-up). Retained service placements are asserted. Host phase test now fails every finish_end reload boundary and retries the same warm-set restore used by compiler entries: 5,528 sanitized assertions across 7 begin and 13 end failure boundaries. ZDE build and compiled residency proof pass.
2. Accepted with conservative guidance. The generated guide now explicitly says save/restore is not interrupt atomicity, states the raw example's assumption, recommends high-level atomic helpers for multi-step bank access, and explains preserving incoming IFF rather than blind EI. It makes no universal assumption about OS/handler selector use and adds no new raw API.
3. Verified, no ignore fix required for tests. git check-ignore -v reports neither test file ignored; both appear as untracked nonignored files in git status. /tests/native/ is wholly permitted by the existing parent whitelist. New generated/docs/tool files have explicit allow entries. Final evidence is supplied in follow-up.
4. Accepted. Sound selection and same-bank A0..A2 aliases now documented, with sample-only $80 selection.
5. Accepted by narrowing the CRT claim: it now states that a fixture exercises all 16 documented routines, rather than claiming every prose snippet ran. FirstBar is extracted verbatim by the existing generator and passed. The CRT fixture and physical ReadKey tests passed; register import and scroll readback tests passed. No claim that every illustrative fragment has its own execution test.
6. Adapted. The equality check now explicitly asserts both named/literal equivalence and reversed uses-order output independence. Both properties are intentional; report label names both. Exact COM equality passed (6,678 bytes each).
7. Accepted. The generated guide cites the pinned public header and line ranges for read permissions/latches/commit ordering. Full numbered header and hashes are supplied in read-only review context. GFX successfully read back $0123 using named registers and restored scrolling.
8. Addressed with native evidence. Expanded-interface Snake and FirstBar pass RAM/saved checks as applicable and detached execution; Snake remains exactly 35,844 bytes with unchanged SHA256 8d1ae8b2ddc79a1299dd14080746f7f79b4fb47faa2bfe52e709666d051b5942. Stack margin 8,367 bytes reported by both focused client runs. The final relocated-helper Unit-initialization/Real/Chain regression passed 643 assertions (capture 20261108T061012.956032Z-63581, existing local emulator). SPRITES is still running on the host-installed emulator; Codex will require it to pass before finalization and record its result below. The guide continues to disclose Snake Memory-mode source-allocation failure.
Wording nit accepted: cursor nibbles are described as swapped.

Verification so far: register clients 117 native harness assertions, Snake/FirstBar 65, CRT guide 33, all passed with saved source/Unit independence. No outstanding source edits are planned; SPRITES and follow-up review remain. Documentation link count is now 23 after evidence links. Existing framebuffer harness overflow and physical-board qualification limits are explicit in evidence.

## Opus turn 20261108T061437-ff5eb88c

Mode: code; read-only

# Follow-up review: zvb-register-docs-v1, against f084eca (final turn)

The recovery fix for finding 1 is correct for the paths it covers, but its compiled proof has a blind spot. The native client evidence also appears to predate the reconciliation edits. Finalization stays conditional on SPRITES. I ran nothing; every pass/fail statement below is taken from the supplied evidence.

## Disposition of prior findings

**1. Statement bank after a failed reload: partially resolved; one verification gap remains (finding A).**

What is now established by the supplied code:
- `banks.c` guards all three resident compiler entries with `code_phase_restore()`. `compile_project`, `compile_request` and `compile_unit` each fail closed with `COMPILE_PLATFORM_FAILURE` before selecting a compiler bank.
- The two real non-parser consumers of BANK2 have moved to retained BANK11:
  - `compile_limits_at`, `compile_limits` and `cc_sources_error`, into `compiler_sources.c`;
  - `image_compiler`, the saved-image entry and near decoder, in CMake.
- `check-unit-banks.py` walks every reverse edge from BANK2 functions. Any chain that reaches a root other than `compiler_compile_project` (for example `main`, or an assembly or indirect-only caller) fails. That is a sound, conservative proof for IDE, build-check and saved-image paths.
- The host test now fails each of the 13 `finish_end` reload boundaries. It then shows that the next warm-set restore recovers the full set and the inventory stays consistent.
- `code_phase_park`/`return` cannot resurrect or require the bank, because `resume_mask` only contains banks that were valid at park time.

This is ownership-based recovery, not an assumption that the reload succeeds. The remaining gap is described in finding A.

**2. Interrupt atomicity for the bank selector: resolved.** The guide states the raw example's assumption and says save/restore is not atomicity. It describes the IFF-preserving DI pattern without blind `EI`, and makes no unsupported claim about what the OS does.

**3. Possibly ignored test files: resolved.** Accepted on the stated `git check-ignore` result; the parent `/tests/native/` whitelist covers both files.

**4. Sound-bank overlap: resolved.** The text matches header lines 289–313 and 354–358: the sample voice is bit 7 / `$80`, and SELECT is a voice bitmap.

**5. CRT example-coverage claim: resolved.** The narrowed claim matches `crt-guide.pas` and the 33-assertion evidence.

**6. Named-vs-literal check mixing in uses order: resolved.** Both properties are now named in the check label, and the evidence shows two 6,678-byte COMs that are identical.

**7. Latch and commit sources: resolved.** The cited ranges match the supplied numbered header:
- lines 72–78: raster values latched on LSB read;
- lines 80–98: scroll registers are R/W and latched on MSB write;
- lines 265–266: sound frequency taken on high-byte write.

**8. Interface growth for clients: resolved for Snake and FirstBar.** SPRITES is still pending (see finding C).

## New and residual findings

**A. Medium: the guard proof stops at `compiler_compile_project` but does not check that function's own tail (`tools/check-unit-banks.py`, parser reverse walk).**
- **Trigger:** within a unit compile, `code_phase_unit_finish_end()` fails to reload BANK2. The failure can come from inside `cc_units_write`, or from the `failed:` epilogue's two retry calls.
- **What then runs without BANK2:** `compiler_compile_project` continues in BANK14 through `cc_units_close`, `build_discard_image`, `compiler_capture_failure`/`cc_sources_error` and `build_close`. It then returns into `compiler_compile_unit` (BANK16).
- **Why the checker misses it:** the reverse walk deliberately stops when it reaches `compiler_compile_project`. If `compiler_capture_failure` or any other epilogue callee (transitively) called a BANK2 function, the walk would hit the stop node and pass silently.
- **Second blind spot:** the forward closure explicitly excludes sites after `finish_end` in the roots ("Activation placement after end is allowed to use its restored bank"). Those sites run even when `finish_end` returned 0, unless the source checks its result.
- **Assessment:** moving `cc_sources_error` suggests Codex audited this tail by hand. The proof does not enforce it, so a future edit could regress it silently.
- **Fix:** add a forward closure over:
  - the call sites in `compiler_compile_project` after its first `finish_begin` and in `failed:`;
  - the post-`finish_end` sites of `cc_place_code`, `cc_units_link` and `cc_units_write`.

  Assert that no callee lies in any lent bank (2, 6, 10 or 15), or show that each post-end site is gated on a successful `finish_end`. Alternatively, supply `compiler_job.c` and the root bodies so that gating can be confirmed by reading.

**B. Medium: most native evidence predates the reconciliation code moves (`docs/unit-reference-evidence.json`).**
- The capture timestamps all fall before the first review run (06:00:21), whose diff still had `image_compiler` in BANK2:
  - CRT fixture: 05:46:38;
  - authoring: 05:52:32;
  - register clients: 05:57:45;
  - ReadKey: 05:58:42;
  - demos: 05:59:08.
- The moves of `image_compiler`, `compile_limits` and `cc_sources_error` were made afterwards. Only the Unit-init/Real/Chain run (06:10:12) postdates them.
- `zzvbtest` links `image_compiler` and `unit_runtime_core`, so the client, demo, CRT and ReadKey passes, including the saved-image checks, were obtained with a different compiler bank layout. The final `zpascal.zpl` hash in the evidence belongs to the new build; there is no corresponding `zzvbtest` hash.
- **Fix:** rerun the USES/ORDER/SMOKE/GFX, FIRSTBAR/SNAKE, CRT-fixture and ReadKey commands on the final build together with SPRITES. Record the `zzvbtest.bin`/`.zpl` hashes in the evidence. Otherwise, state explicitly that these results are pre-reconciliation.

**C. Low: the evidence file reports overall success while SPRITES is pending.** Its `status` says "qualification passed", but it has no SPRITES entry. Add the SPRITES result (with its capture directory) before finalization, consistent with Codex's own gate.

**D. Low: the emulator binary differs between captures.** The Unit-init run used `/Users/.../Zeal-NativeEmulator/build/zeal-native`; the others used `/opt/homebrew/bin/zeal-native`. Both are recorded, which is good. If B is rerun, use one emulator, or note that the difference is intentional.

## Checked with no findings

- `compiler.c` lost only the moved functions. `compiler_sources.c` now includes `compiler_layout.h`, presumably for `cc_required_stack`/`GLOBAL_BASE`. `test-staging-limits.py` now compiles the new home of these functions.
- `ZP_STATIC_LAYOUT` is not visible in CMake, but the checker observes all four drops in emitted assembly, so the lending is confirmed active in the product build.
- The generated register guide matches the header's register ranges, R/W/WO notes and values.
- The CRT documentation still matches `screen-engine.inc`, and the nibble wording is fixed.

## Limitations

- I have not seen `compiler_job.c` (`compiler_capture_failure`), `compiler_units*.c`, the root function bodies or `main.c`. Finding A therefore remains a proof gap, not a demonstrated defect.
- SPRITES is unfinished.
- Physical-board behavior and a full framebuffer rerun remain unqualified, as disclosed.

### Codex reconciliation

A. Verified current correctness; no production/source edit after the follow-up. Independently walked the final compiled compiler_compile_project failure epilogue, starting at its emitted `failed:` label, through every direct/SDK-banked callee: **49 callees**, banks 0/3/4/5/7/9/11/13/14/16, no lent banks 2/6/10/15 and no unresolved/indirect call. This includes compiler_capture_failure, cc_sources_error, cc_units_close, cc_tables_end, build_discard_image and build_close. Final product ZPL SHA256 is b06502ee54a0cd672ffad253f9ee2b90593633ab3afd369e6c38865f702cd490. Also read the success gates in the actual root bodies: compiler_units_names.c:137–140 and217–218 return0 immediately on failed finish_end; compiler_build.c:124 returns0 before all post-end placement calls. compiler_compile.c short-circuits to failed when cc_units_write/cc_units_link/cc_place_code return0. compiler_compile_unit's return tail only clears requested/output_descriptor. Thus no unguarded parser call occurs on failed restoration in the current code. The permanent checker does not automatically model all intraprocedural branch dominance; this general automation is deferred, with the current failure-tail proof and manual gates recorded here. This is a future test-hardening opportunity, not an unresolved current code defect.

B. Accepted evidence provenance correction. **Authoring, USES/ORDER/SMOKE/GFX and Snake/FirstBar captures in unit-reference-evidence.json predate the helper/decoder bank moves. They qualify the unchanged public interface, generator, constant values and output equivalence, not the final bank layout.** They were not all rerun. Final-layout qualification includes native Unit initialization/Real/Chain643, SPRITES32, and fresh CRT fixture33 plus physical ReadKey2-injections (both RAM and saved execution). The source moves changed compiler bank ownership, not Pascal generated runtime code; final-layout saved-image use is exercised by those fresh tests. Host sanitized ownership/reload, moved placement-bound helper tests and compiled graph checks also ran after the moves. This explicitly uses the review's alternative of labelling prior evidence rather than implying all captures used the final image.

C. Resolved. Final SPRITES native run passed32 harness assertions on /opt/homebrew/bin/zeal-native, capture analysis/native-slice/20261108T061058.910009Z-64872; 26,457-byte COM, stack margin8,367, saved execution with all PAS/INC/ZPU files removed. The reviewed evidence JSON is a checkpoint; these final supplements are recorded here without rewriting reviewed inputs.

D. Intentional and disclosed. Unit-init used the existing local emulator. All final ZVB client and CRT/ReadKey supplements used the user's host-installed /opt/homebrew/bin/zeal-native. No emulator source was changed.

Final CRT fixture passed33 assertions in analysis/native-slice/20261108T061639.797757Z-70664, and final ReadKey observer injected exactly2 physical key presses in analysis/native-slice/20261108T061641.034749Z-70890. No code or documentation source changed after the final Opus turn. No known current correctness blocker remains. Physical-board qualification and existing full-framebuffer harness packaging overflow remain disclosed limitations.

Final reviewed native harness artifacts:
- zzvbtest.bin: 16188 bytes, SHA256 fdd19cb8947cf55ba37427524ad0ecdc86c0cf9005982511babed428cc43a922
- zzvbtest.zpl: 256891 bytes, SHA256 2ff84280aeb97b5fa086a1a18c087f928d67554d18306204b59bf951874dcf8c
- zunitinittest.bin: 16328 bytes, SHA256 5388c5c44bf4c2b719271666cade36f5eb7e686e9e8c8b8947d4c21b90d9e111
- zunitinittest.zpl: 255292 bytes, SHA256 5a6118cb9bfc59b4aae584ac85719f88ab758f81b6c1f005d1e79267e1019882

Supplemental compiled failure-tail result:

```json
{
  "passed": true,
  "failure_tail_callees": 49,
  "callee_banks": [
    0,
    3,
    4,
    5,
    7,
    9,
    11,
    13,
    14,
    16
  ],
  "roots": [
    "cc_units_close",
    "page_drop",
    "page_drop",
    "build_discard_image",
    "cc_tables_end",
    "code_phase_unit_finish_end",
    "compiler_capture_failure",
    "cc_sources_error",
    "build_close",
    "code_phase_unit_finish_end"
  ],
  "lent_banks": [
    2,
    6,
    10,
    15
  ],
  "compiler_library_sha256": "b06502ee54a0cd672ffad253f9ee2b90593633ab3afd369e6c38865f702cd490",
  "manual_success_gates": {
    "cc_units_write": "compiler_units_names.c:137-140 returns 0 after failed finish_end",
    "cc_units_link": "compiler_units_names.c:217-218 returns 0 after failed finish_end",
    "cc_place_code": "compiler_build.c:124 returns 0 before post-end placement",
    "compiler_compile_project": "compiler_compile.c:141 and149 short-circuit to failed on !cc_units_write or !cc_units_link;174 does so on !cc_place_code"
  }
}
```

