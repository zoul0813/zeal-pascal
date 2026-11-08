# ZVB demos

**Development checkpoint: native Unit authoring and Snake's saved demo pass.
Interactive pixels and behavior pass; the original wipe is still slower than C
(2.44 s versus 1.16 s). Two Opus review turns are recorded and Codex verified the follow-up corrections. Physical hardware is unqualified.**

Four small programs for the Pascal ZVB unit. They need a Zeal Video Board; the
graphics demos also need its display clock running, because their vblank waits
block until it ticks.

| Program | What it shows | Controls |
| --- | --- | --- |
| `SNAKE.PAS` | Original title sprites, tilemaps, LZ assets, vblank timing | Arrow keys or WASD move; fresh key after game over replays; Q or Esc exit |
| `SPRITES.PAS` | RGB565 palette, 2-bit tiles, sprite arrays, flips, position updates | Any key exits |
| `CRC32.PAS` | Hardware CRC32 over files, fed in chunks | File names on the command line |
| `ZVBCHECK.PAS` | CRC32 interleaved with sound, page-aware DMA copying, tone and sample playback | Runs to completion |

## Building

Install `ZVB.ZPU` and `CRT.ZPU` beside `zpascal.zpl`, or on the compiler's
unit search path. Save the demo's Pascal source with **S**, select
**O → C (Com-file) → Q**, then press **C** to compile. The COM is written beside
the source. Run it from ZShell, or press **R** in Com-file mode to compile and run.

Memory mode (**O → M → Q**) compiles and runs from RAM without a disk workspace.
To publish a COM after Memory testing, select Com-file and compile again. The latest Snake source is qualified through Com-file mode. Its asset includes
exceed the compiler RAM available at the Memory-mode peak; that path is not
qualified for Snake.

The public demos embed helpers and assets in generated include sections.
Snake additionally needs `NUMBERS.INC` and `SNAKEFONT.INC` beside its source
at compile time to keep its root source within one source page. Saved executables embed their selected library
code and assets; they need no PAS, INC or ZPU files at run time. Redistribute
`ZVB.LIC` and `ZVB.NTC` with programs containing ZVB code. Snake also carries its
font attribution and license links; keep the [asset credits](CREDITS.md).

## Maintaining the stock unit

From the repository root, use the existing Zeal development environment:

```sh
zde --no-tty cmake build --target zpascal_bin zzvbtest_bin -j4
python3 tools/test-zvb.py --skip-hardware --output-library build/ZVB.ZPU
python3 tools/test-zvb.py --skip-hardware --prebuilt build/ZVB.ZPU --clients
python3 tools/package-tf.py
```

`zzvbtest_bin` is the intentional native test-harness target. The authoring test
compiles the current Pascal unit on the emulated Zeal and installs `build/ZVB.ZPU`
only after its checks pass. The client command compiles and executes all examples.
`--skip-hardware` skips the separate compiler hardware-access foundation suite;
it does not disable the emulated ZVB or certify a physical board. Run that
foundation suite separately with `python3 tools/test-zvb.py --hardware-only`.

Packaging installs `CRT.ZPU`, `CRT.PAS`, `ZVB.ZPU`, `ZVB.LIC` and `ZVB.NTC`
beside `T:/lib/zpascal.zpl`. The last two are the ZVB license and notice, using
8.3 filenames.

Regenerate asset includes with
`python3 tools/generate-zvb-assets.py --sdk /path/to/Zeal-VideoBoard-SDK`, then
refresh the public demo sources with `python3 tools/zvb-source.py --demos`.
Edit the authoritative INC files rather than generated include sections.

## Snake

The opening matches the SDK C demo: six sprites spelling SNAKE followed by the
snake head bounce into place, one at a time, over the original checkerboard.
After the title, play starts automatically. The dark bottom row has the original
`SCORE:000` at columns 10–18. No menu, waiting prompt, win icon or lose icon is added.

Arrow keys, WASD or numeric keypad 8/2/4/6 steer. Reversals against the selected direction are ignored;
like C, two valid turns in separate frames before a move can produce a U-turn.
Movement takes 20 vblanks initially, falling to 10 as speed increases every eight
scored apples. Eating relocates the fruit immediately; growth and score follow
on the next move. Length caps at 255 and score at 253 while play continues.

On collision, apples wipe across the playfield. A fresh key replays without
repeating the title. Unlike the original's broken replay, Pascal restores the
board and palette and uses an iterative loop. Esc or Q returns to ZShell.
`-c` enables the original SNES controller protocol on the PIO user port;
replay still requires the keyboard. Physical-controller testing is separate.

