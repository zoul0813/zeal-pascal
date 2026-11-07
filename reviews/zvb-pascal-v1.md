# Review: zvb-pascal-v1

## Opus turn 20261107T063344-9ae3f2d9

Mode: code; read-only

# Independent review: `zvb-pascal-v1`

**Result:** I found one high, one medium and three low established findings, plus five questions. No tests were run for this review, and I do not infer any client, demo or hardware pass from the supplied evidence.

## Established findings

### 1. High: `GfxSpriteSetTile` and `GfxSpriteSetFlags` omit the SDK's paired byte write
- **Location:** `libraries/ZVBGFX.INC`, `GfxSpriteSetTile`, `GfxSpriteSetFlags` and `SpriteField`
- **Trigger:** Any call to either routine.
- **Impact:** The Pascal code writes only offset 4 or only offset 5. If sprite VRAM commits this field as a 16-bit pair, as the SDK code suggests, `SetTile` may have no effect or may commit a stale companion byte. `SetFlags` could likewise affect the wrong tile. This hardware effect is inferred, but the API deviation is established.
- **Evidence:**
  - `set_tile.c` writes `tile`, then does `destination->flags = destination->flags`.
  - `set_flags.c` does `destination->tile = destination->tile`, then writes `flags`.
  - The other sprite setters write complete 16-bit fields.
  - The deviation is not listed under the documented Pascal differences.
- **Coverage gap:** `GFX.PAS` checks only DMA read-back, which passes if the emulator has no such latch. Snake and Sprites do not call either routine.
- **Fix:**
  - Preserve the SDK's order of writes to both bytes.
  - One option is a `VRAMTransfer` flag that uses `LD A,(DE)` as its source, so the routine can rewrite the companion byte without adding a fifth leaf.
  - Recheck all relative jumps and rerun the leaf tests.
  - Add a physical hardware check for both routines.

### 2. Medium: 1-bit and 2-bit tileset loads lost SDK 4bpp support
- **Location:** `libraries/ZVBPACK.INC`, `LoadPacked`; `docs/zvb-library.md`, "Tilesets and compression"
- **Trigger:** `GfxTilesetLoad1Bit`, `GfxTilesetLoad2Bit`, or `GfxTilesetLoad` with those formats, while using a 4bpp context.
- **Impact:** Pascal returns `GfxInvalidArg`, while the SDK packs and loads the pixels. This is a functional API regression, not just stricter validation. The docs describe the rejection but do not list it as a deliberate difference.
- **Evidence:**
  - `load_1bit.c` and `load_2bit.c` have explicit non-8bpp branches.
  - Only the 4-bit, RLE and LZ loaders require `ctx->bpp == 8`.
  - `LoadPacked` rejects `BitsPerPixel <> 8` for all packed formats.
- **Fix:** Either implement the 4bpp branches or obtain approval for the removal and list it under the deliberate differences.
  - Keep the SDK's nibble packing and masked palette offset.
  - Pass the already-packed bytes with zero offset, so `VRAMTransfer` does not add the offset again.
  - Keep the corrected page-boundary behavior.
  - Add 4bpp tests, including one that crosses a page.

### 3. Low: A non-canonical `Opacity` value can select fill mode
- **Location:** `libraries/ZVBGFX.INC`, `GfxTilesetLoadNone`, argument `Ord(Opacity)`
- **Trigger:** `Opacity` holds a stored value other than 0 or 1, such as a partly initialized `TGfxTilesetOptions` local, and `Ord` does not normalize it.
- **Impact:** Bit 1 of the flags means "fill with Add". A stored value of 2 could therefore overwrite tiles with `PaletteOffset`, and the source data would be ignored.
- **Evidence:** The value goes directly into a two-bit flag byte. The SDK treats any nonzero `opacity` only as true.
- **Fix:** Compute the flag with `if Opacity then 1 else 0`, or mask it with `and 1`. Add an options test using a non-canonical value, if the language permits one.

### 4. Low: A non-direction key cancels a queued Snake turn
- **Location:** `examples/zvb/SNAKE.PAS`, `ReadControls`
- **Trigger:** The player presses `W`, then any unmapped key, before the next snake movement.
- **Impact:** `NewDirection` defaults to `Direction`, so the unmapped key writes the current direction back to `NextDirection`. The turn is lost.
- **Fix:** Update `NextDirection` only when `W`, `A`, `S` or `D` was recognized.

