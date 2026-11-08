# Review: zvb-decoder-v1

## Opus turn 20261108T044833-4a3e6963

Mode: code; read-only

# Review of zvb-decoder-v1: RAM-only RLE/LZ Inline decoder (baseline f1d1e20)

**Verdict:** I found no blockers and no established defects in the decoder. I hand-decoded all 383 bytes against the generator. The opcodes, the 35 relocated internal words, the state offsets, the budget/history invariants and the chunk-resume logic all check out. The findings below are test-coverage gaps, guards against future drift, and questions to settle during native acceptance.

## What I verified (by hand; nothing executed)

**Opcodes**
- Every IY-indexed form is correct: loads (`7E`/`4E`/`46`/`6E`/`66`), stores (`77`/`71`/`70`/`75`/`74`), and `36 d n`, `BE`, `96`, `35`, `34`.
- The remaining opcodes are also correct: `ED42` (`sbc hl,bc`), `0F` ×4, `C6`, `E6`, `37`, `D2`/`DA`/`CA`/`C2`/`C3`/`CD`, `21 00 01`.

**Relocation**
- I recomputed every label position from `generate-zvb-decoder.py`:

  | Label | Offset | Label | Offset |
  |---|---|---|---|
  | token | 24 | emit | 261 |
  | long_history | 85 | emit_literal | 280 |
  | history_check | 108 | emit_history | 289 |
  | single | 129 | store | 300 |
  | lz_literal | 143 | chunk | 327 |
  | rle | 148 | invalid | 334 |
  | literal | 176 | done | 338 |
  | budget | 200 | source | 345 |
  | full_budget | 227 | source_available | 358 |
  | store_budget | 234 | finish | 381 |
  | known | 255 | | |

- All 35 `*±n` words resolve to their intended label. Each delta is relative to the word's own position. This matches both `cc.emitted` in `compiler_inline.c` and the test's `0x8000+len(data)+delta`.
- `*` sets `delta.code=1` and forces a word. `*-301` parses as `*`, then subtract, then 301, which is within the decimal limit.

**Frame and state layout**
- `IX+6` holds State and `IX+4` holds Pixels, consistent with the existing leaves (last parameter at `IX+4`).
- The state offsets 0–18 match the record, assuming packed fields and 1-byte Booleans.
- IY is pushed and popped once. Stack depth is 4 bytes at most. IX is untouched.

**Token semantics**
- **Budget:**
  - `Full`/`Remaining=0` wraps to `65536-count` and clears `Full`.
  - When not full, carry means invalid.
- **Known:** clamped through H≠0 to 256.
- **History:** the offset is checked against `Known` *before* this token's count is added. That is correct for overlapping copies.
- **Offset 256:** the low byte is 0, so the copy reads the not-yet-overwritten ring slot. Correct.
- **Literal truncation:** high byte of `left` nonzero, else compare low byte with the count. Correct for counts up to 128.
- **RLE value / LZ long offset at EOF:** both go to `invalid`. Correct.

**Resume and chunks**
- `inc (iy+6)` sets Z at wrap. `chunk` then sets `Position=256` while `Count` stays pending.
- On re-entry with `Count≠0`, the decoder skips validation and budget, which were already reserved.
- `LoadEncoded` zeroes `Position` before each call.

**Errors**
- `invalid` sets `Error=2` and `Done`.
- `LoadEncoded` skips the partial flush on error. Earlier full chunks remain visible. This matches the old semantics.

**Read bounds**
- Source reads happen only while `left>0`.
- A source ending at `FFFF` wraps HL to 0 without dereferencing it.
- Ring writes use H=0 plus DE, so they cannot leave the 256-byte buffer.
- History reads cannot touch uninitialized `Pixels` because of the `Known` check.

## Findings

### 1. Medium: chunk-straddling tokens lack deterministic coverage
- **Where:** `tests/host/zvb-leaves.c`, `decoder_tests`.
- **Trigger:**
  - The 64 KiB RLE test (runs of 128) and its overflow variant are aligned so every chunk ends exactly at a token boundary.
  - The 264-byte LZ case totals 390 bytes and also chunks only at a token end.
  - Mid-token resume is therefore exercised only by random RLE streams (run kinds 0 and 1).
  - Resuming a pending history copy (run kind 2) after a chunk is effectively untested. Random 1–128-byte LZ streams almost always fail on the first history token, long before 256 bytes are produced.
