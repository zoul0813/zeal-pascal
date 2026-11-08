# Review: snake-parity-creative-v1

## Opus turn 20261108T014453-bd721c95

Mode: creative; read-only

# Snake parity: creative review of the fidelity contract (initial pass, read-only)

No files were edited. All findings below come from reading the supplied C source (`title.c`, `snake.c`, `snake.h`, `controller.c`, `palette.c`) against `docs/snake-parity.md` and the current Pascal source. Anything marked **[verify]** is my derivation from source, not an observation of the shipped binary.

## Overall judgment

The contract has the right intent: faithful reproduction, no redesign, sound off by default, and a documented exception for one defect. The weakness is that it describes the C demo by its *rules*. Several of the most player-visible traits come from the C program's *sequencing*, and those are missing:

- what is on screen during and after the title;
- when the snake first appears;
- input semantics;
- when the score updates;
- the fruit's spatial distribution.

As written, a careful implementation could pass every listed check and still feel different in the first ten seconds. The current Pascal game diverges further still, which is expected at this stage.

## Numbered findings and recommendations

### 1. Opening sequence: the board is bare until the first move (`docs/snake-parity.md`, title and start bullets)

**What the C program does, in order:**
1. `init()` draws only the checkerboard and the dark row, then enables the screen.
2. The title bounces over an empty board: no snake, no apple, no "SCORE:" text.
3. The sprites are hidden, followed by `msleep(100)` on a bare board.
4. `play()` calls `reset()` (data only) and `update_stat()`, so `SCORE:000` appears.
5. Nothing else is drawn until the first `update()`/`draw()` on vblank 20.
6. The snake then pops in already moved: head tile 1 at (11,7), tail tile 11 at (10,7), with the apple drawn in the same frame.

The snake is never visible at (10,7)/(9,7).

**Where the contract and Pascal differ:** The contract says "two-cell snake at (10,7)/(9,7)", which describes data, not what the player sees. Pascal's `ResetGame` draws the snake, apple and score immediately, and `Initialize` calls it before any title.

**Recommendation:** Specify the visible sequence explicitly:
- bare board during the title and the 100 ms pause;
- score appears at play start;
- snake and apple first appear together on the first move.

This is the player's first impression, so I rank it top priority.

### 2. Title trajectory: give Codex a reference trace (title bullet)

The contract's prose is correct but leaves out details that change frame counts:
- `y` is `uint16` and `velocity` is `int16`.
- The impact test is `whole > 64`, i.e. `y ≥ 16640`, not `≥ 64`.
- On impact, `y` snaps to `64<<8`.
- On the stopping frame there is **no** `set_y`; the sprite holds its previous `whole`, then `title_play` sets it to 64 outside vblank.
- Each letter costs one extra frame, the initial `sprite_render` at y=0 (hidden).
- Letters drop strictly in sequence; earlier letters stay at 64.

My derived model, per letter **[verify]**:

| Segment | Frames | Notes |
|---|---|---|
| Hidden render frame | 1 | Sprite rendered at y=0 |
| Fall | 30 | Impact velocity 1080, reverses to −540 |
| Bounce 1 | 30 | Rises about 15 px (min whole 49); impact 540, reverses to −270 |
| Bounce 2 | 15 | Rises about 4 px; impact 270 ≤ 360, stops |
| **Total** | **76** | |

Over six letters this is about 456 vblanks, roughly 7.6 s of title.

**Recommendation:**
- Have native evidence log each sprite's Y per vblank and compare against a host model of the exact C arithmetic.
- Also compare total title duration against the C binary in the emulator.

**Coordinates:** The 16-pixel sprite offset is confirmed by the layout. Raw X 128–208 centers six 16 px sprites on 320 only if screen X equals raw X minus 16. So "floor 64" is raw sprite Y, which is screen Y 48.

**Hiding:** C hides the sprites at y=0 and Pascal at 480. Both are invisible, so that is fine.

**`title_flip_head`:** It is defined but never called from `snake.c`, so the head stays tile 0 (facing up) with no flip. Do not animate it.

### 3. Input semantics: the contract's rule is not the C rule (control bullet)

**What C does:**
- `input()` drains the FIFO every frame and skips `KB_RELEASED` pairs.
- Within a frame, the first key that passes `check_key` and is not the current direction wins.
- `check_key` rejects reversal against `snake.direction`, which is already overwritten by earlier frames' choices before any move happens.
- Across frames, the **last** accepted choice before a move wins.
- There is no one-turn-per-move limit.

