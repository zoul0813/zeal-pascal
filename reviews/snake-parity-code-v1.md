# Review: snake-parity-code-v1

## Opus turn 20261108T021506-550c314e

Mode: code; read-only

# Snake C-parity review (`snake-parity-code-v1`): linker scope0 fix, Pascal port, observer and USES check

This review is read-only and covers only the supplied materials. I have not seen any successful compile, native run or evidence JSON, so nothing below is certified as tested.

**Summary:** I found no definite correctness defect in the Snake gameplay logic. Its collision, tail-vacate, deferred growth, cap, speed, U-turn, title physics, controller sequence and death palette all trace correctly against `snake.c`/`title.c`. The main risks are:
- the linker scope0 gate (Q1);
- the keyboard mode constant (Q2);
- observer checks that are vacuous or weak (findings 1–3).

## Questions that need context (possible required fixes)

**Q1 (high if confirmed) – `src/compiler_units_link.c`, both `UR_IMPORT` loops: the scope0 gate also skips chain patching**
- **What changed:** The new `&&cc_unit_word(at+2)` condition gates two things: rooting in the first loop, and `cc_patch_chain(at+8, …)`/`cc_rput(scope,R_ENTRY,…)` in the second loop.
- **Why it matters:** The old bug only fired because scope0 imports had a nonzero `at+8`, and `at+8` is treated as a code patch chain.
  - If that chain is real (for example, typed or structured constants, or `SizeOf`/type references emitting a fixup site), those sites now stay as unpatched placeholders. Nothing reports an error.
  - If `at+8` is not a real chain for scope0, then `compiler_units_object.c:cc_units_write_object` is still wrong. It walks `code_word(head)` for *every* `UR_IMPORT` and creates `UR_EXTERNAL` references to `export+8`. A unit that imports constants from another unit would then serialize bogus external references, and transitively re-root fragment0.
- **Pre-fix damage:** The old code also did `cc_rput(0,R_ENTRY,…)`, which overwrote routine 0. That may be the real cause of the "overflow at EOF".
- **Needed:** The code that writes `UR_IMPORT+8` for non-routine symbols.
- **Suggested fix (either way):** Make it a hard error: `if(kind==UR_IMPORT && !scope && chain) cc_fail(UNIT_FORMAT)`, or explicitly clear the chain at its source. Apply the same `scope` condition, or that assertion, in the object writer's import walk.

**Q2 (medium) – `SNAKE.PAS` main block, `KeyboardMode(4, …)`**
- **The doubt:** As I recall `zos_keyboard.h`, `KB_READ_NON_BLOCK` is `1<<2` and `KB_MODE_RAW` is `1`. That would make raw non-blocking equal to 5, and 4 equal to cooked non-blocking. I am not certain of these values.
- **Impact if wrong:** No 254 release prefixes, and arrows may not arrive as `0xA0`–`0xA3`.
- **Also check:** That 0 is the correct restore mode at exit. The program does not save the prior mode, so its ordering relative to `CrtInit`/`CrtExit` matters too.
- **Verify:** Compare against the installed header.

**Q3 (medium) – Inline leaves (`KeyboardMode`, `RawRead`, `PickFruit`)**
- **Syscall registers:** These look right for Zeal: H=dev, L=syscall (0 read, 7 ioctl), C=cmd, DE=arg/buffer, BC=size, results in A/BC.
- **Unconfirmed:**
  - that this compiler's frame puts the last parameter at IX+4 with IX as frame pointer;
  - that `RST 8` preserves IX;
  - that an Inline body may clobber A/BC/DE/HL.
- **`PickFruit` layout:** It depends on `AppleY` immediately following `AppleX`. If that allocation is wrong, the second byte written lands in `Tick` or another global.
- **Optional hardening:** Pass `var X, Y: Byte` separately.

