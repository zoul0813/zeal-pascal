# Shipped Crt library

Production programs use `uses Crt;` before declarations. Crt operations
are ordinary imported procedures/functions, with qualification and shadowing
following the normal unit rules. The normal build produces `build/CRT.ZPU`
and its interface reference `build/CRT.PAS`. TF packaging installs them beside
`T:/lib/zpascal.zpl`. Rebuild the artifact whenever the private runtime changes.

Lookup first tries `UPPERCASE.ZPU` beside the program source, then beside the
compiler bank container actually loaded (including a command-line override).
Only a missing local file permits fallback: an invalid, stale or unreadable
local artifact is an error. This lookup applies to user-authored units too.

Crt exports ClrScr, ClrEol, GotoXY, InsLine, DelLine, CrtInit, CrtExit,
LowVideo, NormVideo, HighVideo, Delay, TextColor, TextBackground, WhereX and
WhereY. The color constants remain ordinary core predefined constants.

The instruction emitter is a build tool shared with the legacy regression
control. Production contains no CRT instruction generator. The host author
writes the same checked ZPU1 fragment format as user-authored Pascal units;
Crt has no compiler-specific namespace or automatic import. Only selected
operations and their fragment dependencies enter the output. CrtInit/CrtExit
share a snapshot and the existing per-Run lifetime flag. Crt still requests
three independent execution pages; moving it to disk does not establish a
smaller runtime stack contract for framebuffer and sleep operations.

Export flag bit0 requests the caller's I/O checking setting in A before an
ordinary native call. All other primitive parameter/result rules remain the
same. Crt wrappers capture this setting in the invocation-owned IOCALL
frame before any syscall. They preserve the caller's source PC, validate full
Integer ranges even under R-, keep query results on the invocation stack and
preserve IX. Taking Addr of these context-sensitive exports is rejected;
ordinary unit routine addresses are unchanged.

The compiled program contains its selected Crt code. Running it requires
neither CRT.ZPU nor CRT.PAS. Missing source does not prevent compilation
or execution; the interface reference carries no executable implementation.

Production Chain output also links imported unit fragments through the same
linker. The parent and child retain the existing matching Start/End/runtime
profile requirements. Crt activation starts a fresh CRT session when the
chain replaces the parent. The native qualification includes a saved parent
and a child that imports Crt and restores its cursor snapshot.

The extraction reduces the production bank container from 240094 to 236209
bytes (3885 bytes). CRT.ZPU is 3566 bytes. Compiler startup still retains ten
code pages: the bank that previously held CRT emission also owns other code.
This stage removes generator bytes; it does not free another whole startup page.

The final oracle passes 502 assertions, including the live banked saved-parent
Chain launch, complete framebuffer checks, I/O suppression and source locations.
A separate standalone observer validates Chain output and page recovery without
compiler/unit files. The 905-assertion unit/Real regression, focused Screen/MATH
IDE comparisons, host sanitizers and actual TF installation/default launch pass.
Reports and hashes are in `screen-library-evidence.json`.

The initial Opus review was reconciled. Its required follow-up failed with API
429 (`usage_limit_reached`); this checkpoint is functionally verified but its
review is incomplete. See `../reviews/screen-library-v1.md`.

The unit was renamed from Screen to Crt to follow Turbo Pascal 4–7 naming.
Programs now declare `uses Crt;`, with qualified calls such as `Crt.ClrEol`.
No stock Screen alias is shipped. Build and TF packaging remove the former
SCREEN.ZPU/PAS artifacts. The rename passes the 502-assertion CRT suite, the
905-assertion unit/Real suite, and TF copy/preservation/default-launch checks.
See `crt-rename-evidence.json` for the new artifact hashes and reports.