**Consequence:** Moving right, a player who taps UP and then LEFT within one move interval (167–333 ms) gets direction LEFT. That is a reversal: fatal at length ≥ 3, harmless at length 2, where the head moves into the vacated tail cell.

**What the contract and Pascal do instead:**
- The contract ("reject relative to last moved direction; at most one committed turn") silently fixes this, giving first-turn-wins. A quick U-turn becomes "the second tap is ignored."
- Pascal currently rejects against the last moved direction with last-wins, which is a third behavior.

**Creative judgment:** This is core game feel, and quick U-turns are a common snake maneuver, so players will notice. Under "indistinguishable to a player", I recommend replicating C's semantics exactly, including the reversal death. If Codex or the user prefers the fix, record it as a deliberate, player-visible exception, not a parity item. Do not choose it silently.

**Related details:**
- Keys pressed during the title stay buffered and steer on the first play frame in C. Pascal must not flush before play.
- Controller input is read only when the keyboard returned zero bytes that frame.
- Controller priority is Up > Down > Left > Right.

### 4. Score timing on a fatal move (gameplay and collision bullets)

In C, the delayed growth (`length++`, `score++`, speed boost, `update_stat()`) runs inside `update()` **before** the collision check. If the move after eating is fatal, the score still visibly increments, then the game ends with the board undrawn.

**Recommendation:** Add to the contract: "the score updates on the delayed-growth update even when that update is fatal." Also state that eating covers the apple with the head in the same draw, with the new apple drawn last.

### 5. Length cap behavior (gameplay bullet)

At length 255, `just_ate` is never set, so growth, score and speed all freeze while play continues and apples keep respawning.

**Recommendation:** State "score caps at 253 and speed freezes", not only "length 255".

Pascal currently ends the game at 256 and draws an apple icon at (8,14); both are invented.

### 6. Invented game-over icon (missed by the contract)

`Lose` places tail tile 14 at (8,14). The C game has no game-over marker in the score row.

**Recommendation:** Name this explicitly in the contract. "No invented win icon" does not cover the lose icon.

### 7. Death palette: pin the exact expected bytes (game-over bullet)

`colors` is `uint16_t[] = {0xae, 0x73, …}`, so in memory it is `ae 00 73 00 ef 00 …`. `palette_load(…, 3, 6)` copies only 3 bytes **[verify against the binary]**:

| Color | Before | After |
|---|---|---|
| 6 | 0x4B9B | 0x00AE (dark blue) |
| 7 | 0x5BDF | 0x5B73 (low byte only) |

**Recommendation:**
- Keep exactly this, not "greyscale". It is what players saw.
- Wherever tile 7 uses colors 6 and 7, the wipe apples are also recolored. That is part of the look, so keep it.
- Write the expected bytes into the contract so the palette comparison has a concrete oracle.

### 8. Replay: restore exactly the post-title state, with no flash (game-over and replay bullets)

**What C does:** C restarts with every layer-1 cell still an apple and the palette still altered. In the second game the fruit is indistinguishable among the apples, and the tail "eats" a path that reveals the checkerboard.

**My view:** Restoration is the right call, because C's second game is visibly broken. But it is a player-visible deviation and must be documented as one.

**Define "clean" precisely as the first-game play start:**
- original palette;
- bare checkerboard with layer 1 set to tile 15;
- `SCORE:000`;
- snake and apple appearing on vblank 20;
- no title and no 100 ms pause.

**Current Pascal problems:**
- `ResetGame` toggles `GfxEnableScreen(False/True)`. C never blanks on replay, and the blank frame is a visible flicker. Remove the toggle from the replay path.
- The key that triggers a restart must not steer. C consumes that key byte.

**Restart-key detail:** C's wait accepts any byte, including a release code. Holding an arrow through the wipe and releasing it therefore restarts. Pascal's cooked `KeyPressed` probably differs, both in releases and in autorepeat while held. **[verify the Pascal keyboard mode]** A minor deviation here is acceptable if documented.

### 9. Fruit distribution: "random" is not uniform here (defects paragraph)

"Random fruit coordinates need not match" is fine for the *sequence*, but the C *distribution* is a gameplay trait.

**Known bias, from the source:**
- X values 0–11 are twice as likely as 12–19.
- Y values 0–1 are doubled.
- So apples favor the left side.

