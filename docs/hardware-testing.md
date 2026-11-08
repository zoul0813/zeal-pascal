# ZPU hardware test checkpoint

The native emulator checks are a prerequisite, not a physical-board pass. Record
board revision, ZVB firmware version, Zeal OS version, storage device and keyboard
before testing. Use Zeal OS v0.8.0 or a kernel containing both upstream fixes
`98ce65d` (pfree return status) and `45e9478` (free the requested page), plus ZShell.
The physical kernel `0.6.0-24-g5bb34a7` lacks these fixes: its pfree passes zero
to the MMU free routine, which returns native error 6 and leaves the allocated
page unavailable. Compilation requires working page allocation and freeing.
Update through the normal Zeal OS installation process before accepting compiler
results; do not ignore the failed free or treat this as a Pascal syntax error.

## Package layout and installation

The integrated acceptance directory is a file overlay, not a bootable disk image:

- `bin/zpascal.bin`: compiler launcher.
- `lib/zpascal.zpl`, `SYSFILE.ZPU`, `CRT.ZPU`, `CRT.PAS`, `ZVB.ZPU`, `ZVB.LIC`
  and `ZVB.NTC`: matching compiler banks and stock Units.
- `examples/`: all general examples, rebuilt BIN files, sources, includes and
  input transcripts. Programs with relative data/companion paths must run from
  their own example directory, for example `cd T:/examples/measure`.
- `zvb/`: rebuilt COM demonstrations, source/asset includes, CRTCHECK, license
  and credits, and `DIGITS.TXT` (`123456789`, exactly nine bytes without newline).
- `README.TXT` and `MANIFEST.JSON`: deployment instructions, hashes, compiler
  checkpoint and the two passing native-example report locations.

Keep this checklist and the complete Unit references in the source checkout on
those host-side paths. Copy the acceptance directory's contents onto your test
card, retaining `bin/`, `lib/`, `examples/` and `zvb/`. On a card mounted as `T:`,
launch `T:/bin/zpascal.bin T:/lib/zpascal.zpl` from ZShell. Adjust the device or
test-directory paths if needed. Units are found beside the bank file, after
checking the program's source directory. Keep the matched launcher and bank
file together.

Build the production compiler and test harnesses, author the stock ZVB Unit,
then rebuild/replay every example. Set `ZP_ZEAL_ROOT` to the existing Zeal tools
root when running from a separate worktree:

```sh
zde --no-tty cmake build --target zpascal_bin zexampletest_bin zzvbtest_bin -j4
python3 tools/test-zvb.py --skip-hardware --output-library build/ZVB.ZPU
python3 tools/test-examples.py --package
python3 tools/test-zvb.py --prebuilt build/ZVB.ZPU --skip-hardware --disk-snake \
  --case SMOKE --source SMOKE tests/native/crt-guide.pas --case ZVBCHECK \
  --case SPRITES --case SNAKE --case FIRSTBAR --case CRC32
```

Use `--emulator /path/to/zeal-native` on the example/ZVB commands to select an
installed emulator explicitly. After the reports pass, commit the source and
refreshed example binaries before packaging. The directory records a clean
checkpoint and refuses dirty tracked/untracked inputs:

```sh
python3 tools/package-hardware.py --examples-report analysis/native-slice/RUN/examples-report.json \
  --zvb-report analysis/native-slice/RUN/zvb-report.json --output build/tf-acceptance
```

The ZVB client run must cover SNAKE, SPRITES, ZVBCHECK, FIRSTBAR, CRC32 and a SMOKE
source override using `tests/native/crt-guide.pas` for CRTCHECK. Packaging validates
sources, includes, stock Units, compiler identity and exact tested executable
hashes. It refuses to overwrite an existing acceptance directory. Saved
executables contain their selected Unit code and assets and need no PAS, INC or
ZPU files at run time.

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

Keep the package manifest with the results. Record load time separately from Snake's
asset initialization and title animation. For display faults, retain a photo or
capture; for crashes, record any runtime status/PC and whether ZShell recovers.
A passing emulator report does not fill in any physical-hardware result row.
