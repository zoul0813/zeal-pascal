# Review: zvb-docs-v1

## Opus turn 20261107T044014-d2a38bca

Mode: creative; scoped editing

I edited all three assigned files. Every technical statement comes from reading the supplied source and is **not compiled or tested**. Creatively the guide is ready for use while writing a program. Technically it stays draft: no `ZVB.ZPU` exists, and items 1–7 under "Technical inconsistencies" need your disposition.

## Substantive edits

**`docs/zvb-library.md`**

1. **New opening section, "A first program."** The old snippet left the machine in graphics mode with a black palette, which isn't a usable start. The new program draws a visible bar, handles errors and cleans up through `VIDEO.INC`. It also shows the setup `GfxInitialize` skips: palette, layer 1 and scroll.
2. **New "What each number counts" table.** This was your central focus. It covers bytes, palette entries, tiles, pixels, sprite slots, sprite records, dividers and physical addresses. `FromColor` and sprite `Count` were the unmarked traps: `SizeOf` is wrong for `Count`.
3. **Conventions split by lookup task.** The sections are Types, Buffers, Results, and Interrupts/mapping/shared hardware. It now says plainly that ZVB has no cleanup routine and that the program owns restoration.
4. **Routine tables with parameter order for each subsystem.** I checked that all 44 functions are reachable from the reference, without opening `ZVB.PAS`.
5. **Corrected a misleading `GfxInitialize` statement.** "The full background map" hid the fact that layer 1, sprites and scroll are left alone. All are now stated, each with a usable remedy.
6. **Tileset section rebuilt.**
   - One format table now includes decoded size and the 2-bit/4-bit value swap.
   - A worked example shows `$4100` as tile 65 × 256.
   - Exact RLE and LZ token tables replace the prose.
   - New limits: 1-bit always offsets zero pixels, raw 4bpp operates per byte, and decoders use 256 bytes of stack.
7. **New practical examples:** a vblank loop, multi-part CRC, sample playback, a single DMA copy and hiding sprites.
8. **New interrupt warning.** Interrupts stay disabled for a whole VRAM copy or `CRCUpdate`, and DMA holds the bus, so long calls should be split.
9. **DMA additions:**
   - a physical VRAM address table;
   - the requirement to set `DMADescLast` on single transfers;
   - `DMAOpDecrement` compared with the shifted constants;
   - a note that the page check covers only the first descriptor.
10. **Previously undocumented names now covered:** `GfxSpritesCount`, `SoundSampleTableSize`, `SpriteNone`, `SpriteOptionNone`, the palette shift/mask and `TDMAClock`.
11. **Status line now states the real blocker,** the executable code limit, and that no ZPU exists. Refresh this line in the final follow-up.
12. **Fixed missing spaces before digits throughout.** Examples: "unsigned8", "the512-byte", "at256". These were in the files as supplied.

**`examples/zvb/README.md`**

13. Added `CRC32.PAS` to the table, plus a File checksums section with sample output.
14. Moved build steps into a Building section. They no longer tell readers to install a unit that doesn't exist yet.
15. Rewrote Snake in player terms and added the start tone and the win condition, which the old text left out.
16. Repositioned Graphics cleanup as the model for what any ZVB program must do, including the failed-setup path. Replaced "all programs need the display clock" with an accurate statement: the graphics demos do, and the vblank waits are why.

**`examples/zvb/CREDITS.md`**

17. Retitled to cover all the examples and labelled **Changes** and **Redistribution** explicitly. All authors, license links, the SDK commit and the adaptation note are preserved.
18. The redistribution note now covers saved executables, which embed the font. The old wording could be read as covering only asset files.
19. Corrected "original asset hashes." `assets.json` hashes the SDK files, not the OpenGameArt originals.
20. Added the `CRC32.PAS` adaptation note, taken from that file's own header. Licensing evidence is unchanged.

## Technical inconsistencies (Codex to decide)