**Q4 (low) – Byte×constant index widening**
- **Affected expressions:** `Board[HeadY * BoardWidth + HeadX]`, `TailY * BoardWidth`, `AppleY * BoardWidth`.
- **Risk:** Rows ≥12 exceed 255. Correctness depends on Byte operands being promoted to Integer, as in Turbo Pascal.
- **Coverage:** The observer only exercises row 7. Confirm the promotion, or test rows 12–13 and the right and bottom walls.

## Established findings

**1. (medium) `tools/test-snake-parity.py`: the "default audio untouched" check is vacuous**
- **Problem:** `SoundUsed`/`SoundEnabled` are vestigial and never become True, so the check cannot fail.
- **Fix:** Observe the sound I/O ports directly, by breakpointing or watching OUT instructions to the sound register range, or by snapshotting the sound registers before and after.

**2. (medium) Observer: the "Escape returns to OS" check is weak**
- **Problem:** `PC < 0x4000` after `run 10000000` is also true while a still-running program sits inside the `read` syscall.
- **Fix:** Assert actual exit, using the `Snake finished` output, the shell prompt or the program-exit breakpoint.

**3. (medium) Verification gaps against the contract's claimed validation**
- **Not checked by the observer:**
  - The head, body, corner and tail tile IDs in the foreground layer. `BodyTile`/`TailTile` are never compared in VRAM.
  - That the failed move is not drawn. Only state variables are checked.
  - Tileset VRAM pixels after on-device LZ/`pal_offset`/opacity loading. The C capture also never recorded tile memory ("next run" comment).
  - The font palette (colors 32+). Only 38 bytes are compared.
  - Wipe order and the 4 ms pacing.
  - Speed increments, the length-255/score-253 cap, buffered title keys and WASD.
- **"Throughout the title":** This is checked only once, after the title.
- **Fix:** At minimum add:
  - a foreground snapshot after the growth move, checking tiles 1, 12 and 9 at (12,7), (11,7) and (10,7);
  - a corner case after the UP turn;
  - one tileset VRAM block compare against a C capture.

**4. (low) Observer flake: the random apple can land on cell 152**
- **Trigger:** After the first eat, `NewApple` may land on 152.
- **Impact:** The next move eats again, so `just_ate==0` fails intermittently.
- **Fix:** Rewrite the fruit fixture to a far cell after the eat.

**5. (low) `SNAKE.PAS` main loop: replay moves one vblank early**
- **Cause:** `ResetGame` in the Waiting branch is followed in the same iteration by `Tick := Tick + 1`.
- **Impact:** The first move comes 19 vblanks after the replay frame instead of 20, as C has after `reset()`.
- **Fix:** Skip the tick on the reset frame.

**6. (low) Waiting branch inspects only `Keys[0]`**
- **Fresh key:** A batch that is only a release pair (key held through the wipe, released after the flush) restarts the game. C behaves the same, but the contract says "a fresh key". Decide which behavior you want and document it.
- **Quit key:** Only lowercase `'q'` quits here, while `ReadControls` uses `UpCase`.

**7. (low) USES linker check strength (`tools/test-zvb.py`, `tests/native/zvb/USES.PAS`)**
- **Threshold:** `code_bytes < 512` only detects the regression if fragment0 plus its transitive closure is large relative to the baseline. Record the pre-fix value to show the check fails without the fix. Better, assert equality with an otherwise identical client that has no symbol references (the old `begin end.`).
- **Missing case:** Add a structured constant import (string/real/set).
- **Observability:** `Halt` versus normal completion is not observable, so the fixture does not verify the imported values at run time.
- **Crash path:** `next(...)` raises `StopIteration` if the USES row is missing. Default it to failure.

**8. (low) Documentation and hygiene**
- **README:** The table row for `SNAKE.PAS` still lists "sound effects".
- **Generator:** It still emits `SCORE.INC` and `score_letters`, which Snake no longer uses. Either confirm another consumer or drop them.
- **Capture script:** `capture-snake-reference.py` uses hard-coded C addresses without first asserting the binary's SHA-256.

## Verified by trace (not tested)

