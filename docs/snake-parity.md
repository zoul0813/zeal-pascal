# Snake C/Pascal fidelity contract

Reference: local Zeal-VideoBoard-SDK commit 467e224, examples/snake, with unchanged source/assets and its existing 19,315-byte native binary (exact size/hash in the reference record). The original screenshot shows a 320x240 field: 20x14 checkerboard tiles and one dark score row. This task changes the Pascal demo, ZVB implementation paths, tests, assets and documentation. It does not modify the SDK, emulator or public ZVB API. A compiler staging-limit correction is documented in code-staging.md.

Target presentation and normal gameplay:

- Load the same snake/numeral/alphabet pixels and RGB565 palette values. Embed the eight displayed original glyphs at their ASCII tile IDs using the existing verified LZ codec; unused slots are transparent; keep one self-contained COM. Score uses actual ASCII glyph tile IDs at column10,row14.
- Before play, drop and bounce six sprites, S N A K E and tile0 head, sequentially. X positions128,144,160,176,192,208. Use the C fixed-point trajectory: velocity+=36 each vblank, position+=velocity, floor64, halve/reverse velocity on bounce, stop when impact velocity<=360. Hide them after the title and pause100ms. No invented menu or prompt.
- Automatic start: bare board throughout the title and 100 ms pause, then SCORE:000. Initial state is head (10,7), tail (9,7), but neither snake nor fruit is drawn until the first movement at vblank 20. Keyboard direction is updated each input frame, rejecting the opposite of the current selected direction. Multiple turns before a move follow the C behavior, including its possible U-turn. Keep buffered title keys. Support original -c controller mode; WASD, numeric keypad 8/2/4/6 and Esc/Q are conveniences.
- A fruit marks pending growth. Length/score increase on the following movement update; speed increases every8 scored apples, capped10. Move every20-speed vblanks. Cap length 255 and score 253, continuing play without further growth/score. Commit pending score even on a fatal update. No invented win or lose icon.
- Render identical head/body/corner/tail tile IDs. Draw newly moved snake, then the fruit. A collision stops before drawing the failed movement, preserving the last visible snake until the game-over wipe.
- Game over changes the same palette bytes as the observed C binary, then fills280 foreground cells with apple tile7, left-to-right/top-to-bottom with4ms pauses. Flush pre-existing keys; a fresh key starts another game. Title plays only at application startup. Restore a clean board/palette on replay and avoid the C recursive-restart stack leak.
- The reference-faithful version is silent because the C reference is silent; remove the added sound-effects path. --demo remains an automated finite path and exercises title/gameplay. Existing normal OS cleanup/exit remain.

The C source has apparent defects: odd-size death-palette request (observed visible result changes color 6 to AE 00 while color 7 stays DF 5B; whether this is a loader or hardware commit effect is unproven), no board/palette restoration before recursive replay, and possible fruit placement on the initial body. Observe the shipped C binary and document any deliberate reliability exceptions; do not silently call intent inferred from comments an observed result. Fruit uses the same refresh-register algorithm and its biased distribution; exact random sequences need not match. Bounded retries followed by a free-cell scan prevent hangs on crowded boards. Replay consumes its triggering raw input batch, including release events, rather than steering with that key. No screen blanking on replay. Audio registers are untouched.

Validation: hash-bound C/reference evidence; native title trajectory and sprite-layout checks; VRAM/palette/font comparisons; automatic start, direction buffering, collision and delayed-growth/speed checks; fresh-key replay; silent default and exit; detached saved COM execution. Confirm binary fits without restoring page padding. Physical-controller and audio-waveform acceptance are separate from emulator checks.


Compilation uses NUMBERS.INC and SNAKEFONT.INC as source includes so the root
stays within one source page. They are required beside SNAKE.PAS at compile
time. The saved COM embeds both streams and opens no asset files at runtime.
The public ZVB API and artifact format are unchanged. The compiler separates temporary code capacity from final physical RAM checks; see code-staging.md.

Replay accepts any fresh raw event, including a key release after the FIFO
flush, as C does. After reset, movement waits for subsequent gameplay vblanks.
The frame-20 initial appearance, normal turn/growth rules and visual tile IDs
are the fidelity targets; exact random sequences and long-snake CPU slowdown
are not promised. Physical SNES controller acceptance remains separate.

## Measured checkpoint

The saved 36,798-byte COM passes the pixel, title cadence, controls, growth,
collision, score, replay and actual successful OS-exit checks. It also completes
its finite demo after all source and Unit files are removed. These checks use
header-derived globals and instruction signatures from the linked ZVB methods.

Full performance parity remains incomplete. The original per-cell placement
and `Delay(4)` sequence is retained. The measured wipe is 24,385,215 T-states
(2.439 s at 10 MHz), versus C's 11,584,421 (1.158 s). Of the Pascal time,
9,583,840 T-states are in the 280 public tile calls, 11,393,760 in kernel sleep
syscalls, and 3,407,615 in other caller/loop work. The direct tile leaf takes
338 T-states including a test RET; most public-call cost lies outside that leaf.
The overall parity test remains failed, with `behavior_passed` recorded
separately. Timing is not hidden by batching the wipe or shortening its delays.

The ZVB Unit follows the SDK's direct stores, LDIR copy/fill paths, OTIR CRC
chunks, single-selection sound operations and direct DMA startup. Pascal
validation, interrupt protection and preserved voice holds remain deliberate
API contracts. See [the reference](zvb-library.md#sdk-implementation-paths).