1. **`SPRITES.PAS` never clears layer 1.** `GfxInitialize` doesn't either, so stale values, including text-mode attribute bytes, could display as garbage tiles. Please fix or disprove this; Snake and the first-program example both clear it.
2. **Bitmap modes get `BitsPerPixel = 4`.** `(Mode and 2) <> 0` is true for modes 2 and 3, and I expect the SDK's bitmap modes to be 8bpp. That would make packed loads reject them. Please check against the SDK.
3. **4bpp layer 1 is unreachable.** `GfxTilemapLoad` ignores Layer and `GfxInitialize` doesn't clear that map. If it holds per-tile attributes, stale values remain and tile bit 9 can't be used on maps. The guide only states the Layer behavior; it may need a known-limitation note.
4. **`TDMADescriptor` may hide the clock byte.** It has `Reserved[0..2]`, while `TDMAClock` is declared but unused. If the SDK places clock at offset 9, zeroing it should be confirmed as valid. The guide currently says "no routine takes it yet."
5. **Non-`Nil` `GfxTilesetLoad` options have no example.** No code passes a pointer to a record, and I couldn't confirm that `Addr(Options)` is compatible with `PGfxTilesetOptions`. I gave no syntax for this. If it works, add one line.
6. **`DMADescLast` chaining.** My "keep a whole chain within one page" advice assumes consecutive descriptors. Confirm, or correct the wording.
7. **Possible compile issues in the demos:**
   - `program Sprites` shares a name with `var Sprites`.
   - `ZVBCHECK.PAS` calls `Delay` without `CrtInit`.

**Please verify these claims I added or tightened:**
- `GfxFailure` is never returned in the current code.
- 640×480 modes show 40×30 tiles.
- Layer 1 draws over layer 0.
- Tile 15 is transparent in Snake.
- `SoundSampleTable` isn't valid for voice volume.
- `SoundSetChannels(SoundSampleTable, SoundSampleTable)` alone routes sample audio.
- `Length` can exceed 256 at practical sample rates. Byte-by-byte `WritePeripheral` feeding may underrun at low dividers, and that throughput is unmeasured.
- An empty file gives the expected CRC.
- `$CBF43926` is the output for `123456789` via `CRC32.PAS`.
- The physical VRAM table is usable for DMA **writes**; only palette reads are demonstrated.

**Please compile "A first program."** It is new, and I'd rather it were tested than trusted.

## Recommendations outside my files

- **`SNAKE.PAS`:** there is no `Randomize`, so apples may follow the same sequence on every run if the generator's seed is fixed. Add it if the runtime provides one, keeping `--demo` deterministic.
- **`SPRITES.PAS` closing line:** it says "press any key to exit" after the program has already exited. Change it to past tense or print it at start.
- **`SNAKE.PAS` closing line:** it prints the controls after the game ends. A final score would end the game better.
- **Long term, `SaveVideo`/`RestoreVideo`:** the first program depends on a demo include that needs `Crt`. A supported restore path would be cleaner, but the code limit may rule it out for now.

**Unresolved:** the acceptance evidence, final filenames/sizes and RAM measurements still have to come from your final follow-up.

### Codex reconciliation

Accepted the substantive documentation edits1–20: the routine tables, numeric
units, examples, explicit initialization/cleanup, draft status and redistribution
credits improve use without changing the API. Reviewed the applied changes in
all three files. Acceptance tests and artifact sizes remain pending.

Technical dispositions:1 accepted, sprites now clears visible layer1 before
showing the scene.2 checked against SDK sdcc/gfx/core.c: its expression also gives
4bpp for bitmap modes2–3, so retain that public behavior and test/document it.
3 accepted: C tilemap/load.c always selects the requested layer, so correct our
Pascal loader to allow layer1 in4bpp (attributes).4 checked include/zvb_dma.h:
bytes9–11 are reserved, not a clock field. Keep the record and zero padding.
5 add a non-Nil options fixture and documentation after native verification.
6 emulator DMA fetches9 bytes plus3 padding then advances to the next descriptor;
contiguous physical12-byte descriptors are confirmed, with chain tests pending.
7 rename program SpriteDemo for clarity; Delay has no CrtInit precondition
(existing native CRT tests call it independently). Native demo compilation pending.

