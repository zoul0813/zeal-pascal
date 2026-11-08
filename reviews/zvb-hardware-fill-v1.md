# Review: zvb-hardware-fill-v1

## Opus turn 20261108T150625-a45fdfff

Mode: code; read-only

# Review: `zvb-hardware-fill-v1`

**Result:** I found no correctness defect in the changed fill bytecode, and no blockers or high-severity findings. There are two established low/medium issues, and the patch has not been shown to address the reported corruption.

## Checked by hand
I decoded the full `VRAMTransfer` Inline stream by byte offset.
- **Branches:** All relative branches land on instruction boundaries at the intended targets: general loop at 96, restore at 175, and fill loop at 83 via `20 F6`.
- **Fill loop:** It reloads Add, stores to DE, increments DE, decrements BC and tests BC. It stops at exactly the requested count.
- **Bounds:** The same-bank check allows the end to equal `$4000` and otherwise diverts to the general path.
- **Cross-bank fills:** Flag 2 branches from offset 134 to 151, skipping `LD A,(HL)`. The docs' "never read VRAM" claim therefore holds for both fill paths.
- **Restoration:** The MMU and IFF restore sequence is unchanged.

## Established findings

**1. Medium: reads of device memory remain, and the docs misdescribe two of them**
- **Location:** `GfxSpriteSetTile`, `GfxSpriteSetFlags`, `GfxPaletteRead`, `VRAMTransfer` flags 4 and 8, and `docs/zvb-library.md` under "SDK implementation paths".
- **Evidence:**
  - `GfxSpriteSetTile` contains `$71/$23/$7E/$77`, which writes the tile, then reads and rewrites the flags.
  - `GfxSpriteSetFlags` contains `$7E/$77`, which reads and rewrites the tile.
  - `GfxPaletteRead` uses LDIR with VRAM as the source, and the docs call it a Pascal extension.
- **Impact:** A board retest cannot isolate VRAM reads as the cause while these paths remain, if the failing program reaches them. The docs' statement that sprite setters "write fields directly" is inaccurate for these two setters.
- **Fix:**
  - Compare both sprite setters with the SDK source, then correct the docs or the code.
  - List every remaining device-memory read and whether the SDK has the same read.
  - Soften "one definite departure" unless that audit supports it. `GfxPaletteRead` has no SDK counterpart by the project's own description.

**2. Low: the new fill keeps interrupts masked longer than necessary**
- **Location:** `VRAMTransfer`, offsets 83–92.
- **Trigger:** `GfxInitialize` fills 3,200 tilemap bytes in one masked call.

| Fill method | T-states per byte | 3,200-byte fill | Time at an assumed 10 MHz |
| --- | ---: | ---: | ---: |
| Previous LDIR | 21 | about 67,200 | about 6.7 ms |
| New loop | 58 | about 185,600 | about 18.6 ms |
| SDK-shaped loop | 39 | about 124,800 | about 12.5 ms |

- **Impact:** At the assumed clock, the new loop masks interrupts for longer than one 60 Hz frame. The effect is mostly limited to setup, but it is avoidable.
- **Fix:** Use the SDK loop shape, since HL is unused on this path:
  `DD 7E 06 / EB / 5F / loop: 73 / 23 / 0B / 78 / B1 / 20 F9`
  This adds 2 bytes, so the affected branch offsets must be updated. Add a `leaf_cycles` limit like the tile leaf's check.

**3. Low: the no-read regression has narrow coverage**
- **Location:** `tests/host/zvb-leaves.c`, fill-oracle loop.
- **Gap:** The no-read assertion covers only flag 2 with Add `$12`. Flag 3 and Add 0 are checked for output but not for reads. Add 0 is the value `GfxInitialize` uses.
- **Impact:** The risk is small because these cases share the decoded path. The test also exercises only the leaf, so it does not prove the native unit's callers perform no VRAM reads.
- **Fix:** Add flags 2 and 3 and Adds 0 and `$12` to the loop. The pending native run should cover `GfxInitialize` and `GfxTilesetAddColorTile`.

## Evidence versus hypothesis

**Established from the supplied code:** The old same-bank fill read VRAM, and the new fill does not. The solid-fill callers supplied are the three `GfxInitialize` fills and `GfxTilesetAddColorTile`.

**Not established:** Nothing supplied links this change to green turning purple.
- The only palette bytes this path touches are entry 0, through the 2-byte `$E00` fill. Other entries still use the unchanged RAM-to-VRAM LDIR in `GfxPaletteLoad`.
- A bad fill read could corrupt tile pixels or map cells, but a consistent color substitution would need another explanation.
- Corruption varying across launches could also involve state saved and restored by `VIDEO.INC`, which was not supplied.
- Pure RGB565 green (`$07E0`) becomes `$F81F`, magenta, when inverted. Byte-swapped, it becomes `$E007`, which has no green. Palette data or byte order is therefore worth checking, but this is only a lead.