### 5. Low: `RestoreVideo` changes sound state in programs that do not use sound
- **Location:** `examples/zvb/VIDEO.INC`, `RestoreVideo`
- **Trigger:** Any exit from `SPRITES.PAS`, or from the first example in the docs.
- **Impact:** Every voice is held and master output is disabled, even though these programs never saved or changed sound state. This also conflicts with the documented rule that a program owns restoring state it changes.
- **Fix:** Add a `SoundUsed` flag or separate sound cleanup, used only by programs that start sound.

## Questions needing evidence

1. **Is the `$8E` device register readable?** `SaveVideo` and `SOUND.PAS` read it, but the SDK header does not mark it R/W. If it is write-only, restoring `SavedDevice` could select the wrong peripheral.
2. **Can DMA read the palette at `$100E00` on physical hardware?** If it cannot, `SaveVideo` stores incorrect colors and the demos restore a bad text palette. The emulator tests cannot settle this.
3. **Does `Delay` work without `CrtInit`?** `ZVBCHECK.PAS` calls it without initializing Crt, and the Crt source was not supplied.
4. **Can a valid RLE stream have one trailing pad byte?** The SDK stops at `size - 1` and ignores such a byte, but Pascal rejects it as truncated. Confirm that the Zeal encoder never produces this form.
5. **Should Snake support arrow keys?** The C example uses them, while the Pascal version uses WASD. Also confirm how Crt's `ReadKey` reports arrows, because a TP-style second byte such as `P` could trigger pause.

## Checked with no defect found

- **Inline leaves:** I decoded all four by hand. Argument offsets, relative jumps, page advance, page 0 restore and IFF restore are consistent with the stated stack layout. Compiled client execution remains the proof of that layout.
- **Bounds and codecs:**
  - Packed output limits and page-boundary copying are consistent.
  - RLE and LZ token parsing, truncation checks and overlapping history are correct.
  - LZ history is preserved across 256-byte flushes.
- **CRC, sound and DMA:**
  - CRC register order is correct.
  - Sample configuration and FIFO flag use match the SDK.
  - The deliberate voice-hold change is documented.
  - The master volume bits are correct.
  - Physical address calculation and descriptor layout are correct.
  - The 12-byte descriptor page check is correct.
- **Snake:**
  - Collision rules and ring-buffer handling are consistent.
  - Moving into the tail cell works.
  - The board-full path is consistent.
  - The finite demo covers eating, losing, resetting and quitting.
- **Docs:** The "44 routines" count is correct.

## Coverage gaps

- There is no client test for 4bpp 1-bit or 2-bit loads. These tests are needed once finding 2 is resolved.
- There is no explicit LZ test at maximum distance 256, or with history crossing a page boundary.
- `SoundFrequencyToDivider` is tested only at 440 Hz, so add a value of at least 32768 Hz to exercise unsigned division.
- `SOUND.PAS` does not assert that an odd-length 16-bit sample buffer is refused.
- The DMA address assertion recomputes the same expression as the implementation. The copied bytes are the stronger check.

## Review limitations

- **Files not supplied:**
  - `SNAKETIL.INC`, `LETTERS.INC`, `NUMBERS.INC` and `PALETTES.INC`
  - `snake.h`
  - `examples/zvb/README.md`
  - `libraries/ZVB-NOTICE.txt`
  - `tests/host/zvb-leaves.c`
  - `tools/zvb-source.py`
  - The `SMOKE`, `OPTIONS` and `PAGE` client sources
  - The Crt unit source
- **Effect on conclusions:**
  - I could not verify the asset bytes against `assets.json`, or the Snake tile and direction numbering.
  - I could not review the documentation linked in those files, or the leaf harness.
  - The default client list in `tools/test-zvb.py` depends on the three missing client files, so confirm they exist.
- **Compiler behavior:** Boolean `Ord`, `Mem[...]` as an untyped `var` argument, and indexing a string constant remain unproven until client compilation passes.

### Codex reconciliation

Accepted and implemented all five findings:

1. Sprite Tile/Flags setters rewrite both bytes in SDK order. The actual-Z80
   oracle watches low-byte/high-byte writes, including a page crossing.
2. 1/2-bit decoders support SDK 4bpp packing, including layer1 attributes.
   PACK.PAS and OPTIONS.PAS verify packed pixels and the non-Nil options path.