**Derived, needs empirical confirmation:** The two `ld a,r` reads are a fixed number of M1 cycles apart, with one extra cycle on the `sub` path. Y would then be a deterministic function of X's raw R value, and the fruit could only ever appear in about **32 of 280 cells**, barring an interrupt between the reads. A regular player would sense recurring apple spots.

**Recommendation:**
- Run the C binary in the emulator, log fruit cells across many eats, and confirm or refute the 32-cell set.
- If confirmed, reproduce that set, picking uniformly among its 32 members and excluding body cells. If refuted, at least reproduce the 2:1 left and top bias.

**Reliability exception:** With a 32-cell set, C's `do…while(position_in_snake)` hangs once the snake covers all reachable cells. Pascal's "never hang" fallback should be documented as a reliability exception.

**Initial placement:** Placing the initial fruit on the starting body is invisible as a rule because placement is random, so excluding the body is a safe fix.

### 10. Wipe timing (game-over bullet)

`msleep(4)` × 280 is 1.12 s only nominally. The wall-clock duration depends on the ZOS `msleep` granularity and the per-place mapping overhead.

**Recommendation:** Match the measured duration of the C binary, not the nominal value.

### 11. Performance at long lengths

C's `draw()` redraws the whole snake on every move. Pascal draws incrementally. If a C iteration overruns a frame at length ~200+, the C game actually slows down as it grows.

**Recommendation:** Measure the C move period at high length in the emulator. Mimic it only if it is perceptible.

### 12. Sound off means silent and untouched (sound bullet)

`Initialize` writes the sound registers and volumes unconditionally, and `ResetGame` beeps.

**Recommendation:** Without `--sound`, make no audio writes at all. This avoids any click and leaves the audio state as C does. **[verify]**

### 13. Font and glyph details (asset bullet)

- `ScoreLabel` currently uses glyph IDs 65–69 (A–E) as stand-ins. The title needs S, N, A, K, E, so the full alphabet at its ASCII IDs is required, as the contract says.
- **[verify]** `:` (58) comes from the numerals tileset.
- **[verify]** The C load order (letters, then numbers) and the overlap behavior both produce identical VRAM.
- **[verify]** Pascal's `True` flag matches C's `opacity = 1` for transparent letter backgrounds on the sprites.

### 14. Controller mode

C cannot restart from the controller, because the wait loop reads only stdin.

**Recommendation:** Keep that as the baseline. A controller restart would be an acceptable documented convenience only if it requires a fresh button edge; otherwise a held direction restarts instantly.

## Creative suitability versus technical readiness

**Creative suitability:** The contract's intent is right. It needs items 1, 3, 4, 6 and 8 added before it actually describes the experience the player gets. Item 9 is the one place where "need not match" may be hiding a real feel difference.

**Technical readiness:** Not assessable yet. The current Pascal game is pre-parity: it waits for a key, uses immediate growth, draws the snake early, beeps, has the invented icons, and has no title. All of this awaits the implementation pass.

## Claims requiring verification

| Claim | Item |
|---|---|
| Sprite Y trace and the 76-frames-per-letter count | 2 |
| Death palette results 0x00AE and 0x5B73 in the shipped binary | 7 |
| 32-cell fruit set and the left/top bias | 9 |
| Measured `msleep` wipe duration | 10 |
| Move-period slowdown at high length | 11 |
| Pascal keyboard mode, release events and autorepeat | 8 |
| Opacity flag equivalence and `:` source | 13 |
| No audio writes without `--sound` | 12 |

## Unresolved issues for Codex or the user

1. **Reversal-death input semantics.** I recommend matching C; this needs an explicit decision (item 3).
2. **Replay restoration.** I endorse it, but it must be documented as a player-visible exception (item 8).
3. **Fruit distribution.** Pending emulator evidence (item 9).

### Codex reconciliation

