# ZPU hardware test checkpoint

The native emulator checks are a prerequisite, not a physical-board pass. Record
board revision, ZVB firmware version, Zeal OS version, storage device and keyboard
before testing. Use a known working Zeal OS installation and ZShell.

## Package layout and installation

The hardware-test ZIP contains a `card/` file overlay, not a bootable disk image.
Keep the documentation and reference sources on the host:

- `card/bin/zpascal.bin`: compiler launcher.
- `card/lib/zpascal.zpl`, `SYSFILE.ZPU`, `CRT.ZPU`, `CRT.PAS`, `ZVB.ZPU`, `ZVB.LIC`
  and `ZVB.NTC`: matching compiler banks and stock Units.
- `card/zvb/`: self-contained COM demonstrations, source/asset includes, license and
  asset credits, and the nine-byte CRC input `DIGITS.TXT` (`123456789`, no newline).
- `docs/`, `libraries/`, `examples/` and `reviews/`: host-side Pascal references and the test checklist.
- `manifest.json`: exact file sizes, SHA256 hashes and artifact provenance.

Copy the contents of `card/` to a test directory or to the corresponding directories on your
existing card. The compiler needs its matching launcher and bank file. On a card
mounted as `T:`, launch `T:/bin/zpascal.bin T:/lib/zpascal.zpl` from ZShell. If using
a different device or test directory, supply its bank-file path explicitly. Units
are found beside that bank file, after checking the program's source directory.
Do not substitute another checkpoint's `zpascal.zpl`.

Run the COM demonstrations directly from their installed paths. They do not need
ZPU, PAS or INC files at execution time. Their embedded code is identified per
file in the manifest; some emulator-verified COMs predate the latest compiler-only bank
placement change. They are deliberately retained as tested executable bytes.

## Test sequence

Record pass/fail, observed output and any restoration or input problem for each
step. A frozen program, missing key response or corrupt shell after return is a
failure. Record the last visible state and the exact program/path used.

| Step | Action | Expected result |
| --- | --- | --- |
| 1 | Launch the compiler with the explicit bank path above | IDE opens; editing, saving and returning to ZShell work |
| 2 | Compile a minimal program in Memory mode, then save it in Com-file mode | RAM Run and the saved COM both execute; no missing stock Unit error |
| 3 | Run `zvb/ZVBCHECK.COM` | CRC and DMA verification complete; tone and sample playback are audible; returns to ZShell |
| 4 | Run `zvb/CRC32.COM T:/zvb/DIGITS.TXT` (adjust path) | The file checksum is `CBF43926`; returns to ZShell |
| 5 | Run `zvb/CRTCHECK.COM` | All 16 CRT routines' fixture checks complete and print `CRT-GUIDE-OK` |
| 6 | Run `zvb/FIRSTBAR.COM` without arguments | Orange bar on dark blue; a key exits; original text mode/settings return |
| 7 | Run `zvb/SPRITES.COM` | Moving sprites render correctly; any key exits; shell remains usable |
| 8 | Run `zvb/SNAKE.COM` | Animated title, checkerboard, apple and bottom score render; arrow keys and WASD steer |
| 9 | Eat an apple, collide, then press a fresh key | Growth/score update; collision wipe completes; replay restores a playable board |
| 10 | Exit Snake with Q or Escape | Text display and shell input work again |
| 11 | Recompile a supplied small source using the installed ZVB/CRT Units | Saved COM runs on the board; Unit lookup and static linking work on real storage |

Snake is silent, matching the original C game. Test audio with ZVBCHECK. The
emulated Pascal collision wipe is currently slower than C; measure it separately
rather than treating timing parity as established. Graphics waits require a
running display clock. SNES-controller testing (`SNAKE.COM -c`) is optional and
requires the matching PIO wiring; keyboard replay remains required.

For compiler/CRT checks, this small source is suitable for both execution modes:

```pascal
program BoardCRT;
uses Crt;
var Key: Char;
begin
  CrtInit;
  TextBackground(Blue);
  TextColor(Yellow);
  ClrScr;
  GotoXY(3, 2);
  Write(Con, 'Hardware CRT check - press a key');
  Key := ReadKey;
  CrtExit
end.
```

CrtExit restores cursor/settings, not the overwritten screen contents. Call it
explicitly on normal exit. See [CRT](crt-library.md), [ZVB](zvb-library.md) and
[register access](zvb-registers.md) for complete contracts and cleanup details.

## Results to retain

Keep the ZIP manifest with the results. Record load time separately from Snake's
asset initialization and title animation. For display faults, retain a photo or
capture; for crashes, record any runtime status/PC and whether ZShell recovers.
A passing emulator report does not fill in any physical-hardware result row.