Recommendations: accepted Randomize only for interactive Snake, keep --demo
repeatable; use a final score and a past-tense sprites exit line. Defer moving
SaveVideo/RestoreVideo into the public API: that was not part of the approved
interface, and the include exposes the restoration policy explicitly.

Tightened technical claims remain queued for native acceptance, including the
new FirstBar program, sample/CRC behavior, physical VRAM DMA and empty checksums.
No full-unit or physical-board acceptance is claimed.

Final technical reconciliation: the unit and native clients pass, with exact
artifacts and reports in zvb-library-evidence.json. Palette read-back is a45th
documented Pascal extension; shared cleanup uses it without DMA. CRC32 uses
three supported Zeal syscall leaves for raw file lengths; typed-file headers
and128-byte untyped records are unsuitable here. Memory/Com-file Snake COMs
are identical and complete180 observed frames. Packaging verifies stock units
and companions beside ZPL, with recovery. No board display/audio qualification.

Snake fitting deliberately removes optional pause and alphabet banners. It
keeps numeric scoring, arrow/WASD controls, R restart, Q/Esc exit, sound and
original snake/number assets, with required font credit. Uniform green uses an
original palette color. It has395 static bytes of margin, now documented. The
recommendations to expose general save/restore API remain deferred: only the
small palette-read operation belongs in the unit; cleanup policy stays local.
Interactive Randomize, deterministic demo mode and useful final score remain.

## Opus turn 20261107T112119-6903a9b4

Mode: creative; read-only

This was a read-only follow-up, so I made no edits. The reconciled guide is creatively sound: it is organized for lookup, the units are explicit, and initialization and cleanup are honest. All 45 routines are present and reachable from the reference tables. I counted 17 graphics, 8 sprite, 3 CRC, 10 sound and 7 DMA routines.

It is not yet technically ready to publish. One required credit statement is now wrong, two sections describe code that changed, and the stock filenames and the program size limit are missing. I have checked the documentation against the supplied source and evidence only. I am not inferring any physical-board verification.

## Must fix before release

1. **`CREDITS.md`, Project code, contradicts the code.** It says `CRC32.PAS` adapts the SDK utility "to Pascal typed files and `ParamStr`." The program now deliberately avoids typed files and uses three raw syscall leaves. Change it to match the README: "to Pascal, `ParamStr` and three local syscall wrappers for raw bytes." This is a provenance statement, so it must be accurate.

2. **`CREDITS.md`, Changes, says Snake still reuses the letters.** It says "This demo reuses that SDK's snake tiles, letters, numbers and palette values." Snake now embeds only the snake tiles, the numerals and the palettes. Suggested wording: "Snake embeds the snake tiles, numerals and palettes. `LETTERS.INC` keeps the alphabet for other programs but is not compiled into Snake." The font credit is still required, because the numerals come from JROB774's font.

3. **`zvb-library.md`, DMA, cites `VIDEO.INC` for code it no longer contains.**
   - "`VIDEO.INC` and `ZVBCHECK.PAS` show the loop" is now false. `VIDEO.INC` uses `GfxPaletteRead`, so only `ZVBCHECK.PAS` shows the page-splitting loop.
   - "VRAM has fixed physical addresses, as `VIDEO.INC` uses for its palette backup" is also false.
   - No code now exercises the physical VRAM address table. The `$100E00` palette read was its only evidence; the other rows are derived from the page numbers 64 and 68.
   - Either add a native DMA-to-VRAM assertion, or mark the table "derived from the VRAM page layout; not exercised by tests."

4. **`zvb-library.md`, the compilation paragraph is in the wrong section.** The Memory and Com-file paragraph ("The larger Snake demo uses the compiler's Com-file option…") sits inside *Graphics setup and palette*. Move it to *Installation and compilation*. Also state one rule: either mode works, and Com-file mode stages generated code on disk. The README says "Select O → C → Q, then compile" but never says how to save the COM from Memory mode, or when to prefer either mode. Give one complete procedure.

5. **Stock filenames are never named in the reference.** *Installation and compilation* says only "the stock ZPU files beside `zpascal.zpl`." The packaging evidence installs these files to `T:/lib/`:
   - `zpascal.zpl`
   - `CRT.ZPU` and `CRT.PAS`
   - `ZVB.ZPU`
   - `ZVB.LIC` and `ZVB.NTC`

   List them. The README also says "with the ZVB license and notice," but on the target those are the 8.3 names `ZVB.LIC` and `ZVB.NTC`, so name them.

