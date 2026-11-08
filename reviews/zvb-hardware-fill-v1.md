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