- **Board representation:** The links are consistent, including the head moving into the vacated tail cell and the length-2 U-turn, which C also allows.
- **Ordering:** Pending growth and score are committed before collision.
- **Growth and speed:** The cap logic matches C, and `Score mod 8` is equivalent to `apples_to_boost`.
- **Title:** The trajectory, the final `set_y(64)`, the hide at Y=0, and X/tile values match C.
- **Death palette:** `AE 00` at color 6 matches the observation.
- **Controller:** The clock and latch sequence matches C.
- **`ReadControls`:** Its batch, break and overwrite semantics match C `input()`.

## Limitations

I could not see:
- where `UR_IMPORT+8` is written;
- this compiler's frame and Inline ABI;
- the keyboard header values;
- the native results.

The ZDE build and native tests are still pending, so none of these conclusions is a test claim.

### Codex reconciliation

Q1: rejected the original diagnosis after tracing writers: non-routine imports
have no code chain. The speculative scope0 changes and USES-only test changes
were fully reverted. The actual EOF overflow was the temporary $F400 code ceiling;
CC_CODE_LIMIT now separates staging from final physical/stack checks. No claimed
constant/type rooting fix remains. Host boundaries, 1,024 sanitized fragment
assertions, 963 native fragment assertions, bank residency and 643 Unit/Real/Chain
assertions pass; the earlier 30,294-code-byte Snake now compiles and runs.

Q2/Q3: confirmed against installed headers and compiler/kernel sources (supplied
ABI reference). Raw nonblocking is 4; raw Escape is $F0. Inline arguments are
last-first at IX+4; IX/IY and IFF contracts are verified. Main globals derive from
header word24; native fruit/board behavior verifies the adjacent one-byte fields.
Q4: bottom-row fixture at index270 and fatal bottom collision pass.

Findings1-4: accepted. Removed vestigial audio state; executable-level omission
check rejects the sound writer. Exit uses actual zos_loader_exit with status0.
Observer now checks actual sprite Y readback, head/body/tail/corner IDs, unchanged
fatal board, all selected tiles/font/palette against hash-bound C captures,
row-major wipe, speed/cadence, arrows/WASD, title-buffered input and bottom bounds.
Fruit fixture is reset after eating. Single-byte restore observation was fixed
by avoiding an extra step that skipped the tail breakpoint. Raw keyboard delivery
is allowed two input frames after a tap. Current behavior checks pass.

Findings5-8: accepted/adapted. Replay skips tick on the reset frame. Any fresh raw
batch, including release, may replay as in C; documented explicitly. The speculative
USES regression additions were reverted with Q1, so no threshold-based fix is
claimed. Source/generator no longer retain SCORE.INC or sound descriptions; capture
scripts assert reference binary hashes. Memory compilation is explicitly not
qualified for the larger source; Com-file saved runs pass with sources/Units absent.

Known unresolved fidelity issue: original per-cell placement plus Delay(4) now
wipes in 24,385,215 T-states versus C's 11,584,421. Overall parity remains failed;
behavior_passed is separate. CPU oracle measures the optimized tile leaf at338T;
the 280 public calls cost9,583,840T, kernel sleeps11,393,760T and other caller work
3,407,615T. Do not claim complete performance parity. Physical controller/output
remain unqualified. Subsequent ZVB direct paths are submitted in the follow-up.

## Opus turn 20261108T041256-2c49a12c

Mode: code; read-only

# Snake C-parity final follow-up review (`snake-parity-code-v1`)

**Summary:** I hand-decoded every changed Inline leaf against its Pascal parameter layout (last argument at IX+4) and its SDK reference. I found no byte-level defect in the ZVB leaves. The one likely must-fix is outside ZVB: Snake leaves the keyboard in raw blocking mode on exit (finding 1). The other findings are verification gaps, a pre-existing unchecked wrap, and stale documentation. I have not seen results from the running 10-client regression and do not certify it.

## Established findings