6. **The general program-size limit is missing.** The reference gives Snake's 395-byte static margin, but not the limit itself. A programmer cannot tell how close their own program is, or what error they will see when they exceed it. State the boundary that `static_address_margin_bytes` is measured against, and the diagnostic.

7. **Redistribution obligations for programs that embed ZVB code.** This is a question for the legal owner, not a wording fix. Every saved program contains Apache-2.0 ZVB code. If `ZVB-NOTICE.txt` is an Apache §4(d) NOTICE file, anyone distributing their own COM must carry its attributions. The reference links the notice but never tells programmers whether they must ship it. Decide that, then add one sentence under *Installation and compilation*.

## Should fix

8. **Bitmap modes are 4bpp in the context.** Your disposition kept the SDK behavior and said it would be documented, but it isn't. Under *Graphics setup*, add: "`GfxModeBitmap256`/`GfxModeBitmap320` set `BitsPerPixel` to 4, as the SDK does. The 4bpp limits on tile indexes and loaders therefore apply."

9. **Stale layer 1 matters more in 4bpp.** *Tilemaps* now correctly calls layer 1 the attribute map in 4bpp. Since `GfxInitialize` leaves it alone, a 4bpp program inherits stale palette and tile-bit-9 attributes. Add: "In 4bpp, clear layer 1 too, or tiles may use the wrong palette or upper tile bank." If you can, give the attribute bit layout instead of the parenthetical.

10. **There is still no example of passing `Options` to `GfxTilesetLoad`.** `OPTIONS.PAS` passes, so copy its exact syntax (`Addr(Options)` or `@Options`) into one line under *Tilesets and compression*. Readers cannot infer it.

11. **Sample-rate limit.** Samples are fed byte by byte through `WritePeripheral`, and the tested divider changed from 20 to 7. The reference should say this is the tested rate, and that lower dividers (faster rates) are unmeasured and may underrun silently.

12. **DMA chaining is unverified.** The guide gives page advice for chains ("keep a whole chain within one page"), but your disposition said chain tests were pending. If `DMA.PAS` doesn't cover chaining, mark that sentence as untested.

13. **The measured-sizes paragraph is written for maintainers.**
    - "The compiler image remains 277,338 bytes" is `zpascal.zpl`'s size. Name it, and drop "remains."
    - Readers can't interpret "10 cold / 15 warm startup pages" or "two 16 KB pages of metadata." Say what each costs the programmer, such as extra RAM while compiling a program that uses ZVB, or move these figures to the evidence file.
    - "Native-authored" means "compiled by the Zeal-native compiler." Say so.

14. **The Real-runtime sentence is opaque.** "Its standalone Inline wrappers retain the compiler's conservative Real runtime requirement" should say what it means in practice. For example: "A program that declares its own `Inline` procedures also links the Real runtime, which adds code. Calling ZVB routines does not."

15. **Explain FirstBar's `if ParamCount = 0 then`.** In a teaching example this is unexplained. Add a comment such as `{ Any argument skips the key wait, for automated tests. }`. Also confirm the doc listing is byte-identical to `tests/native/zvb/FIRSTBAR.PAS`, apart from the include path, since the guide presents it as tested.

16. **The Sprites figures need a qualifier.** The size table and the status line ("tests pass for … its demos") include Sprites. Its clean standalone repeat is still running, so label the row "from the previous group run" until the repeat passes.

17. **README *Building* mixes users and maintainers.**
    - "Run from the worktree" should be "from the repository root."
    - Move the rebuild commands under the existing maintainer paragraph.
    - Check the target name `zzvbtest_bin`; the double `z` looks like a typo.
    - Check that `--skip-hardware` means what readers will assume.

## Cosmetic

18. **"Final reviews are in progress" appears twice in the reference.** Keep one status line, so the release update touches only one place.