3. Opacity passes through a Boolean branch; GFX.PAS exercises stored value2.
4. Unmapped Snake keys leave the queued direction intact. Arrow codes160..163
   and WASD are recognized, with one queued turn checked per movement.
5. Shared video cleanup does not touch sound. Snake gates sound cleanup with
   SoundUsed, including setup failures before sound initialization.

The prior native basic and graphics groups each passed140 assertions; ZVBCHECK
passed32. Each saved client was launched with all ZPU/PAS/INC files renamed
away. The initial integration failures were fixed in the compiler/storage
checkpoint09f49ac; its reports retain independent review limitations.

Questions: RTL excerpts/hashes in zvb-rtl-reference.md establish readable$8E,
palette reads and the DMA/core bus routing. This is source evidence, not a board
test. Crt Delay uses the supported OS sleep syscall without CrtInit. Crt ReadKey
returns the native arrow codes (no TP second-byte prefix); its real-key RAM and
saved tests pass32 assertions. The SDK encoder emits complete RLE tokens without
trailing padding; Pascal deliberately rejects malformed/truncated streams.

A measured Snake failure exposed page pressure when root code crossed16KB.
GfxPaletteRead is a documented Pascal extension in the existing VRAM transfer
path; it snapshots colors before initialization and removes the demo cleanup's
DMA dependency. The corrected four leaves total232 bytes and pass950050 host
CPU assertions with sanitizers. Final graphics140 and utility88 pass. Snake passes Memory36 and Com-file32
assertions, produces identical COM bytes, and completes180 observed demo frames
with all libraries and source files removed. See zvb-library-evidence.json.

CRC32 now uses three local Pascal Inline syscall leaves for raw files. Existing
CP/M-style typed files contain a header and untyped files use128-byte records;
neither is appropriate for arbitrary binary lengths. No C runtime is linked.
The saved-demo driver releases compilation resources before exec, matching a
normal launch. The prior CRC exec failure was resource pressure, then raw input
showed the typed-file mismatch. Golden raw-file acceptance passes for empty,9-byte and4358-byte files.

Supply the previously missing asset/include sources, flattened Snake, SDK
snake.h, manifests, README/NOTICE, all clients, leaf oracle, source generator,
Crt generator/key tests and RTL references in the follow-up. Do not infer physical
display/audio acceptance or reviewer approval from emulator results.

Final fitting: Snake embeds the original snake tiles and numeric font only
(1404 compressed bytes), uses bounded Integer counters and a body-tile lookup,
case dispatch, a uniform original-palette field and precomputed tone dividers.
It retains movement, growth, collisions, score, restart, arrow/WASD input and
sound. Pause and alphabet banners were optional draft features removed to fit.
Y480 hides even pre-existing32px sprites below this240-line mode. Sound uses
master/per-voice/channel controls explicitly. The game has395 static bytes left.
The 180-frame marker after cleanup prevents an early initialization exit from
masquerading as successful demo execution. Current source exactly matches its
native fixture; Memory and Com-file images have identical hashes.

Packaging now copies only changed stock files, verifies all installed bytes and
IDE startup, and preserves a recovery card. A full reinstall timed out after
the shipped set grew; incremental install avoids unnecessary TF operations.
The final61539-byte ZPU, CRT and license/notice companions are verified beside
zpascal.zpl. No physical display/audio acceptance or extra core review approval
is claimed.

Final proof after all credit/fit changes: Memory36 and Com-file37 assertions
pass, each observes180 completed frames without ZPU/PAS/INC. Images are identical
45884-byte COMs, code28096/globals1404, leaving196 static bytes. Clean Sprite31
and DMA32 runs also pass; chain alignment can no longer silently skip. The
Crt/demos/stock packaging artifact hashes are recorded in the final evidence.
Creative follow-up's20 dispositions are in its report; corrected technical text
is supplied here for code review. No physical-board test is inferred.

## Opus turn 20261107T114446-e2129505

Mode: code; read-only

# Final follow-up review: `zvb-pascal-v1`

**Result:** All five original findings are resolved. I found no blocker, high or medium defect in the current code. What remains is two low findings, three questions and the physical-board qualification gap. I ran no tests; test results below come from `docs/zvb-library-evidence.json`.

## Dispositions of the original findings

