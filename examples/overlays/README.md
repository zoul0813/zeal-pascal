# A sheet using RAM overlays

[SheetOvr](sheetovr.pas) keeps three cells in shared memory. `Sum` and `Scale`
are consecutive overlays in one group. `Scale(2)` changes the cells directly;
`Sum` returns the total. No Execute/Chain handoff or intermediate data file is
needed. An uncalled overlay in another group demonstrates demand loading.

Copy this entire folder to H: and run `sheetovr.bin` from that directory. The
included `ZP000B` folder contains its companion groups. The shipped bytes and
native validation provenance are recorded in [binary.json](binary.json).

To rebuild, compile `sheetovr.pas` using the native IDE's Com-file mode. Run the generated
COM from its directory and retain its generated `ZPxxxx` folder. This example
selects H with `OvrDrive(8)`; change that argument to `20` for T, or use `0` when
the current drive and directory already contain the companions.

Expected output:

```text
Sheet total: 120
OVERLAYS OK
```

The program also checks that a repeated Sum preserves its local typed counter
and switching away and back preserves it (the third call observes8). A failed check raises division by zero.
The native acceptance test runs it with the unused group's file absent, and
checks the final cells, total and process cleanup.

The full [ZealCalc example](../zealcalc/README.md) also uses code overlays,
with a shared runtime/stack and one separate data-cache bank.

See [overlay behavior and limits](../../docs/overlays.md).

The manager now keeps its code and state in its existing MMU bank. Its packed
bootstrap occupies the initial overlay window and is cleared after unpacking.
Fixed resident control/job data is24 bytes, plus gateway code, wrappers and the
program directory. This example's resident code and globals total3660 bytes,
7191 fewer than the previous layout. The native suite also runs a program with
8000 bytes of shared data, alongside the larger-than64KiB overlay program.