**1. (high, likely must-fix) `examples/zvb/SNAKE.PAS` main block: `KeyboardMode(0, InputError)` at exit**
- **Trigger:** Every exit, including a failed `Initialize`.
- **Evidence:** The supplied `keyboard_h.asm` defines `KB_MODE_RAW = 0`, `KB_MODE_COOKED = 1` and `KB_READ_BLOCK = 0`. Mode 0 is therefore raw blocking, not the shell's normal cooked mode. Startup mode 4 is correct.
- **Impact:** Unless `CrtExit` or the kernel resets the mode, ZShell would receive raw lowercase bytes and `$FE` release pairs, with no line editing.
- **Why tests miss it:** The C demo never exits, so there is no precedent. The observer stops at `zos_loader_exit` and never types a command afterwards.
- **Fix:** Restore `KB_MODE_COOKED or KB_READ_BLOCK` (1), or whatever mode the shell's own initialisation uses.
- **Verification:** After Esc, type a shell command in the observer and assert it runs.

**2. (medium) `src/compiler_groups.c` `cc_group_close`: unchecked 16-bit wrap**
- **Code:** `end=cc.emitted+word(g,at+G_BYTES)` is computed in `uint16_t`.
- **Trigger:** With emitted up to `0xFFFE` and member bytes up to about `0x7FFE`, the sum can wrap.
- **Impact:** The zero-fill loop is skipped and `cc.emitted` moves backwards.
- **Context:** This predates the change (the old `F400` bound could also wrap), and the comment calls the path dormant. It is still exactly the "unprotected wrap" this round asked about.
- **Fix:** `if((uint32_t)cc.emitted+bytes>CC_CODE_LIMIT) return fail(...)`.

**3. (low) `src/compiler_units_link.c`: misleading diagnostic when staging is exactly full**
- **Trigger:** Unit linking starts with `cc.emitted == 0xFFFF`.
- **Impact:** `link()` rejects `origin>=limit` with `CF_ARGUMENT`, which is reported as `UNIT_FORMAT` rather than capacity error 99. The image is not corrupted.
- **Fix:** Fail with error 99 before calling `cc_fragment_plan` when `origin>=CC_CODE_LIMIT`.

**4. (medium, verification gap) `tests/host/zvb-leaves.c`: the sample FIFO-full path is never exercised**
- **Evidence:** The oracle's `in(0xA2)` returns `conf|0x80` while sampling, so bit 6 (FIFO full) is never set.
- **Untested:** The `bit 6 / jr nz` throttle loop.
- **Assumed:** The bit numbers (full = 6, ready = 7, size = 0, sign = 2). They depend on SDK `ZVB_SAMPLE_CONF_*` constants that were not supplied.
- **Why it matters:** A wrong full-bit means FIFO overrun on hardware. A wrong ready-bit means a hang with interrupts masked, recoverable only by reset.
- **Fix:** Have the oracle model a finite FIFO depth, and cite the SDK header for the bit values.

**5. (low, verification gap) `tools/test-snake-parity.py` "sound writer omitted" is only a proxy**
- **What it checks:** Only that the `WritePeripheral` body is absent.
- **What it misses:** `SoundSetHold`, `SoundSetVoices`, `SoundSetVoicesVol`, `SoundSetChannels` and `SoundPlaySamples` each select the device with their own `3E 03 D3 8E` sequence and never call `WritePeripheral`. Linking any of them would not be caught.
- **Fix:** Search for each sound leaf's Inline bytes, or assert that no `OUT ($8E)` with A=3 occurs during the run.

**6. (low) `SNAKE.PAS` `ReadControls`: undocumented direction keys**
- **Code:** Raw codes `#136/#134/#130/#132` also steer, and `#240` quits.
- **Gap:** `$F0` (Escape) is explained in the reconciliation. The four steering codes appear in neither the README nor the fidelity contract.
- **Fix:** Document them (apparently keypad 8/6/2/4), or remove them.