1. Accepted: bare board/title/pause, score-only start, first visible snake on move 20.
2. Accepted trajectory and sequential rendering. Unchanged C binary observed: 450 Y calls (74 trajectory writes plus final floor write per sprite), then six hides. Native Pascal comparison is in progress; do not claim tested parity yet.
3. Accepted actual C per-frame direction changes, including reversal via two turns; buffered title input is retained. Added WASD/Esc/Q remain deliberate conveniences. No one-turn-per-move filter.
4–6. Accepted delayed scoring even before fatal collision, 255 length cap, no invented icons.
7. Adapted to observed binary, rejecting the derived three-byte outcome: native VRAM color 6 becomes AE 00, but color 7 remains DF 5B. The shipped loader rounds the odd count down to two bytes. Pascal requests two bytes. Hash-bound evidence: docs/snake-c-observation.json.
8. Accepted iterative clean replay with restored original palette/bare board, no blanking, no repeated title. Raw triggering input batch is consumed.
9. Reused the exact refresh-register arithmetic/instruction spacing between R reads instead of approximating a guessed 32-cell set. Retry at most 32 candidates, then scan for a free cell. This is a documented crowded-board reliability exception; exact random sequence is not promised.
10. Uses the same supported 4 ms OS delay per row-major apple. Measured duration comparison remains pending native tests.
11. Keeps existing incremental draw rather than emulating unverified high-length slowdown. No claim of cycle-exact behavior at long lengths.
12. Adapted: remove the optional sound path entirely to match the silent original and fit the native compiler's remaining 29 KiB code-staging limit. My earlier intention to preserve --sound is superseded by fidelity and measured build capacity. No audio writes.
13. Original glyph pixel values, palette offset and ASCII IDs retained. Eight glyphs needed by title/SCORE are selected; transparent omitted slots reduce compile footprint. Numbers retain colon 58. All arrays decode independently to expected source pixels.
14. Original -c PIO protocol retained; keyboard replay as original. Physical controller validation remains unperformed.

No creative files were edited by Opus. Actual implementation and native technical verification remain Codex's responsibility.

## Opus turn 20261108T020914-324a96ac

Mode: creative; read-only

# Snake parity: creative follow-up (final normal turn, read-only)

No files were edited. I traced the current `examples/zvb/SNAKE.PAS` against the C reference, the observation record (`docs/snake-c-observation.json`) and my initial review.

**Creative suitability:** As read, the implementation now matches the C demo's player experience in every area I flagged. I found **no creative blockers**. One technical precondition could turn the fruit behavior into a blocker if it fails (item 1).

**Technical readiness:** Not assessed. No native tests were run by me, and none are claimed.

## Response to Codex's dispositions

| My item | Disposition | My position |
|---|---|---|
| 1 Opening sequence | Accepted | Implemented as specified. `PlayTitle` runs over the bare board, then `ResetGame` draws only `SCORE:000`. The first `MoveSnake` at Tick 20 draws tail 11 at (10,7), head 1 at (11,7), then the apple. The observation's death-entry foreground confirms the same visible states. |
| 2 Trajectory | Accepted | The observed trace confirms my model: 30 + 30 + 14 writes, plus the floor write, per sprite. `PlayTitle` skips `SetY` on the stopping frame and floors afterwards, which matches. |
| 3 Input | Accepted | `ReadControls` is equivalent to C: first valid key per batch, later batches override, reversal checked against the selected direction, and release pairs skipped. |
| 4–6 Score, cap, icons | Accepted | `ShowScore` runs before the collision check. `JustAte` is gated at 255. Both invented icons are gone. |
| 7 Death palette | Adapted to observation | **I withdraw my three-byte derivation.** The hash-bound VRAM shows color 6 = AE 00 and color 7 unchanged at DF 5B. Observation beats source reading, and the two-byte request is correct. |
| 8 Replay | Accepted | No blanking, a clean palette and board, and no title. I endorse this as the documented player-visible exception. |
| 9 Fruit | Adapted | Reusing the exact instruction sequence is better than my proposed 32-cell table: it inherits whatever set the hardware actually produces. I agree. |
| 10 Wipe timing | Pending | Acceptable while measurement is pending. |
| 11 Long-length slowdown | Deferred | Acceptable deferral. It is unverified, so mimicking it would be invention. |
| 12 Sound | Removed | Creatively correct, because C is silent. **However, the original assignment said "Sound is preserved as an opt-in."** Removing it is a scope change. Codex should report it to the user as a deliberate decision with its reasons (fidelity and staging capacity), not fold it in silently. |
| 13 Glyphs | Adapted | The eight-glyph subset is invisible to players, since no other letters are ever displayed. I accept it. |
| 14 Controller | Accepted | The PIO sequence, priority (Up, Down, Left, Right) and keyboard-only restart all match C. |

## Remaining items

### 1. Fruit write target must be AppleY (conditional blocker, technical)

`PickFruit(AppleX)` writes X, then `inc hl`, then Y. That only works if AppleY sits at the address immediately after AppleX.