1. **Sprite tile and flags paired write: resolved.** I re-decoded the 119-byte `VRAMTransfer` leaf.
   - Every relative jump target is correct, and every path reaches the page 0 and IFF2 restore.
   - With flag bit 2, `GfxSpriteSetTile` writes the tile byte from `Pair[0]`, then reads the flags byte back from VRAM and rewrites it. `GfxSpriteSetFlags` rewrites the tile byte, then writes the flags byte. Both writes happen in SDK order.
   - `tests/host/zvb-leaves.c` checks the low-then-high write order, including at offset 16383.
   - The uninitialized half of each local `Pair` is never read, which is correct.
2. **4bpp support in the 1-bit and 2-bit loaders: resolved.**
   - The expansion limit drops by one shift for 4bpp output, and pixels pack as two nibbles per byte with `PaletteOffset and 15`.
   - Flushes always fall on whole bytes, and the flush passes offset 0 with no opacity, so the offset is not applied twice.
   - The new `PACK.PAS` assertions match the SDK arithmetic: `$A5` with offset 14 gives `$FE $FE $EF $EF`, and `$1B` with offset 31 gives `$F0 $12`. The page-crossing case at `$3FFF` is covered.
3. **Non-canonical `Opacity`: resolved.** `GfxTilesetLoadNone` now branches on the Boolean. `GFX.PAS` stores the value 2 and expects opacity behavior, so it shows the compiler treats any nonzero byte as True.
4. **Lost queued Snake turn: resolved.** `NewDirection` starts at 255, and only W/A/S/D and the arrow codes `#160..#163` update `NextDirection`. The reversal check against `Direction` is correct.
5. **Sound cleanup owner: resolved.** `VIDEO.INC` no longer touches sound. Snake mutes only when `SoundUsed` is set, which covers early setup failures.

The answers to my first-review questions are accepted as source evidence, not board evidence. The `$8E` read-back and palette-read path come from the RTL excerpts, and the RLE behavior from the SDK encoder.

## Newly verified with no defect found

- **Palette snapshot:**
  - `GfxPaletteRead` uses flag bit 3 of the existing leaf (`LD A,(DE)` / `LD (HL),A`). The buffer is validated, and the same bounds as `GfxPaletteLoad` apply.
  - The RTL palette latch reads each byte independently, so byte-by-byte reads are consistent.
  - `VIDEO.INC` no longer depends on DMA.
- **Snake game logic:**
  - The `BodyTile` table matches every `snake.h` corner and line tile: 3→5, 5→13, 6→8, 9→4, 10→12 and 12→10.
  - The tail tiles 11/9/6/14 and the head tile (equal to the direction) match the SDK `draw()` logic.
  - The ring buffer holds exactly 256 segments at the win condition.
  - Hiding sprites at Y=480 works for 16×32 sprites. The demo deterministically exercises eating, losing, restarting and the 180-frame marker.
- **CRC32 syscall wrappers:** I decoded all three leaves.
  - The registers match the Zeal syscall convention: open uses L=2, BC=path and H=flags; read uses L=0, H=device, DE=buffer and BC=size; close uses L=3.
  - A negative open result is converted to a positive error code.
  - On a read failure the file is closed before `Fail`. The empty-file path returns 0 through the zero-length update.
- **Measurements:** the evidence file agrees with the docs on every figure I checked.
  - The leaf sizes are 119+25+29+59 = 232 bytes.
  - The ZPU, code, globals and metadata sizes match.
  - The five client code and COM sizes match.
  - The Snake static margin is 196 bytes.
  - The host oracle passed 950,050 assertions.
- **New tests:** the LZ maximum-distance (256) case across a flush and page boundary, the unsigned 32768 Hz frequency divider, and the rejection of odd-length 16-bit samples are all correct and meaningful.

## Remaining findings

1. **Low: the `FIRSTBAR.PAS` fixture can drift from the documented listing.**
   - **Location:** `tools/zvb-source.py --demos` regenerates only SNAKE and SPRITES.
   - **Impact:** `tests/native/zvb/FIRSTBAR.PAS` is a hand-expanded copy of the docs listing plus `VIDEO.INC`. A later `VIDEO.INC` edit would refresh the demos but not the fixture. The "tested fixture expands the same `VIDEO.INC`" claim would then become silently false.
   - **Fix:** generate the fixture with the same expansion function, or add a check that compares the docs listing, expanded with `VIDEO.INC`, against the fixture.
