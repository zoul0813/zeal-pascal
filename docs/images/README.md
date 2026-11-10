# Documentation screen captures

Captured on 2026-11-10 from native programs running on the unmodified Zeal Native
Emulator and the development environment's Zeal OS ROM. These are emulator
captures; they do not establish physical-hardware behavior.

Captures come from the native 640×480 display. Text-only images keep the full
640-pixel width and are cropped below the last used text row, with one extra
blank row. Each text row is 12 pixels tall: three used rows produce a 48-pixel
image. Lighthouse, Snake and ZealCalc retain the full 640×480 display.

Text captures use the display's actual
character and attribute planes, glyph ROM and RGB565 palette, without a host font
substitution. Snake's 320×240 tile display is shown at an integer 2× scale.
The blinking hardware cursor is omitted from the rendered captures.

| Image | Scene |
| --- | --- |
| `ide-menu.png` | Work file `H:/SQUARES.PAS`, Memory mode |
| `ide-editor.png` | A short CRT program printing squares from 1 to 10 |
| `ide-memory-run.png` | That program's output, followed by the IDE's return prompt; no COM was written |
| `ide-options.png` | Compiler options with Memory selected |
| `ide-error.png` | Error 54 for `Number := ;` |
| `ide-error-location.png` | Escape returns to line 4, column 13 in the editor |
| `lighthouse.png` | Cottage, key and matches collected, cellar still locked |
| `snake.png` | Saved Pascal Snake during play, score 000 |
| `zealcalc.png` | Products, formulas, total 52.00, and deliberate overflow/division-error cells |
| `calc.png` | Expression demo after ZShell `clear`; syntax and division errors are intentional |

The calculator input follows `examples/calc/demo.in`; failed expressions leave
ANS unchanged. ZealCalc uses the sheet fixture from the acceptance runner's
`screenshots()` function in `tests/acceptance/test-zealcalc.py`. Lighthouse and
Calculator use the shipped example binaries. Snake uses a retained saved COM
whose Pascal source and includes match `examples/zvb/`.

Capture input and image hashes are recorded in [captures.json](captures.json).
For future refreshes, use disposable emulator media and HostFS copies. Keep the
application at the intended scene, inspect every PNG, and ensure shell commands
or boot messages have not leaked into the display before replacing an image.
