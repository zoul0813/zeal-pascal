# Pascal overlays on Zeal

Saved COM programs accept `overlay procedure` and `overlay function`. Overlay
bodies execute in the MMU window at `8000..BFFF`; shared Pascal variables, the
resident program and cross-bank call wrappers remain in page3. Private-call
prologues and returns stay in the member; CPU frames remain in page3. Parameters,
function results and lexical links use the ordinary Pascal calling convention.

```pascal
var Total:Integer;
overlay procedure Add(N:Integer);
begin Total:=Total+N end;
begin OvrDrive(8);Total:=35;Add(7);WriteLn(Total) end.
```

Select Com-file in the IDE's Options menu and compile with C. Keep the generated
`ZPxxxx` directory with the executable. Its `P.000`, `P.001`, ... files contain
consecutive groups of overlay declarations. An ordinary declaration between
overlay declarations starts another group. Nested overlays form child groups.
Run from the matching directory on the selected drive.

`OvrDrive(0)` selects the current drive at first use; `1` is A, `8` is H, and `20`
is T. Selection uses the current directory path on that drive. Executing an
absolute executable pathname does not change this selection. A group retains
its original opened file and drive after subsequent OvrDrive calls.

The first call loads only the requested member. Repeated calls reuse RAM.
Switching group members copies an immutable cached template into that group's
execution page and resets its typed constants; repeated calls to the same
selected member preserve their values. Changing a parent invalidates child
selections. Immutable templates may be recycled when RAM is exhausted; later
misses reread the group's original opened handle. Mutable execution pages stay
owned until termination. Uncalled groups need not have their files present.

A missing, corrupt or unavailable demanded member is a fatal F0 overlay error,
including under `{$I-}`. Active group members cannot be replaced. Ordinary nested
routines within the current member can call through resident wrappers.

The manager's code and state live together in one owned 16 KiB bank, temporarily
mapped into page1 during dispatch. Fixed resident control/job data is 24 bytes;
the gateway, routine wrappers and program-sized directory also remain resident.
The packed bootstrap starts in the initial overlay window and is cleared after
unpacking, so it occupies no permanent page3 code/data space. The overlay
window belongs to the runtime from program start, including this cold phase. No additional
manager bank is needed for this state placement.

Each member must fit in one 16 KiB page. The shared data, resident routines,
wrappers, gateway, program directory and other compiler helpers must also fit their resident
layout and leave the checked stack reserve. Overlays increase aggregate code
capacity; they do not enlarge the shared Pascal data or heap address space.

Memory compilation, overlay declarations inside compiled Units, captured nested
callback addresses from members, and combining overlays with Chain/Execute are
currently rejected. Units used by a program still link into its resident code.
Large UI/Unit combinations must budget for that resident Unit code.
The generated `LongWord` arithmetic helper belongs to each code member that
uses it; its cached address never crosses a member boundary.
[ZealCalc](../examples/zealcalc/README.md) demonstrates code overlays with a
separate bounded RAM data bank for its display cache and formula sources.
This is a native Zeal format, with no CP/M overlay-file compatibility claim.

Source-based F lookup recompiles the same source and selects the first matching
member for an overlapping overlay PC, following TP3's source lookup convention.
Keep source and compiler options consistent with the saved executable.

Compilation creates an exclusive generation directory. COM replacement is
staged, copied and verified; an existing COM is backed up before replacement and
restored on failure. This is recovery using the supported file APIs, without a
filesystem transaction or power-loss guarantee. If rollback itself fails, the
old image is retained as `OLD.COM` in that generation directory. Old published
generations are retained so an existing executable keeps its companions. Remove
an old directory only after no saved or running program needs it.

See [the sheet example](../examples/overlays/README.md) for shared cells,
parameters/results, typed-constant reset and an unused overlay.