- **Impact:** the resume paths for `emit_history` and `emit_literal` across a chunk depend on chance coverage. This includes `Position-Offset` wrapping through index 0 after a resume, with offsets 1, 5 and 256.
- **Fix (required before claiming "chunk-straddling tokens retain old semantics"):** add deterministic cases:
  - An LZ long-history token, e.g. count 67 starting at position 220, straddling 256, with offsets 1, 17 and 256.
  - An RLE literal of 128 starting at position 200.
  - An LZ literal straddling a chunk with exactly enough source left.
  - Then print or count how many random cases produced more than 256 bytes per kind.

### 2. Medium: no guard ties generator offsets to the Pascal record and frame layout
- **Where:** `generate-zvb-decoder.py` (comment-only offset table), `TEncodedState`, and the hard-coded `0xc2xx` offsets in the test.
- **Trigger:** any change to field order or types, or a compiler packing or Boolean-size difference. The oracle cannot catch this because it uses the same hard-coded offsets.
- **Fix:** in the native acceptance program, assert `SizeOf(TEncodedState)=19` and the `Error`/`Full` offsets (via `Addr` differences). Alternatively, generate the record text and the test offsets from one table in the generator.

### 3. Low: source-size margin is about 19 bytes and unguarded
- **Where:** `tools/zvb-source.py`, `source()`.
- **Trigger:** the expanded source is 32,749 bytes. If the two-page limit is 32,768, a one-line comment edit would silently break native authoring.
- **Fix:** assert the actual limit in `source()`.
- **Question:** please confirm the exact two-page limit.

### 4. Low: no drift check between the generator and `ZVBDECODE.INC`
- **Trigger:** a hand edit to the `.INC`, or a generator edit without regenerating.
- **Fix:** add a `--check` mode to the generator that compares its output with the file, and run it with the leaf tests.

### 5. Low: oracle weaknesses to close through native evidence
1. **Single origin.** Relocation is resolved only at `0x8000`, by the test's own resolver, not by `compiler_inline.c` or the unit linker.
   - **Fix:** in native acceptance, extract the decoder bytes from the built COM. Then confirm they equal the oracle stream after relocation at the actual load address, as the profiler already does for `VRAMTransfer`.
   - **Question:** in `ZP_UNITS_EXPERIMENT` unit mode, is `cc.emitted` unit-relative and covered by `cc_relocate_value`? Please confirm with a Unit build.
2. **No interrupts fired.** The irq variants only set IFF; no interrupt is ever injected. IY preservation by Zeal OS interrupt handlers is assumed.
   - **Question:** please confirm the Zeal OS ISR saves IY, or inject an interrupt in the oracle.

### 6. Low: profiler discovery heuristic is fragile
- **Where:** `tools/profile-snake-startup.py`, `Discover`.
- **Trigger:** discovery identifies the LZ entry by walking saved-IX links two frames up from any `VRAMTransfer` with page ≥ 68 and flags < 2. This is correct for LZ → `LoadEncoded` → `GfxTilesetLoadNone`.
- **Impact:** a direct `GfxTilesetLoadNone`, `LoadPacked` (through the nested `Flush` frame) or `GfxTilesetAddColorTile` (flags 2, excluded) taken before the title would walk the wrong frames. The `found.get` assertion would then fail spuriously or pick the wrong target.
- **Question:** does Snake make any direct raw tileset load before the title?
- **Fix:** match the discovered target against a known code signature, as `leaf_entry` does.

## Optional polish (not required)
- The per-byte cost is roughly 200–350 T-states because Count, Position and Value round-trip through `(iy+d)`. That works out to about 3–4M T for 11,776 output bytes, which already meets the target.
- If more speed is wanted later, keep Count and Position in registers within a run, or use LDIR for literal runs that do not straddle a chunk.

## Limitations
- I did not see the native build, the compiler's tokenizer handling of `*`, the record-layout rules, `zvb-leaves.h` output, or evidence that the 1,361,111 assertions passed. No tests were run by me.
- I trust the `.INC` byte sequence to match the generator, based on spot-checking every jump word and the total length.

### Codex reconciliation