**Suggested board check:** Run baseline and patched builds across the same number of cold boots. Use a minimal program with a known palette and a solid tile, and record the OS and firmware versions. Test `GfxPaletteRead` and sprite setters separately, so their reads do not confound the result.

## Questions
1. Do the SDK's sprite tile and flags setters read the companion byte, or do they write without reading?
2. Which graphics calls does Snake make at startup and during play, and does `VIDEO.INC` restore palette or VRAM contents read from the device?
3. Which callers in `ZVBPACK.INC` use `VRAMTransfer`, and with which flags?

## Limitations and pending verification
- **Not run or evidenced here:** I had no logs for the 1,434,093-assertion pass or the pre-fix failure, so those remain reported results. I have no result yet for `build/ZVB-FIX.ZPU`, the Snake recompile and 180-frame run, or the board retest.
- **Files not supplied:** The SDK source, `VIDEO.INC`, Snake source, `ZVBPACK.INC`, the Z80 core and the two newly unignored evidence/review files.
- **Follow-up artifacts:** This patch removes 4 code bytes, so any size or hash presented as current must be regenerated. The figures in `docs/zvb-library.md` are acceptable only as historical checkpoints.
- **HelloName:** The native error 6 report is untouched by this change, and the supplied diff makes no claim to fix it.

### Codex reconciliation

Initial findings reconciled:

1. Accepted documentation clarification. Inspected SDK `sdcc/gfx/sprite/set_tile.c` and `set_flags.c`: they read/rewrite the companion byte exactly as Pascal does. No sprite-code change warranted. Documented those reads, the Pascal-only palette-backup read, and internal transfer read/companion flags. Current public tileset callers pass copy/opacity flags0/1 or fill2; packed loaders decode RAM then call GfxTilesetLoadNone. Snake saves512palette bytes through GfxPaletteRead and restores them on exit. This patch changes solid fills, not all device reads, and makes no confirmed physical-root-cause claim.
2. Accepted SDK-shaped fill loop. Uses HL as destination and E as color, with39T-states per repeated byte rather than58. Updated relative branches for the two additional bytes; final leaf186bytes versus188baseline. Added same-bank cycle bound45*count+1000. The3200byte fill remains interrupt-masked, as the SDK is; physical latency still requires measurement.
3. Accepted wider no-read oracle coverage: flags2/3, fill values0/$12, bothIFF states, zero/one/31/512/16391byte spans and normal/cross-bank offsets. Final sanitized leaf suite passes1,637,765assertions. The old same-bank fill fails the no-read regression; final version passes. A fresh native Unit passes28assertions and has SHA256a5c11c1bfe8fb0420796d38983fd11e448621f5038b3ccc2fbd0cfc3ac966bc0. Saved Snake verification now passes: report `analysis/native-slice/20261108T151836.088644Z-2023/zvb-report.json`, 38assertions, zero failures,180demo frames and saved execution with allZPU/PAS/INCfiles removed. Program SHA256 `86ffd2ffaeecd4738fd0f86dded4029d492368f5009b5f733d7d480fc375af98`.

Separate compiler failure diagnosed from the reported hardware kernel `5bb34a78bf23a4ff514488ffb0c109b389dce029`: its pfree zerosA while checking ownership, then passesA=0 to MMU_FREE_PAGE; mmu_free_page subtracts RAM page base32 and returns ERR_INVALID_PARAMETER6. The page remains allocated even though its owner was cleared. Upstream fixes98ce65d and45e9478 correct status and page number; v0.8.0 includes both. Our ZDE test kernel includes those corrected instructions. This explains the exact native6 at compiler cleanup without changing Pascal or modifying third-party source. Exact HelloName compiles in production IDE under bothHostFS and real emulatedTF storage (capture paths in forthcoming evidence). Added the kernel prerequisite to the hardware checklist. Physical confirmation after a standard OS update remains required.

## Opus turn 20261108T152835-e93f530c

Mode: code; read-only

# Follow-up review: `zvb-hardware-fill-v1` at `e2c0a77`

**Verdict:** I found no blockers to issuing this as a hardware **retest** overlay. All three earlier findings are resolved, and I found no defect in the final fill code. Fix the retest instructions in finding 1 before handing it over, because they could lose the comparison the retest exists to make. This is not board acceptance.

