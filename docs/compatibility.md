# CP/M Turbo Pascal 3 and Zeal Pascal

Zeal Pascal targets the CP/M-80 Turbo Pascal 3 source-language style while
running natively on Zeal. It does not run CP/M executables, emulate a CP/M BIOS,
or claim complete equivalence for every program. The references describe current
implemented behavior; this page explains the major migration boundaries.

## Native executables and environment

| CP/M expectation | Zeal Pascal behavior |
| --- | --- |
| COM runs at a CP/M address with CP/M page zero | COM is a native Zeal program with its own runtime/header/layout. Recompile source. |
| BDOS/BIOS calls and fixed OS addresses | Use [ZOS](zos-syscalls.md). Bdos, BdosHL, Bios and BiosHL are not native built-ins. |
| CP/M drive/user-area and 8.3 rules | Native paths/device roots and filesystem case behavior; no silent 8.3 truncation. |
| Terminal installation/TINST escape strings | Native Zeal text display and keyboard; [CRT](crt-library.md) handles screen access. |
| `ClrScr`, `GotoXY`, `Delay` and other screen calls are always available | Add `uses Crt;`. Without it these names are compiler error 41. `KeyPressed` and the color names need no Uses clause. |
| CP/M memory/free-space values | Actual owned native memory; unavailable OS capacity queries are reported unavailable. |
| Identical code bytes, addresses, code sizes or timing | Not promised. Native static linking, banked compiler and hardware change them. |
| DOS/CP/M-86 facilities from a combined TP3 manual | Not automatically present. This is the CP/M-80 dialect with documented native additions. |

Never port an absolute CP/M memory/port address by assuming the number identifies
the same object on Zeal. Inline/External code must obey the native Pascal ABI,
restore mappings/registers and use valid native storage.

## Porting checklist

Work through these in order when bringing a CP/M Turbo Pascal 3 program across:

1. Start from source. A CP/M COM or CHN file does not run on Zeal.
2. Add `uses Crt;` after the program heading if the program uses any
   [screen routine](crt-library.md), and call them only in 640×480 text mode.