**7. (medium, documentation) Stale or contradictory claims**
- **`docs/zvb-library.md` Installation section:** "Both compiler paths produce identical saved Snake bytes" contradicts the later statement that Memory-mode Snake compilation is not qualified.
- **`docs/zvb-library.md` banner:** "native emulator tests pass for the unit and demos" is premature. The 10-client regression is still running, and Snake parity overall is recorded as failed.
- **`examples/zvb/README.md` banner:** "Final code review is pending" will be stale after this turn.

## Performance objective (open by design, not a defect)

Per wipe cell, the profile shows:

| Item | Pascal per cell | Source |
| --- | ---: | --- |
| Public `GfxTilemapPlace` call | ≈34,200 T | 9,583,840 T / 280 |
| Direct tile leaf inside that call | 338 T | host oracle |
| Kernel sleep | ≈40,700 T | 11,393,760 T / 280 |
| C, whole cell including its sleep | ≈41,400 T | 11,584,421 T / 280 |

Kernel sleep alone is close to C's whole per-cell cost, so the gap is the roughly 34,000 T spent in each public tile call.

- **Leaf versus call:** Roughly 33,900 T of each call lies outside the leaf. That is far more than argument copies or the `GfxContextValid` comparisons should cost.
- **Likely cause:** The generated `RT_GUARD`/poll per guard point, possibly issuing a syscall.
- **Question:** If that guard reads stdin for break detection, does it ever consume raw key bytes intended for `RawRead`? The tests' taps pass, but they are delivered while execution is paused at breakpoints.
- **Status:** This is correctly left open. The docs do not overclaim parity.

## Verified by trace (not tested)

**`VRAMTransfer`, all 188 bytes:**
- All relative-jump targets resolve correctly.
- Fast-path eligibility is right: no carry, H<$40 or exactly $4000, Add 0 or fill.
- LDIR copy and the `(de)=v; hl=de; de++; ldir` fill are correct.
- In the slow path, the read, companion-parity and transparent/add/fill branches are correct.
- Bank crossing (D=$40 → D=0, `inc (ix+14)`, remap) is correct.
- IFF2 is restored via `pop bc; bit 2,c`.

**Graphics leaves:**
- `GfxTilemapPlace` and `GfxTilemapLoad` compute 80·Y+X with the layer at +$1000.
- `GfxPaletteLoad` and `GfxPaletteRead` use base $E00 + 2·FromColor with an LDIR in the correct direction.
- `GfxSpriteRender` and `GfxSpriteRenderArray` use base $2800 + 8·index, with length 8·count for the array.

**Sprite setters:**
- `GfxSpriteSetXY` writes X low then high, then Y low then high.
- `GfxSpriteSetX` and `GfxSpriteSetY` each write their field low then high.
- `GfxSpriteSetTile` and `GfxSpriteSetFlags` rewrite the companion byte, as the SDK does.

**`CRCFeed`:**
- It reproduces the SDK's ceil(size/256) OTIR loop.
- When the high byte is $FF it wraps to 256 iterations, which is correct.
- A zero size reads the result only.

**Sound leaves:**
- `SoundSetHold` sets or clears the hold mask correctly.
- `SoundSetVoices` saves the prior hold and restores it after configuring, as documented.
- `SoundSetVoicesVol` writes select then volume.
- `SoundSetChannels` writes left (`$AB`) then right (`$AC`).
- `SoundPlaySamples` maps modes 0, 1 and 2 to configuration values 1, 0 and 4, and restores the hold from E.

**`DMAStartTransfer`:** It matches `_zvb_dma_virt_to_phys` followed by `_zvb_dma_start_transfer` byte for byte in semantics.

**Snake Inlines:**
- `KeyboardMode` uses H=1, L=7 (ioctl), C=0 (`KB_CMD_SET_MODE`) and E=mode.
- `RawRead` uses H=1, L=0 (read), DE=buffer and BC=count.

**Staging boundary:**
- The last emitted byte is at most $FFFE.
- The low 15 relocation bits are at most $7FFE.
- `cc_link_reference` and the unit-range copy bound to `CC_CODE_LIMIT`.
- Plan/emit page selection stays within two slots.
- `CC_STATIC_LIMIT` is retained for globals.
- A word operand straddling $FFFF fails at emission.