## Prior findings
| # | Status | Basis |
|---|---|---|
| 1. Remaining device reads and docs | Resolved | The docs now describe these reads accurately. `ZVBPACK.INC` confirms that public loaders use only flags 0/1, through `GfxTilesetLoadNone`. The SDK sprite behavior is accepted as reported, because that source was not supplied. |
| 2. Fill loop timing | Resolved | The loop is `LD (HL),E / INC HL / DEC BC / LD A,B / OR C / JR NZ`, or 39 T-states per repeated byte. HL and E are free on this path. |
| 3. Oracle coverage | Resolved | The loop now covers flags 2/3 and Adds 0/`$12`. The 45n+1000 bound rejects the earlier 58 T-state loop. |

## Checked by hand
- **Bytecode:** I decoded all 186 bytes of `VRAMTransfer`. The fill loop branches back to offset 88, restore is at 177, and the general loop is at 98. All branch targets land on instruction boundaries.
- **Assertion delta:** The widened loop accounts for exactly the reported increase of 203,672 (1,434,093 to 1,637,765). This checks the arithmetic only; I did not run the suite.
- **Artifact binding:** The library hash, Snake hash, 35,842-byte size and `ZVBGFX.INC` hash agree across the manifest, evidence file and native reports. I did not recompute any file hashes.
- **Tested ROM:** The `pfree` bytes decode as described, and hex offset `1125` equals the evidence file's 4389.

## Correction to my first review
I said a consistent color substitution "would need another explanation." The Snake source now supplied weakens that statement:
- Checkerboard tiles 32 and 33 and bottom-row tile 34 are created only by `GfxTilesetAddColorTile`, using palette indices 16, 17 and 18.
- Those entries are green: `$AEA8`, `$9E27` and `$6C44`.
- Palette entry 0 is `$DC1F`, a pink-purple.

If overlapping-LDIR reads returned 0 on the board, each tile would keep one green pixel and turn purple elsewhere. **This is a plausible mechanism, not evidence of the cause.** The same appearance could come from failed writes, which the patch would not fix.

Snake also does not call `GfxSpriteSetTile` or `GfxSpriteSetFlags`. After this fix, its only VRAM read is the startup `GfxPaletteRead`, which affects restoration on exit and not in-game colors. That removes the confound I raised earlier for this program.

## Established findings

**1. Medium: the retest procedure can lose the baseline comparison**
- **Location:** `build/tf-snake-retest-review/README.TXT`.
- **Trigger and impact:**
  - The README first says to copy the new `zvb/SNAKE.COM` over the same path, then asks for a comparison with the original. Followed in order, this overwrites the original on the card.
  - It also runs the comparison only on the updated OS. If the OS update changes the symptom, the known failing environment is gone and a clean result cannot be attributed to the fill change.
- **Fix (README only):**
  - Keep the original under a distinct name, such as `SNAKEOLD.COM`.
  - Run the comparison on kernel `0.6.0-24-g5bb34a7` first, then repeat it after the update. Snake does not need the compiler.
  - Ask the tester to record whether purple is limited to tiles 32–34, and to photograph one tile closely.
  - State that FirstBar and the other precompiled demos still contain the old fill, so they do not test this fix.

**2. Low: the OS prerequisite is not fully tied to what was tested**
- **Location:** `docs/zvb-hardware-fill-evidence.json`, under `tested_rom` and `verified_fixed_release`; README prerequisite.
- **Gap:** v0.8.0 is established by commit ancestry, while the emulator passes used a local ROM with no recorded version. The supplied evidence therefore does not show that v0.8.0 itself ran the compiler.
- **Fix:** Record `git describe` for the tested ROM. If it is not v0.8.0, label v0.8.0 as containing the fixes but not emulator-tested.

**3. Low: the overlay does not prove compiler compatibility with the base package**
- **Location:** `MANIFEST.JSON`.
- **Gap:** The new `ZVB.ZPU` will be used with the compiler already on the card, and this overlay bypasses the packaging tool's compiler-identity check. The source diff shows no compiler changes, but the base manifest was not supplied.
- **Verification:** Compare the report's `zpascal.bin`, `zpascal.zpl` and runtime hashes with `build/tf-acceptance`, then record the result in the final manifest.

**4. Low: the final native run does not check fill contents**
- **Evidence:** The new reports show that Unit authoring and Snake execution passed, but they list no client that inspects filled VRAM. Fill contents are proven at leaf level only.
- **Test gap:** No leaf case ends exactly at a bank boundary, although every 64th 8bpp tile does. My decode finds that case correct, so this is coverage only.
- **Optional check:** The documented `GFX.PAS` client reads tiles back through DMA and needs no source change.