3. Replace `Bdos`, `BdosHL`, `Bios` and `BiosHL` calls with [ZOS](zos-syscalls.md).
4. Review every `absolute` address, `Mem`, `Port`, `Inline` and `external`. Each
   must be rewritten for Zeal addresses and the
   [native calling convention](language.md#calling-convention-for-external-and-inline).
5. Change filenames to [native paths](runtime.md#file-names).
6. Recompile `overlay` routines to native COM and keep their generated companion
   directory; review [OvrDrive selection and MMU limits](overlays.md).
7. Confirm every file is explicitly closed on every normal path.
8. Confirm recursive routines are compiled under `{$A-}`.
9. Compile, then check [the error reference](errors.md). Do not rely on code
   sizes, addresses or timing measured on CP/M.

## Source-compatible foundations

The language retains 16-bit Integer, Byte, Boolean/Char, six-byte Real, explicit
short Strings, ordinal subranges/enumerations, arrays, packed records/variants,
sets, typed pointers, nested routines, forward declarations, writable typed
constants and the classic declaration/statement syntax. File components and
ordinary data representation retain the documented byte-oriented conventions.

Important inherited behavior includes eager Boolean operations, writable typed
constants, static routine workspace by default, explicit recursive `{$A-}`
workspace, range checking off by default, and the separate recursion stack.
Modern Pascal assumptions about short-circuit evaluation, managed values,
automatic resource finalization or IEEE floating point are inappropriate.

The [language reference](language.md) and [runtime reference](runtime.md) give
exact supported syntax, argument conventions and limitations.

## Deliberate native extensions

- [Units and ZPU libraries](units.md), with interface/implementation sections,
  dependency initialization, selected-code linking and compiled-library lookup.
  They are not binary-compatible with DOS TPU files.
- [Word and LongWord](unsigned-integers.md), with documented unsigned literals,
  casts, arithmetic, formatting and restrictions.
- [ZOS system calls](zos-syscalls.md): checked arguments, staged buffers and raw
  register calls. CP/M call numbers do not map to these selectors.
- [CRT](crt-library.md) and [ZVB](zvb-library.md) libraries for native console,
  graphics, sprites, sound, DMA and hardware registers.
- [Unit native leaves](unit-hardware.md) with explicit memory/calling obligations.
  `Port`, `Mem` and `absolute` are inherited facilities whose addresses refer to
  native Zeal storage/hardware instead of CP/M.
- IDE **A** Save As, native directory creation for source/executable saves and
  source-based saved-program error lookup without a map sidecar.

## Corrections and target adaptations

| Area | Current behavior |
| --- | --- |
| Source lines | All 127 bytes are accepted; longer physical lines are rejected with error 100 rather than silently ignored/truncated. |
| Minimum Integer literal | Decimal `-32768` and Val('-32768') work. The preserved CP/M binary rejects their positive magnitude during parsing. |
| Larger positive literals | Native Word/LongWord types support values beyond signed Integer; this is an extension. |
| Buffered input | A 127-byte native limit, instead of the preserved installation's effective 126-byte limit. |
| Source files | CR/LF/CRLF normalize to LF; trailing ASCII spaces and CP/M EOF padding are handled by source import. Saved source has LF and no record padding. |
| Data files | Typed header/component representation is retained. Native physical allocation need not add CP/M sector padding; untyped operations still count 128-byte records. |
| File control blocks | Native runtime storage; use SizeOf and lifecycle routines, not CP/M offsets or copied descriptors. |
| File safety | Existing Rename targets are rejected; I/O and publication failures are reported rather than hidden. Rewrite still intentionally truncates. |
| Command tails | Preserve case; spaces/tabs separate arguments, literal quotes are not a modern shell quoting grammar. Limit is 127 bytes. |
| Global Find | Selects its match instead of preserving the original's scan-to-EOF defect. |
| Save/load workflows | One Work buffer; save dirty Work before compiling another Main or navigating to another file. Includes come from disk. |
| Error location | Native source identities and generated-code coordinates; use the matching source, compiler and saved bounds for lookup. |
| Nested I/O in Write arguments | Each active I/O call keeps its own file and formatting state, so a function such as `Eof(F)` inside a `WriteLn` argument list does not disturb the outer output. |
| Checked I/O | All I/O routines follow the same `{$I+}` abort policy, including Erase and Rename on logical devices. |
| `Trm` input | Reads immediately with printable echo, as documented. The preserved binary treats it as buffered `Con`. |
| Raw ReadLn | `Kbd`, `Trm`, `Aux` and `Usr` stop at CR without waiting for a following LF. |
| Zero-size file components | Reset/Rewrite report I/O `$90` before touching the file. |

Enumeration storage changes at 257 elements: 256 still fits a byte. This follows
the observed dialect representation rather than ambiguous manual boundary wording.
Runtime/device errors keep meaningful Pascal categories while native ZOS calls
return their own raw or checked status conventions.

## Unsupported or incomplete facilities

CP/M Bdos/Bios variants, replacement
ConInPtr/AuxInPtr/UsrInPtr and ErrorPtr dispatch remain unavailable. The Options
P parameter-tail editor is omitted, matching the preserved CP/M-80 executable's
behavior. Saved-program shell arguments are supported separately.

Saved COM programs support demand-loaded Pascal overlays using MMU RAM banks
and OvrDrive. Their native companion format differs from CP/M. Memory mode still
reports error77; compiled Units containing overlays and Chain/Execute combinations
are rejected. See [overlays](overlays.md) for supported behavior and limits.
CP/M service names remain ordinary unknown identifiers, error41.

Names from later or DOS Pascal are not predefined. The table gives the nearest
Zeal route.

| Later Pascal feature | On Zeal |
| --- | --- |
| Bare `string` | Declare a capacity: `string[80]`. |
| `//` comments | Use `{ }` or `(* *)`. |
| `Halt(Code)` | `Halt` takes no argument. Use the checked ZOS EXIT call for an exit status. |
| `ChDir`, `MkDir`, `GetDir` | `ZOS_CHDIR`, `ZOS_MKDIR`, `ZOS_CURDIR` in [ZOS](zos-syscalls.md). |
| `Append` | No equivalent for Text. Copy the existing lines to a new file, then continue writing. |
| `Sound`, `NoSound` | [ZVB sound](zvb-library.md#sound). |
| `Window`, `TextMode`, `TextAttr` | Not provided. CRT has `TextColor` and `TextBackground`. |
| `MemW`, `PortW` | Compose 16-bit values from byte `Mem`/`Port` accesses, or use `Move`. |
| `ExitProc`, `ErrorAddr`, `ErrorCode` | Not provided. `IOResult` reports ordinary I/O failures. |
| `RandSeed` | Not provided. `Randomize` varies the sequence. |

Later Object Pascal facilities—objects/classes, exceptions, generics, dynamic
arrays, managed/Unicode strings, overloaded routines/operators and unit
finalization—are not supplied. General procedural parameters and aggregate/set
function results are not implemented. Unsigned input/Val/automatic Real
conversions and LongWord For/Case/indexes have the limits documented in the
[unsigned reference](unsigned-integers.md).

There is no debugger beyond the original-style compiler/runtime diagnostic and
source-location facilities. Hardware/display/audio/serial behavior has separate
physical acceptance requirements; emulator checks are not proof for every board.
The ZOS reference documents known pinned-kernel and emulator limitations, including
false overwrite error statuses. These are not hidden by Pascal workarounds.