1. Accepted: deterministic LZ history runs of67 at circular position220 with
offsets1/5/17 and position476 with offset256; RLE128-literal at200; LZ64-literal
at220 with exact input end. These force all pending-token paths across chunks.
Random coverage is separately printed:31 RLE streams output>256 bytes, zero LZ;
no random LZ chunk-coverage claim remains. Total1366213 sanitized assertions pass.
2. Accepted: PACK fixture includes the exact generated record declaration (not
its helper) and asserts size19 plus Full16/Error18 address offsets under the real
Pascal compiler. Native PACK/PAGE compile/execute and detached saved runs pass61
harness assertions.
3. Accepted: stock source flattener rejects >32768 bytes with an explicit stock
Unit authoring RAM-budget message. source.c import_feed reserves out+1 for each
byte, with no terminator allocation:32768 bytes occupy exactly two pages. The
preliminary32835-byte variant failed serialization99; the32749-byte source passes
native authoring374. This is a stock qualification budget, not a language limit.
4. Accepted: generator --check compares exact output; the CPU test runs it first.
5. Accepted/adapted: native profiler finds the linked383-byte body and checks all
36 internal relocation words at its actual final address47049. Unit authoring
and saved Snake pass (compiler_inline canonical coefficient1 relocation is real,
not only the synthetic host resolver). PIO ISR in target/zeal8bit/pio.asm saves
AF/BC/DE/HL and does not alter IY; no IY use occurs in the supported kernel/Zeal
ISR sources or their keyboard/vblank handlers. Native Snake runs with live IRQs;
IFF/state oracle variants remain preservation tests, not interrupt-injection claims.
6. Scope clarified: profiler discovery targets this Snake's LZ call chain only.
The explicit --no-lz mode skips that discovery for the raw-asset experiments;
no claim of general profiling across arbitrary decoder/call chains is made.

Measured native result: startup296582421->17958463T (29.66->1.80s at10MHz), LZ
calls289665689->11041731T. Saved COM36798->35844bytes. All pixels, title cadence,
controls, growth, replay, collision and actual exit/shell-command checks pass;
the pre-existing wipe timing remains24385215T versus C11584421T. No full timing
parity is claimed. C startup3259102T uses raw/4bpp inputs, not LZ.

User separately asked whether LZ is still needed to fit. Ready-to-copy assets
would be11776 bytes versus2139 compressed. Both current raw Byte and equivalent
Word-literal variants fail compiler99. We therefore retain the qualified compressed
path and do not claim raw fit or a hardware requirement for compression. Exact
remaining compiler resource constraint is not isolated; captures are recorded.
This experiment does not change or block the shared decoder fix.

## Opus turn 20261108T050553-0d4d3bc1

Mode: code; read-only

# Follow-up review of zvb-decoder-v1: final turn

**Verdict:** I found no defects in the decoder, `LoadEncoded`, or the new tests and tools. Every first-turn finding is resolved or acceptably scoped. The raw-asset compiler error 99 and the remaining startup and wipe gaps are documented objectives, not defects in this change. The only items left are two optional low-severity notes.

## Correction to my first turn

I wrote that there were "35 relocated internal words". The correct number is **36**. Counting the generator's `j(...)` calls gives 36, and I had in fact checked all 36 deltas in my table. The evidence figure of 36 is right.

## Disposition of first-turn findings

**1. Chunk-straddling coverage: resolved.** I traced each new fixture:

- **LZ history, offsets 1, 5 and 17:**
  - 220 bytes of literals (64+64+64+28), then `FF`, then the offset byte: a history count of 67 at position 220.
  - The chunk boundary falls 36 bytes into that token.
  - After the resume, offset 5 reads ring index `(0-5)&255 = 251`, which exercises the wrap.
- **LZ history, offset 256:**
  - 476 literal bytes, so the first chunk ends at a token boundary.
  - The 67-byte history token then straddles 512 with `Known=256`.
- **RLE repeat:** 100 + 128 bytes, then a third 128-byte repeat that straddles at 228.
- **RLE literal:** a 128-byte literal starts at 200. The source ends exactly at byte 133.
- **LZ literal:** a 64-byte literal starts at 220. The source ends exactly with the literal's last byte.

All three pending run kinds now resume after a chunk deterministically. The evidence reports random coverage honestly as RLE 31 and LZ 0.

Native `PACK.PAS` adds further cases that stand on their own:
- a 67-byte overlap with offset 3 straddling a chunk;
- a 256-offset copy crossing the `$3FFF`/`$4000` VRAM page;
- exact 64 KiB output;
- overflow after 64 KiB, which keeps earlier chunks and returns `GfxInvalidArg`;
- a tail at `FromByte` 65535.

**2. Record-layout guard: resolved.**
- `SizeOf=19`, `Full=16` and `Error=18` are asserted by the real compiler.
- With sequential packing and the declared types, the intermediate offsets follow from these three values.
- Limitation: the fixture compiles a copy of the declaration text in the client program, not the unit's own type. The same compiler rules apply, and Snake's native pixel checks would catch a mismatch, so this is acceptable.