**Observer:**
- `sample()` offsets match the new declarations.
- The bottom-row fixture builds a consistent two-cell board.
- Wipe start and end bracket the same span as the C capture, give or take one palette call.

## Prior findings: disposition check

- **Q1:** The revert is accepted. The link file and `USES.PAS` are back to baseline.
- **Q2:** Mode 4 is confirmed as raw non-blocking by the header. The exit-mode problem is new (finding 1).
- **Q3 and Q4:** Resolved.
- **Findings 1–4:** Addressed. Finding 3's coverage is now substantial.
- **Findings 5–8:** Addressed as dispositioned in the reconciliation.

## Limitations

I did not have:
- the Crt unit, `RT_GUARD` or the kernel exit path (these decide whether finding 1 is masked);
- the SDK sound constant header (finding 4);
- the disk-staging implementation for addresses at or above $F400;
- any test logs.

The reported pass counts, and the still-running 10-client regression, are not verified by this review.

### Codex reconciliation

1. Investigated before editing and rejected the assumed shell mode: the supported
ZShell process launcher resets raw nonblocking keyboard mode after exec. The
native observer now exits via zos_loader_exit and types H:/AFTER.COM; it verifies
that a distinct five-byte program loads at4000. This passes with existing Snake
cleanup. CrtExit itself restores video only. No unsupported mode1 change made.

2. Accepted: cc_group_close checks the widened sum against CC_CODE_LIMIT before
narrowing or writing. New host fixture exercises the actual function at FFF0+15,
FFF0+16,9000+8000 and BFF0+32, preserving all canaries/cursor on failure.
3. Accepted: full staging with live Unit roots now fails99 before fragment_plan.
ZDE rebuild and bank checks are required for these compiler changes.

4. Accepted: CPU oracle now models a four-byte FIFO draining one byte every third
status poll. It rejects writes when full, requires actual throttle hits, verifies
all bytes drain before return and retains IRQ/hold assertions. Full/ready bits6/7
and size/sign bits0/2 are confirmed in the SDK's include/zvb_hardware.h lines309-313.
The sanitized suite passes966798 assertions. Production sound bytes are unchanged.
5. Accepted: omission check now extracts and rejects every independent sound leaf.
6. Accepted: keypad8/2/4/6 aliases documented in README and fidelity contract.
7. Accepted: contradictory Memory/Com claims and banners removed; current evidence
reports overall timing failure separately from passing behavior. Review status
is described as two recorded turns, not a new final-source approval.

Performance question: runtime.asm checked_guard/check_boundary preserve registers
and perform bounds/source-PC bookkeeping without syscalls or callbacks. They do
not read stdin. Reducing safe generated overhead remains a separate open objective;
this checkpoint does not weaken guards or alter Snake delays to claim parity.

The final source corrections above were made after this second read-only turn.
No extra Opus turn is requested or claimed. The unchanged-input finalization seal
therefore cannot be reused; Codex verifies these corrections and records the
remaining timing/hardware limitations openly.

Verification closure: final ZDE zpascal_bin/zzvbtest_bin build passes and bank
residency passes (16 active callers, no lent-bank code). Real group-boundary
cases4, physical9, emission9 and sanitized fragments1024 pass. Rebuilt hardware
foundation passes349 native assertions. Its obsolete336 expected count was
corrected using 11 author fixtures, two clients, five lifecycle checks and
13 closes checking each of CODE_BANK_LIMIT slots, then rerun successfully.
Ten final ZVB clients compile/execute and run detached from source/Units
(286 harness assertions, zero failures); CRC32 golden output passes. Snake
saved demo passes38 harness assertions; final behavior and post-exit shell-command
checks pass. Only the explicit wipe timing comparison remains failed. Evidence:
docs/code-staging-evidence.json, docs/zvb-optimized-evidence.json and
docs/snake-parity-evidence.json.