2. **Low: Snake prints an unset `Score` when setup fails.**
   - **Location:** `examples/zvb/SNAKE.PAS`, main block.
   - **Trigger:** `Initialize` fails before `ResetGame`, for example because `GfxPaletteRead` or a tileset load fails.
   - **Impact:** `WriteLn('Snake finished. Score: ', Score)` prints whatever `Score` contains. That is harmless if the runtime zeroes globals, which the supplied material does not establish.
   - **Fix:** set `Score := 0` in the main block. This costs a few bytes of the 196-byte margin, so remeasure.

## Questions

1. **Is the CC BY 3.0 exit credit sufficient?** This is a decision for the license owner, not a code defect. The exit line omits the work title ("Pixel Block Font") and any note that the font was adapted. CC BY 3.0 §4(b) asks for an adaptation credit where practicable. A saved COM may be redistributed without `CREDITS.md`, so this printed line may be its only attribution. Adding the title costs about 17 bytes.
2. **Where does the 4bpp layer 1 attribute layout come from?** The docs state that bit 0 is tile bit 9, bits 2 and 3 flip Y and X, and bits 4..7 select the palette. No supplied RTL excerpt covers this, so cite the source or mark the layout as derived.
3. **Is IX preservation across `RST 8` a documented Zeal OS guarantee?** The CRC32 leaves read IX-relative arguments after each syscall. The golden-output test shows the tested kernel preserves IX, but a documented guarantee would protect against other OS versions.

## Remaining blockers and limitations

- **No code blockers.**
- **Physical-board qualification is still outstanding.** Display and audio output are untested. So is the sprite paired-byte latch, palette read-back timing, chained and zero-length DMA descriptors, and the sample feed rate at faster dividers. The docs and README state all of this honestly.
- **I could not verify these from the supplied material:**
  - the asset bytes against the SDK hashes;
  - the `$F400` static boundary, the `$FC00` stack top and compiler error 99;
  - the Crt internals and compiler behavior, which are out of scope.
- **Stale text in the review record.** `zvb-docs-v1.md` still says "395 static bytes" in an earlier reconciliation paragraph; the shipped docs correctly say 196. This affects only the record.
- **No approval implied.** I imply no creative-review approval and no physical-board acceptance.

### Codex reconciliation

Accepted both low findings. FirstBar now has one producer in zvb-source.py;
--demos regenerates it and native qualification rejects a stale fixture before
execution. Verified generated text equals the documented listing after include
expansion. Snake sets Score to0 before Initialize, including repeated Memory
runs and early failure. These are post-follow-up changes, not review-approved.

Question1: the printed attribution now names Pixel Block Font, JROB774, the
adaptation for Zeal, the source URL and full CC BY3.0 license URI. CREDITS and
distribution instructions retain the original authors/licenses and adaptation.
Question2: appended hash-bound zeal.v lines713/714/811/812 to the RTL reference
and linked the field description to it. Question3: the OS kernel documentation
explicitly reserves IX/IY to applications and says the OS will not alter them;
see zvb-kernel-reference.txt with source hash and excerpt. Exec is the documented
exception and is not invoked by the three raw-file leaves.

Final verification after the low fixes: Memory36 and Com-file37 pass, including
180 observed frames, score1, full credits and removal of all ZPU/PAS/INC files.
Both produce identical45909-byte COMs (28121 code,1404 globals), SHA256
03fb5b33332a5ec1774dd2f6461f47d8d42a1171d2409f74f5d44f7f2303169c.
Static margin is171 bytes. Graphics140, utility88, standalone Sprite31, DMA32,
950050 sanitized leaf assertions, asset regeneration/pixel identity and TF
packaging also pass; exact reports/hashes are in the evidence JSON. All asset
includes regenerate byte-for-byte from the unchanged SDK and independent SDK
decoding preserves their original pixels. The FirstBar listing and fixture are
byte-identical after include expansion. No additional production compiler or
unit hardware source was changed after its respective qualification.

No known code blocker remains. Both budgeted turns completed, but the post-review
low fixes and documentation/reference updates make the input fingerprint stale.
No formal finalization or fresh approval is claimed; an extra turn needs the
user's explicit request. Physical-board video/audio, sample-rate and DMA timing
remain unqualified. Earlier numeric measurements above are chronological; this
paragraph and the current evidence give the final artifact sizes.