## Evidence versus hypothesis
| Claim | Status |
|---|---|
| Old same-bank fills read VRAM; new fills do not | Established from bytecode |
| Fill code and overlay pass emulator checks | Reported, with hash-bound reports supplied |
| Bad fill reads caused the purple checkerboard | Hypothesis; needs board comparison |
| Kernel source defect explains native error 6 | Reported source analysis; physical path is inferred |
| Updating the OS resolves native error 6 on the board | Not tested |

The native error 6 explanation is carefully labeled in the evidence file. Running HelloName on an emulator ROM built from `5bb34a7` would strengthen it if that is easy to do. The successful emulator compiles used a corrected kernel, so they do not reproduce the failure.

## Question
Does `pfree` at `5bb34a7` return the MMU routine's 6 to its caller? Upstream `98ce65d` is described as a return-status fix, and the fixed routine ends in `XOR A`. The kernel source was not supplied, so I cannot confirm that step.

## Limitations
I did not have the SDK or kernel sources, the base acceptance manifest, the test logs, or the binaries themselves. I ran no tests. The final overlay needs its checkpoint and README hash refreshed after any README change; the ZPU and COM hashes should remain unchanged.

### Codex reconciliation

Follow-up reconciled. No code blocker remains for a hardware retest; no physical graphics pass is claimed. The final library and program bytes remain exactly those reviewed.

1. Accepted baseline-preservation correction, adapted to new user evidence. During this review the user updated to kernel 0.8.0 and confirmed both Minimal and HelloName compile; the original Snake remains corrupt. That supplies the baseline on the updated OS, so no downgrade is appropriate. The final overlay will include the original, hash-verified COM as `zvb/SNAKEOLD.COM`, alongside the corrected `zvb/SNAKE.COM`. Final README will tell the operator to compare these on the same updated kernel, record whether purple is limited to the checkerboard/bottom tiles, and retain a close photo of a tile. It will explicitly say other precompiled demos remain unchanged. The review-copy README/manifest stay frozen for audit; the delivered README receives this minor procedural correction after review. No implementation changes or additional review round are needed for that correction.
2. Accepted test-kernel provenance clarification. The ZDE source checkout is `4d6c869ab57d2417f916258bb813408726b86c6e`; `git describe --tags --always --dirty` prints `latest`. This is not a v0.8.0 emulator run. The recorded ROM hash and decoded pfree bytes establish the corrected implementation actually present in the emulator. Release v0.8.0 contains the upstream fixes by ancestry; the user's subsequent physical report now confirms Minimal and HelloName compile on v0.8.0. The earlier evidence JSON is a historical snapshot from before that user report; this reconciliation and final overlay manifest carry the updated result.
3. Accepted compatibility verification. Compared both report hashes with the original acceptance package: `zpascal.bin` SHA256 is `5810328c8d20e18560a8737baceb51570ef9cf075ed6cf072aeab7f4e009781f`; `zpascal.zpl` is `b06502ee54a0cd672ffad253f9ee2b90593633ab3afd369e6c38865f702cd490`. All 166 common source/runtime/product inputs agree between the original examples report and patched Snake report, including runtime payload/private/profile/layout. The final manifest records this comparison. Original Snake SHA256 is `8d1ae8b2ddc79a1299dd14080746f7f79b4fb47faa2bfe52e709666d051b5942`, checked against the base manifest.
4. Accepted coverage observation. Added an ad hoc execution of the unchanged final leaf with sizes 0/1/31/256/512/16384/16391 and offsets 0/16377/16128. This covers a complete bank and a final 256-byte tile ending exactly at $4000; 4,046,917 sanitized assertions pass. The standard committed suite remains the reviewed 1,637,765-assertion suite. Making the additional boundary matrix permanent and running the separate DMA-backed GFX client remain follow-up coverage, not blockers: native Snake already exercises the callers, and fill bytes are inspected exhaustively at CPU-leaf level. No native VRAM-content inspection beyond that is claimed.

Question answered from the exact old source: after `MMU_FREE_PAGE()` calls `mmu_free_page`, old pfree executes `ret` without clearing A. `mmu_free_page` receives zero, subtracts the RAM base 32, takes the invalid-parameter path, loads A=6 and returns it. The user-confirmed OS update resolves the compiler symptom without any Pascal compiler change.

Two normal Opus turns are complete. Board graphics diagnosis remains open: the user still sees corruption with the original COM on kernel 0.8.0; the replacement COM has not yet been tested physically. No third-party application source was modified and no board/media writes were performed by Codex. Final overlay construction must verify all hashes and preserve the original acceptance directory.
