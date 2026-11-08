# CRT: text console, keyboard and delay

`Crt` supplies the native Zeal text-console routines listed below. Use
`uses Crt;` before other declarations. The shipped interface has 16 routines, listed in full below. Its scope differs from DOS Turbo Pascal CRT.

```pascal
program HelloScreen;
uses Crt;
var Key: Char;
begin
  CrtInit;
  TextBackground(Blue);
  TextColor(White);
  ClrScr;
  GotoXY(3, 2);
  Write(Con, 'Press a key to return');
  Key := ReadKey;
  CrtExit
end.
```

`Con` explicitly targets the console. Ordinary `Write`/`WriteLn` uses Pascal
Output, which may be redirected; the CRT screen routines always address the
video console itself. The example intentionally replaces screen contents.
`CrtExit` restores cursor/settings, not the previous framebuffer.

## Installation, compilation and linking

Keep `CRT.ZPU` beside the compiler bank file `zpascal.zpl`, normally in `T:/lib/`.
Unit lookup first tries the program source directory, then the directory of the
compiler bank file actually loaded. A corrupt or stale local `CRT.ZPU` is an
error, not a reason to silently use the installed copy.

`CRT.PAS` beside it is an interface reference. Its empty implementation is not
the source needed to rebuild the native routines. The build authors `CRT.ZPU`
from `libraries/screen-engine.inc`, `tools/screen-library.c` and
`tools/package-screen.py` through the project's normal ZDE build.

Import is static. Only reachable CRT fragments enter the output; the saved
program runs without `CRT.ZPU` or `CRT.PAS`. An unused `uses Crt;` performs no
initialization or screen change. `Crt.ClrScr` and other qualified calls work;
normal Pascal shadowing rules apply to unqualified calls. Taking `Addr` of a
CRT routine is unsupported because these exports carry the caller's I/O-checking
context; ordinary user-Unit routine addresses are unaffected.

In the IDE, save the source, then select **O → M → Q** and **R** for RAM execution,
or **O → C → Q** and **C** to save a COM beside the source. See
[the Unit guide](units.md) for lookup, compilation modes and artifact identity.

## Display model and session lifecycle

Screen, cursor and color routines require ZVB video mode 0: 640×480 text with
80 columns and 40 rows. They do not switch modes for the program. The 320×240 text
mode and graphics modes are unsupported by these screen routines and report
I/O error `$20`. `Delay` and `ReadKey` do not require text mode.

Logical cursor coordinates are one-based: X 1..80, Y 1..40. CRT handles the
hardware text scroll offsets when locating rows and cells. Blank cells use the
board's zero glyph and current foreground/background attributes. Line shifts
preserve each moved cell's original attribute.

`CrtInit` captures the current cursor, text scroll offsets, rendition, cursor
appearance/blink settings and relevant text-control bits, then selects white on
black. It does not clear the screen, reset scrolling or select video mode.
`CrtExit` restores the latest captured session and clears its active flag. Without
an active session it is a no-op, provided the supported text-mode check succeeds.

Sessions are not a stack: a second `CrtInit` replaces the snapshot. One `CrtExit`
restores that latest snapshot; another leaves it unchanged. CRT reconstructs a
saved pending-wrap cursor without depending on the board's shared saved-cursor
slot. A new Run starts a fresh session lifetime. A replacing Chain starts fresh
too; follow the shared Chain compatibility rules in the Unit guide.

There is no automatic cleanup on program termination, `Halt` or runtime error.
Call `CrtExit` on each normal exit path when restoration is wanted. For graphics,
call `CrtInit` while still in text mode, save/change video using the ZVB guide,
restore the previous text mode and settings, then call `CrtExit`. It cannot
restore a graphics mode or an overwritten palette for you.

## Complete routine reference

Names are case-insensitive. Parameter names here describe their purpose; the
shipped `CRT.PAS` interface may use `P 0`/`P 1` for those same parameters.

| Declaration | Behavior |
| --- | --- |
| `procedure CrtInit;` | Start/replace the cursor/settings snapshot and select normal rendition |
| `procedure CrtExit;` | Restore the latest active snapshot; screen contents are retained |
| `procedure ClrScr;` | Blank all 80×40 cells with current colors; reset cursor and text scrolling to the origin |
| `procedure ClrEol;` | Blank the current logical row from cursor through column 80; cursor stays in place |
| `procedure GotoXY(X, Y: Integer);` | Set logical cursor to X 1..80, Y 1..40 |
| `procedure InsLine;` | Insert a blank row at the cursor, shifting later rows down and dropping the bottom row; cursor stays |
| `procedure DelLine;` | Delete the cursor row, shift later rows up and blank the bottom row; cursor stays |
| `procedure LowVideo;` | Set light gray on black; cursor uses the inverse rendition |
| `procedure NormVideo;` | Set white on black; cursor uses the inverse rendition |
| `procedure HighVideo;` | Alias of normal rendition, as in the recovered behavior |
| `procedure TextColor(Color: Integer);` | Set foreground 0..15, preserving the current background |
| `procedure TextBackground(Color: Integer);` | Set background 0..15, preserving the current foreground |
| `function WhereX: Byte;` | Return logical column 1..80, or 81 while a right-edge wrap is pending |
| `function WhereY: Byte;` | Return logical row 1..40 |
| `procedure Delay(Milliseconds: Integer);` | Call native millisecond sleep with 0..32767; blocks |
| `function ReadKey: Char;` | Wait for one decoded key press without echo or an Enter requirement |