**3. Source-size guard: resolved.** The flattener now rejects sources over 32,768 bytes. Your `import_feed` reasoning, and the failure of the 32,835-byte variant, support that limit.

**4. Drift check: resolved.** `generate-zvb-decoder.py --check` does an exact comparison and runs before the CPU tests.

**5. Relocation and interrupts: resolved and scoped.**
- The profiler locates the unique linked 383-byte body at 47049 (`$B7C9`) and checks all 36 words against `0x4000 + position + offset + delta`. That exercises the real `compiler_inline.c` and linker path.
- The ISR analysis, plus native Snake running with live interrupts, closes the IY question. The oracle's IRQ variants are correctly described as preservation tests only.

**6. Profiler heuristic: scope accepted.** Discovery is documented as specific to this Snake's call chain, with `--no-lz` for the raw variant.

## Remaining items (optional, low)

1. **Relocation check runs only in the profiler.**
   - The 36-word check lives in `profile-snake-startup.py`, not in the regular `test-zvb.py` or leaf regressions.
   - A future change to the compiler or linker's relocation would then show up only as a Snake or PACK failure, not as a specific diagnosis.
   - Polish: move the check into a routine test step. This is not required, because those failures would still occur.

2. **Documentation polish.**
   - `docs/zvb-library.md` names `ZVBIO.INC`, `ZVBGFX.INC` and `ZVBPACK.INC` as the private implementation files but omits `ZVBDECODE.INC`.
   - `docs/snake-parity.md` has missing spaces in several phrases, for example "to17,958,463", "takes3,259,102", "by954", "same11,776" and "compiler99".

## Documented objectives, not defects

- **Raw assets.** The Byte and Word ready-to-copy variants fail with compiler error 99, the cause is not yet isolated, and they are explicitly unqualified. This does not affect the decoder fix.
- **Remaining startup gap.** Pascal startup is 17.96M T-states against C's 3.26M.
  - The non-LZ portion is unchanged at about 6.92M T-states before and after, which is consistent.
  - LZ work averages about 937 T-states per output byte. That figure includes the per-chunk `GfxTilesetLoadNone` and `VRAMTransfer` calls and transformed byte-loop copies, such as the font's offset and opacity.
  - The profile does not separate decoder time from VRAM-copy time, so any further optimization would need that split first.
- **Wipe timing.** 24.39M against 11.58M T-states is pre-existing and correctly reported as failed parity.
- **Apple positions.** The changes in `snake-parity-evidence.json` follow from the non-deterministic random sequence, which the contract explicitly does not promise.

## Consistency checks

- Speedup: 296,582,421 / 17,958,463 = 16.51.
- COM size: 36,798 − 35,844 = 954 bytes, matching the code-size change of 29,799 − 28,845.

## Limitations

- I ran nothing. Pass counts are taken from the supplied evidence: CPU 1,366,213; Unit 374; PACK/PAGE 61; Snake 38.
- I did not see the captures themselves, `package-tf.py`, or the native test harness source.

### Codex reconciliation

All first-turn findings are resolved as accepted by the follow-up. No production
or test code changed after this final review. Both Opus turns are complete, with
no decoder defects or required fixes remaining. Native Unit374, PACK/PAGE61,
Snake saved38 and CPU1366213 assertions pass; final linked36-word relocation and
all behavior/pixel/exit checks pass. Only the pre-existing wipe comparison fails.

Optional1: deferred moving the specialized relocation diagnosis into the generic
hardware runner. The startup profiler owns the actual Snake-body check; PACK and
saved Snake independently exercise the same compiler/linker path. No correctness
gap requires a new framework or duplicated check here.
Optional2: accepted the implementation-file listing and spacing corrections in
docs only. Those editorial fixes follow the final review, so no unchanged-input
finalization seal is claimed for the modified docs. The production/test code
review verdict is unchanged. No extra review turn is requested.

The reviewed raw-asset experiments remain unqualified (compiler99), and startup
1.80s remains above C0.33s. These are explicitly open objectives, not claimed
resolved by this decoder change. The requested performance fix is verified from
29.66s to1.80s, and the self-contained COM is954 bytes smaller.

Post-review read-only diagnosis (no production/test code changes): a native
cc_fail trace records error99 at virtual876D, caller return8F39. The linked
compiler_sources.c listing maps that return to line177, after sources_include
returned negative for RAWDATA.INC. This narrows the raw-asset failure to source/
include retention before final linking. Raw runtime fit remains unmeasured.
The evidence/docs are updated with this diagnosis; no code approval or final
input seal is claimed for post-review documentation edits.