19. **Formatting:**
    - The size table has unspaced cells (`|11,131|28,082|`).
    - One overlong line in *Buffers*.
    - Ragged breaks around "tile 65 (`'A'`)."
    - Label the format table's "Decoded bytes" column as 8bpp output.

20. **Snake's exit credit** (out of my scope): the printed line reads `CC BY3.0` and has no license URL. CC BY 3.0 asks for the license URI where practicable. It costs a few bytes against the 395-byte margin, so this is Codex's call.

## Verified as accurate

- The 45-routine count.
- The `GfxPaletteRead` description and its page-0 rule.
- The 1-bit and 2-bit 4bpp output lengths (4 × Size and 2 × Size).
- The `$4100` letters example and `$3000` numerals, which both match tile number = character code.
- CRC32's output format and its fixed exit behavior: no exit status.
- Snake's controls, icons, field color, sound dividers and 256-segment win, all as described.
- `RestoreVideo` no longer mutes sound, and the README no longer claims it does.

## Readiness

| | Status |
| --- | --- |
| **Creative suitability** | Ready, once items 1–6 are corrected. |
| **Technical readiness** | Software and emulator only. Open: Sprites standalone repeat, DMA chaining and physical VRAM DMA tests, and the redistribution question in item 7. Physical display and audio, including palette read-back on real hardware, remain unqualified. |

### Codex reconciliation

Accepted and corrected1–2: provenance now describes raw syscalls and only the
snake/numeric assets used by the game; the unused alphabet remains available.
Accepted3's stale VIDEO references; replaced them with GFX.PAS and ZVBCHECK.
Adapted the claimed VRAM coverage gap: GFX.PAS already reads palette, both maps,
sprites and tiles through DMA at the documented physical addresses and passes
in the final140-assertion group. This is emulator coverage, not a board test.

Accepted4–6: moved compilation to Installation, gave complete S/O/C/Q/C and R
procedures, distinguished Memory execution from Com-file publication, listed all
stock filenames including ZVB.LIC/NTC, and documented the F400 boundary,2KB stack
and error99. Accepted7 as a distribution rule: carry the existing Apache license
and notice with programs embedding ZVB; font programs also keep CC BY credits.
No new license or copyright disposition is invented.

Accepted8–11: SDK bitmap context behavior, exact4bpp attribute bits and stale
layer1 advice, tested Ptr(Addr(Options)) syntax, and tested sample divider7 with
unmeasured faster rates are explicit. Adapted12: DMA.PAS already has a chain;
its alignment condition is now an unconditional Require so coverage cannot skip.
The final chain client passes32 assertions. Chaining is marked emulator-only,
and zero-length descriptors on unqualified hardware are discouraged.

Accepted13–15: named zpascal.zpl and explained compile-time metadata/cache RAM;
clarified Real instructions and the fixed runtime-prefix extent; explained the
FirstBar argument outside the listing to preserve its exact source fixture.
Verified the listing is byte-identical after VIDEO.INC expansion. Accepted16:
clean Sprite repeat now passes31 assertions, independently of obsolete Snake
failures. Accepted17: user and maintainer instructions are separate; the actual
CMake target is zzvbtest_bin, and skip-hardware excludes the foundation suite,
not ZVB emulation. Accepted18–19 formatting/status corrections.

Accepted20: Snake prints both the full font source URL and CC BY3.0 license URI.
Final Memory36/Com-file37 assertions pass after the credit edit; both files are
identical45884-byte COMs, complete180 frames, and retain196 static bytes.

This follow-up was read-only. These targeted corrections were applied afterward,
so its input fingerprint is stale; no extra creative review or formal finalization
is claimed. The final code follow-up receives the corrected technical text and
verification. Physical display/audio remains unqualified.

Post-code-follow-up corrections: FirstBar fixture generation and stale checks
now share a producer; Score initializes before setup. Full printed font title,
adaptation, source URL and CC BY license URI are present. Both final compile
paths pass (36/37 assertions) and produce identical45909-byte COMs, with171
static bytes remaining. All generated asset bytes and decoded original pixels
were reverified. Earlier395/196-byte figures above are chronological; current
docs and the evidence JSON use171. These changes are tested but not subject to
an extra creative/code review; the completed turns are not fresh approval.