This version is silent, matching C; the earlier added sound effects are removed. `--demo` runs the title then 180 finite gameplay frames.
The snake tiles, selected title/score glyphs, numbers and palettes are the original SDK pixels,
embedded in one COM using the verified SDK LZ codec (2,139 compressed bytes).
The checkerboard's two colored tiles are generated in code, as in C.
CPU startup before the title now takes 1.80 s instead of 29.66 s; C takes 0.33 s.
The library decodes RLE/LZ chunks in a bounded Z80 stream rather than a per-pixel
Pascal loop. These timings exclude COM disk loading and the title animation.
The raw-asset experiments still fail compiler error99, so the compressed asset
build remains the qualified version. This is a compiler resource issue, not a
ZVB requirement for compressed tiles. See [startup evidence](../../docs/snake-startup-evidence.json). No asset
file is opened while playing. See [the fidelity contract](../../docs/snake-parity.md)
for sequencing, verification and deliberate reliability differences.

See [CREDITS.md](CREDITS.md) for the Clear_code and JROB774 asset credits and
licenses. **Redistributing the game, including a saved executable, requires the
font's CC BY 3.0 credit.** `assets.json` binds the reused bytes to their source
hashes.

## Graphics cleanup

`VIDEO.INC` shows what a ZVB program must do for itself, because the unit has no
cleanup routine. `SaveVideo` runs before the mode changes and records the 512-byte
palette (with `GfxPaletteRead`), video mode, scroll registers, screen enable and peripheral
selection. `RestoreVideo` puts those values back, both on a normal
exit and after a failed setup.

Returning to text mode clears the display. The previous screen contents and prior
sound settings are not restored. Use the demos' exit controls: killing the program
or resetting the machine skips this cleanup.

Both graphics demos accept `--demo` as their first program argument, for automated
acceptance. This finite mode exits after 180 frames, and Snake also restarts at
frame 100. Interactive play is the default.

## File checksums

`CRC32.PAS` prints the CRC32 of each named file, in eight hexadecimal digits:

```text
CRC32 DIGITS.TXT
CBF43926    DIGITS.TXT
```

Here `DIGITS.TXT` holds `123456789`, the standard check sequence. The program
resets the CRC for each file and feeds it 1 KB at a time. It prints an error
and stops at the first file it cannot open, read or close. The current Pascal
`Halt` has no exit-status argument. The utility changes no video or sound state. It adapts the SDK's `examples/crc32` to Pascal and `ParamStr`. Three local `Inline` wrappers call the supported
Zeal open/read/close syscalls for raw bytes. Pascal typed files have a header;
untyped files use 128-byte records, so neither existing form reads arbitrary
raw file lengths. These wrappers link no C SDK code.

## Hardware check

`ZVBCHECK.PAS` checks three record sizes, the CRC32 result `$CBF43926` for
`123456789`, and all 37 bytes of a DMA copy. It then plays a triangle tone and
unsigned 8-bit samples and reports completion. Output is muted on completion or
assertion failure. A success message confirms software control flow; judging the
actual waveform takes an audio output device.

See [the ZVB reference](../../docs/zvb-library.md) for units, ranges and buffer
requirements, and for the distinctions between byte lengths, tile coordinates,
pixel coordinates and physical addresses.

## Compact runtime checkpoint

The current Snake Com-file build is 35,844 bytes: 6,042 runtime, 957 globals
and 28,845 code. The runtime's omitted Real code leaves no padded reservation.
The final globals/code addresses are assigned at link time. See the
[runtime compaction notes](../../docs/runtime-compaction-notes.md).

Snake remains one self-contained COM and completes its saved 180-frame demo
without PAS, INC or ZPU files. The current [parity evidence](../../docs/snake-parity-evidence.json)
records title trajectory/cadence, actual VRAM pixels and palette, raw input,
growth, score, collisions, replay and OS exit. Its overall result remains failed
because the wipe is slower than C; the same placement and 4 ms delay sequence
is retained. Current compiler overhead remains visible rather than changing
that gameplay sequence to conceal it. Memory-mode compilation is not qualified
for this larger source.

Precompiled demos are in `build/zvb/`; keep the supplied licenses and asset
credits when redistributing them. Loading speed and physical hardware remain
separate acceptance questions.

After rebuilding the compiler, maintainers can check final-link bank residency
and fault recovery with `python3 tools/check-unit-banks.py` and
`python3 tools/test-unit-phase.py`. See the
[runtime compaction notes](../../docs/runtime-compaction-notes.md) for the final
layout and the distinction between measured addresses and linker contracts.