If a raw Port write leaves an invalid cursor row, blank/line operations guard
against writing outside the text planes. Do not rely on raw out-of-range state
as a positioning API. `WhereX`/`WhereY` expose live board state, not a private
cached cursor.

### Colors

These are ordinary predefined Integer constants in the compiler, not exports
qualified as `Crt.Black`. They work in constant expressions, arrays and subranges
and can be shadowed by a user declaration.

| Value | Name | Value | Name |
| ---: | --- | ---: | --- |
| 0 | `Black` | 8 | `DarkGray` |
| 1 | `Blue` | 9 | `LightBlue` |
| 2 | `Green` | 10 | `LightGreen` |
| 3 | `Cyan` | 11 | `LightCyan` |
| 4 | `Red` | 12 | `LightRed` |
| 5 | `Magenta` | 13 | `LightMagenta` |
| 6 | `Brown` | 14 | `Yellow` |
| 7 | `LightGray` | 15 | `White` |

Both foreground and background accept the complete 0..15 range. They are palette
indices; CRT does not install replacement palette colors. Color changes affect
later writes and blanking, not attributes already stored in existing cells.
The cursor's foreground/background nibbles are swapped from the active text
rendition. No `Blink` flag is accepted as an extra foreground bit.

### Keyboard and nonblocking input

`ReadKey` returns one press, consuming the runtime typeahead queue first when
available. It handles Shift/Caps/Ctrl letter conversion and ignores modifier-only
and release events. The Pascal decoder maps physical Enter to `#13`, Escape to
`#27` and Delete to `#127`; Ctrl-J stays `#10`. Other supported special keys retain
the native Zeal code. It does not implement DOS's zero-prefix/second-scan-code
`ReadKey` convention.

`KeyPressed` is a core Boolean intrinsic, not an exported `Crt.KeyPressed`.
It checks/queues a decoded press without consuming it from `ReadKey`'s queue:

```pascal
if KeyPressed then
  Key := ReadKey;
```

`ReadKey` borrows raw input without echo and restores the compiler/runtime's
input-mode policy and echo ownership afterward. This is not a general snapshot
of an arbitrary driver mode changed manually by the program. Programs which
install their own raw keyboard policy must manage that policy explicitly.
ReadKey uses native stdin rather than an arbitrary Pascal Input file or an
application-defined replacement input callback.

### Delay

`Delay(0)` is valid and reaches native `msleep(0)`; it is not omitted. Delay is
neither a CPU busy loop nor a vblank wait. Scheduling, interrupts and call overhead
can make elapsed time exceed the requested duration. For display frame timing,
use ZVB's `GfxWaitVBlank`/`GfxWaitEndVBlank`. No physical-board timing guarantee is
inferred from emulator checks.

## Arguments and errors

CRT validates the complete Integer arguments, even with `{$R-}`. Invalid X/Y,
color or delay ranges raise a range error; `{$I-}` does not turn invalid arguments
into valid values. A previously pending I/O error suppresses the hardware/sleep
operation, though argument evaluation and bounds checking still occur.

Device failures follow the caller's I/O-checking setting. With `{$I+}`, an I/O
error follows the normal runtime diagnostic path and retains the Pascal call's
source location. With `{$I-}`, inspect `IOResult`; reading it clears the pending
status. `WhereX`/`WhereY` return 0 when the operation is suppressed by an I/O error;
ReadKey's zero-initialized result likewise remains `#0` on a suppressed failure.
Zero is not the normal no-key result of blocking `ReadKey`.

```pascal
{$I-}
X := WhereX;
Error := IOResult;
{$I+}
if Error = 0 then
  WriteLn('Column: ', X)
else
  WriteLn('CRT I/O error: ', Error);
```

Declare X as Byte and Error as Integer. The example assumes earlier I/O status
has been handled. Reading the result alone does not prove success: use IOResult
under unchecked I/O. Error `$20` includes the unsupported-video-mode case.

## Scope, mapping and verification

Screen helpers restore the incoming CPU mapping, peripheral selector and
interrupt-enable state. VRAM is mapped only within bounded raw instruction
sequences; no OS call or Pascal guard runs while the OS page is replaced.
Long line operations give keyboard interrupts opportunities between rows.
CRT owns one session snapshot, not multiple concurrent terminal contexts.

There is no `Window`, `TextMode`, `TextAttr`, DOS scan-code emulation or CRT
`Sound`/`NoSound` in this interface. Use the [ZVB Unit](zvb-library.md) for video
modes, palettes, sprites and sound; its [named register reference](zvb-registers.md)
also supports direct `Port[Name]` access.

The native implementation is covered by historical framebuffer, cursor/color,
RAM/saved-program, lifecycle, I/O-failure and key-input checks documented in
[the implementation notes](screen-library.md). A current native fixture exercises all 16 documented routines; ReadKey is tested with
physical emulator key injection. Saved test programs run independently of source
and Unit files. See [the current reference evidence](unit-reference-evidence.json) for checks and
the existing full-framebuffer test harness packaging limitation. Physical
hardware/display/audio timing and full historical Turbo Pascal equivalence
remain separate qualification objectives.