Classic CP/M Turbo Pascal allocates globals at *decreasing* addresses. If this compiler does the same, the Y byte lands in `NextDirection`. AppleY would then never change, and every apple would appear in one row. That would be obvious to a player.

The comment asserts adjacency. **Verify** by inspecting the compiled variable addresses, or by a native run that logs several distinct fruit rows. If it fails, use two locals, or a record, or two separate leaf calls, and preserve the instruction spacing between the R reads.

### 2. Retry bound before the fallback scan (minor creative point)

After 32 failed candidates, `NewApple` scans row-major from the last candidate. That places the apple in a cell the original algorithm may never produce.

C retries without limit, so mid-to-late games keep the original spot pattern until the board is truly crowded. A bound of 256 or more costs microseconds and keeps the pattern for longer, while still avoiding the hang.

This is a recommendation, not a blocker. It affects only crowded boards.

### 3. Transient tile overwrite when the snake has length 2 (timing claim, pending)

On each move, `MoveSnake` places the body tile at the old head and then overwrites it with the tail tile. At length 2 that is the same cell; for example, tile 12 and then 11 at (10,7) on the first move.

This is invisible if all placements finish within vblank. **Verify** with frame-sampled VRAM on the first move, or reason from the measured cost of each place call.

`DrawBackground` on replay writes 30 rows inside one vblank. At worst that gives a single torn frame during the snap back to a clean board. I consider that acceptable, but note it.

### 4. Raw key codes (technical, pending)

Player-visible correctness depends on these values in the shipped raw mode:
- arrow keys 160–163;
- release prefix 254;
- Esc 27;
- lowercase `q`;
- the keyboard mode value 4, meaning non-blocking plus raw.

A wrong arrow code would make the game unplayable. Confirm these against `zos_keyboard.h` and in a native input test.

Also confirm that exiting with `KeyboardMode(0)` restores the mode the OS was actually in. This affects shell usability after quitting, not gameplay.

### 5. Wipe duration (pending)

`Delay(4)` in this runtime may not match ZOS `msleep(4)` in granularity or overhead. Compare the measured time for the 280-cell wipe against the C binary, as already planned.

### 6. Timing equivalence I checked and found matching

These need no change:
- After a replay key, C reads the key outside vblank and counts frame 1 on the next vblank. Pascal detects the key at that same vblank and counts Tick 1 there. The first move falls on the same vblank in both.
- The first game matches the same way.
- Buffered keys from the title and the 100 ms pause reach the first `ReadControls`, as in C.

### 7. Out of scope, for Codex's code review

`NextDirection`, `OldDirection`, `SoundFrames`, `SoundUsed`, `SoundEnabled` and `Waiting`'s initial value are dead or vestigial. This is hygiene only, with no player effect.

## Distinguishing blockers from pending claims

- **Creative blockers:** None.
- **Conditional blocker:** AppleX/AppleY adjacency (item 1).
- **Pending technical or timing verification:**
  - native title trace equality;
  - key codes and keyboard mode;
  - `Delay(4)` wipe duration;
  - placements completing within vblank;
  - physical controller.
- **Documented player-visible exceptions to state to the user:**
  - clean replay;
  - fruit never placed on the initial body;
  - the bounded-retry fruit fallback;
  - WASD/Esc/Q conveniences;
  - removal of the opt-in sound path, which changes the original assignment's stated scope.

### Codex reconciliation

Accepted the original visual and sequencing direction. Native observations
now match the six-letter trajectory/cadence and actual sprite Y values, checkerboard,
ASCII score placement, palette and selected pixel hashes. Main field adjacency is
verified through header-derived native state and fruit observations; no hardcoded
globals base is used. Keyboard mode4 and raw codes were confirmed against headers;
Escape is corrected to$F0. Raw arrows/WASD, title buffering, clean replay, growth,
collisions, speed and actual OS exit pass. Dead sound/direction fields were removed.

Kept the bounded32-retry fruit fallback: it is a documented deliberate reliability
exception and avoids an unbounded refresh-register retry on crowded boards.
Clean iterative replay, avoiding initial-body fruit placement, silent C parity and
extra exit/steering keys remain documented differences. Transient incremental
length2 drawing follows C, but exact intra-vblank completion is not certified.
The wipe is still2.439s versus C1.158s; original per-cell+4ms behavior is retained.
Physical controller and display acceptance remain unqualified. Thus creative
recommendations are reconciled, but complete timing fidelity is not claimed.
Later technical/library changes were not part of the unchanged final creative
review input; this record is not a new final-source creative approval.
