# ZVB demos

**Development build: native emulator tests pass for the unit and demos.
Physical display and audio verification remains outstanding.**

Four small programs for the Pascal ZVB unit. They need a Zeal Video Board; the
graphics demos also need its display clock running, because their vblank waits
block until it ticks.

| Program | What it shows | Controls |
| --- | --- | --- |
| `SNAKE.PAS` | Tilemaps, LZ asset loading, sound effects, vblank timing | Arrow keys or WASD move; R restart; Q or Esc exit |
| `SPRITES.PAS` | RGB565 palette, 2-bit tiles, sprite arrays, flips, position updates | Any key exits |
| `CRC32.PAS` | Hardware CRC32 over files, fed in chunks | File names on the command line |
| `ZVBCHECK.PAS` | CRC32 interleaved with sound, page-aware DMA copying, tone and sample playback | Runs to completion |

## Building

Install `ZVB.ZPU` and `CRT.ZPU` beside `zpascal.zpl`, or on the compiler's
unit search path. Save the demo's Pascal source with **S**, select
**O → C (Com-file) → Q**, then press **C** to compile. The COM is written beside
the source. Run it from ZShell, or press **R** in Com-file mode to compile and run.

Memory mode (**O → M → Q**) compiles and runs from RAM without a disk workspace.
To publish a COM after Memory testing, select Com-file and compile again. The
qualification harness confirms identical COM bytes from both compiler paths.

The public demo sources are standalone: generated include sections already
contain the helpers and assets. Saved executables embed their selected library
code and assets; they need no PAS, INC or ZPU files at run time. Redistribute
`ZVB.LIC` and `ZVB.NTC` with programs containing ZVB code. Snake also carries its
font attribution and license links; keep the [asset credits](CREDITS.md).

## Maintaining the stock unit

From the repository root, use the existing Zeal development environment:

```sh
zde --no-tty cmake --target zpascal_bin zzvbtest_bin --parallel 6
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

Eat apples; avoid the walls and your own body. Each apple grows the snake and adds
a point. Reversing straight into your next segment is ignored. Fill the snake to
256 segments and you win. R starts a new game at any time. A tail icon beside
the four-digit score marks game over; an apple icon marks a win. Movement stops
in either state. The field uses a uniform green from the original palette.

A short tone marks each new game, a higher square-wave tone each apple, and a low
noise burst a collision. Each effect is released and held again by frame count,
so sound never blocks movement.

The snake tiles, numbers and palette data come from the C SDK's Snake demo. The
gameplay is written in Pascal, with added sound, restart and normal exit.
The SDK LZ codec compresses each tile independently. The game embeds only the
snake tiles and numeric font, totaling 1,404 compressed bytes; decoding reproduces their
original pixels exactly. The alphabet include remains available for other demos,
but Snake uses icons and a numeric score to fit its executable address space.
Tone dividers for its three fixed effects are precomputed. No asset file is opened while playing.
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
