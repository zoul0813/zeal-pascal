# Pascal units and ZPU libraries

This page describes what Pascal units support **now** in this worktree. Earlier
checkpoint and proposal documents carry a *Historical* banner. They record the
limits of their own stage; where they disagree with this page, this page is
current.

Units are a native Zeal extension in the style of Turbo Pascal 4–7. They do not
read TPU files, and they do not complete the recovered Turbo Pascal 3
specification.

## Files at a glance

| File | What it is | Needed for |
| --- | --- | --- |
| `zpascal.bin` | IDE launcher | Running the IDE |
| `zpascal.zpl` | Compiler/IDE bank container | Running the IDE. It is not a unit and is never imported |
| `NAME.ZPU` | One compiled Pascal unit | Compiling a program that declares `uses Name` |
| `NAME.PAS` | Unit source | Rebuilding the unit and source navigation; not needed for import or execution |
| `PROGRAM.COM` | Saved program | Running it. Needs none of the files above |

Linking is static. The compiler reads a ZPU only while compiling a client and
copies the reachable code and data into the program. A saved COM runs after
every ZPU, PAS and compiler file has been removed.

**Real arithmetic is not a unit.** There is no `uses Real` and no `REAL.ZPU`.
The compiler selects its internal Real component when retained operations need
it. Real arithmetic and formatting require it; raw Real storage/copy does not.
`Inline` and `External` do not implicitly request Real. Native code that
calls internal Real services must request `{$RUNTIME REAL}` explicitly.
Program transfers still retain the full runtime to preserve the borrowed-runtime
Chain contract.

## Program memory layout

A COM is a contiguous byte stream: runtime, globals, then selected program and
unit code. The linker assigns final addresses and rewrites symbolic references.
Non-Real programs use a 6,042-byte runtime prefix rather than a padded 16 KiB
prefix, recovering 10,342 bytes. There is no empty Real reservation. Real
programs include the actual full runtime and its state.

Execution uses one, two or three physical 16 KiB pages according to the image,
stack and control-space requirements. Small ordinary programs can share one
physical page across the three CPU windows. Raw memory/I/O, explicit heap
inspection, interrupt handlers and program transfers retain independent windows.
RAM Run allocates only the distinct pages it needs. For a cold COM launch,
Zeal’s loader initially supplies three pages; program startup returns the
surplus pages before entering Pascal code. These are hardware mappings and do
not add page padding to saved binaries. The IDE/compiler retain their own
separate pages while using RAM Run.

File operations link selected helpers from the compiler-private `SYSFILE.ZPU`.
Those helpers call Zeal's file APIs directly on the normal program stack and
mapping. They neither unpack a runtime module nor allocate a backing page.
Keep `SYSFILE.ZPU` beside the loaded `zpascal.zpl` when compiling file programs;
saved programs need neither file. This unit is an implementation detail, not a
Pascal `uses` interface. Lookup uses only the loaded compiler library’s directory;
a project-local file cannot shadow it. A missing file reports error 250 and a
file with a different stock checksum reports error 251. The name `SysFile` is
reserved; rebuild and ship it together with the compiler.

## Creating a unit

```pascal
unit Calc;
interface
function Twice(X: Integer): Integer;
implementation
function Twice(X: Integer): Integer;
begin Twice := X * 2 end;
end.
```

1. Write the unit and save it with **S**.
2. If **M Main** names another file, select **M** and submit an empty name, so
   that the unit in Work becomes the compilation root.
3. Select **O → U (Unit-file) → Q**, then press **C**. The declared name sets
   the output: `unit Calc;` writes `CALC.ZPU` beside its source, whatever the
   source file is called. **R** does not run a unit; it explains that a program
   must use it.

Keep each unit in **one source file**. Unit authoring records a single source
identity, so do not split a unit with `{$I}` includes. `libraries/ZVB.PAS` has
include files only because the stock ZVB build flattens them into one source
before compiling.

## Using and rebuilding a unit

```pascal
program UseCalc;
uses Calc;
begin WriteLn(Twice(21), ' ', Calc.Twice(2)) end.
```

Select Memory or Com-file mode, then **C** to compile or **R** to compile and
run. Com-file mode writes the COM beside the program's Main source. Saving the
source (**S**) and publishing a COM are separate actions.

**Lookup.** For `uses Calc`, the compiler tries `CALC.ZPU` beside the
compilation root, then beside the `zpascal.zpl` actually loaded, including a
command-line override. Only a *missing* local file falls back. A local file that
is invalid, stale or unreadable is an error. Install stock units beside
`zpascal.zpl`.

**Rebuilding.** Clients never recompile a unit's PAS file. After editing a
unit, rebuild its ZPU. Then rebuild every unit that uses it, in dependency
order: each ZPU records the exact compiled dependencies it was built against,
and a changed dependency makes its users stale.

**After an error inside a unit.** Main still names the client program. To
rebuild the unit, clear Main (step 2 above) and compile in Unit-file mode.

## What a unit can contain

| Section | Supported | Not supported |
| --- | --- | --- |
| `interface` | `uses`; ordinary `const`; `type`; procedure and function headings | `var`; typed constants |
| `implementation` | `uses`; private constants, types, variables and typed constants; helper routines; an optional final `begin ... end.` body | Finalization |

- **Constants:** Integer, Boolean, Char, String, Real, Word and LongWord values.
  Importing a constant links no code.
- **Types:** aliases, enumerations, ordinal subranges, fixed strings, arrays,
  records with variants, sets, pointers and file types. An imported alias keeps
  its original identity, so a record passed through an alias in another unit is
  still Var-compatible. Distinct records and enumerations stay distinct.
- **Routines:** procedures and functions under the normal Pascal parameter and
  result rules. Call them unqualified or qualified, as in `Calc.Twice`.
- **Hardware:** `Port`, `Mem`, private `absolute` variables, Word/pointer
  conversions and `Inline` with relocated storage operands. See
  [hardware access](unit-hardware.md).

**Public storage.** An interface cannot declare a variable or typed constant,
because both are addressable storage that clients would share. Keep the
variable in the implementation and export routines that read and set it.

## What a unit body cannot do yet

Unit code runs without the helpers the compiler generates for an ordinary
program. These operations are rejected when the unit is compiled, rather than
being linked to missing runtime code:

- console and Text `Read`/`Write`;
- typed and untyped file operations, even on an exported file type;
- heap and stack manipulation;
- console and device callback hooks;
- opaque `External` routine declarations.

Fixed runtime services such as `IOResult`, and the supported memory operations
`Move` and `FillChar`, remain available. If the compiler rejects something in a
unit, do that work in the program and pass the data through parameters. The
stock `Crt` unit is built by a separate host tool, so its screen operations are
not a model for what a Pascal-authored unit may do.

## Dependencies and initialization

- `uses` may appear at the start of the interface and after `implementation`.
  Missing, stale and cyclic dependencies each have their own diagnostic.
- Namespaces are not re-exported. A program using `A` cannot name `B` merely
  because `A` uses `B`; it must declare `uses B` itself.
- Each unit is loaded once per compilation, so all users share one copy of its
  private state.
- Initialization bodies run before the program block, dependency-first and in
  `uses` order, once per program activation. Each Memory **R** runs them again.
  State is not reset except by the body's own assignments.
- An initializer is always linked, even when no routine from its unit is
  called. Keep it small.
- A CHN program has its own copy of each unit and runs its own initializers. It
  does not share unit state with the program that chained to it.

## Memory: compile time and run time

These are two separate budgets.

**While compiling**, in the IDE's RAM:

- the compiler's cached code: 10 pages (160 KB) at a cold start and 15 (240 KB)
  in the warm compiler cache;
- metadata for each imported unit, which stays resident while code streams from
  disk through a 64-byte buffer. ZVB's metadata is 29,338 bytes, in two 16 KB
  pages;
- source, the module catalog, symbol and type tables, maps and output staging.

Available physical RAM limits all of these. Code/table capacity exhaustion
reports error 99, `Compiler overflow`. Declaration-size and variable-storage
limits can instead report error 98, `Memory overflow`; a failed allocation or
I/O operation can retain its platform error.

**In the running program**, only reachable code and data are present:

- Non-Real programs use the compact 6,042-byte prefix. Real programs retain
  the actual 16,383-byte full engine and state; neither layout pads to a page.
- Code, globals and generated helpers must end at or before `$F400`. That leaves
  at least 2 KB of stack below the default `$FC00` stack top. Exceeding it
  reports error 98 for variable storage or error 99 for code capacity.
- Small programs can use one physical page; larger images or recorded stack
  demand select two or three. Known stack demand is recorded in the program
  header, with a minimum 2 KB and 512 bytes of helper headroom. Runtime guards
  stop deeper calls before they reach static storage. Unit initializers, large
  unit-frame contracts, retained raw hardware/heap access and program transfers
  retain independent windows. Calls from a Pascal routine and unresolved forward
  calls also retain independent windows, preserving the existing recursion and
  nesting capacity. A root call to a defined leaf can use its recorded budget.
  The existing CPU-stack window remains `$C800`–`$FC00`; recursive depth is
  bounded by the actual frames and runtime guards. Importing an uncalled routine
  adds nothing. The
  [earlier page measurements](program-pages.md) are historical.

### Limits

| Resource | Limit |
| --- | --- |
| One ZPU file | 65,470 bytes, including code and initial globals |
| Unit code while authoring | Up to address 65,534. A unit can be larger than any one program, which links only what it reaches |
| Modules loaded in one compilation | 32, if physical RAM allows. Several large units may not fit |
| Dependencies recorded in one ZPU | 31 |
| Format | `ZPU1`, ABI1, unreleased. No readers for older experimental layouts; rebuild ZPUs after a compiler or runtime change that invalidates them |

### Current measurements

| Artifact | Size |
| --- | --- |
| `zpascal.zpl` | 276,597 bytes; resident launcher 16,115 bytes |
| `ZVB.ZPU` | 60,330 bytes: 30,578 code, 908 globals, 28,844 retained metadata |
| Snake demo COM | 35,699 bytes from Com-file compilation, with **10,381 bytes** of static address space free |

The following are distinct **program execution pages**, excluding the IDE and
compiler. RAM Run and cold standalone execution produced the same counts.

| Program | COM bytes | Pages |
| --- | ---: | ---: |
| Minimal | 6,655 | 1 |
| Integer | 6,762 | 1 |
| Real | 17,291 | 2 |
| Integer unit | 7,005 | 1 |
| CRT | 7,462 | 1 |
| Block file I/O | 19,787 | 2 |
| Typed file I/O | 19,428 | 2 |
| Text file I/O | 21,767 | 2 |

The [hash-bound evidence](runtime-compaction-evidence.json) records the native
checks, compiler bank sizes and known limits. Raw hardware programs such as
Snake retain three independent windows; their recovered space is available to
code and globals rather than left empty in the first page.

## Stock units and distribution

TF packaging installs these files beside `T:/lib/zpascal.zpl`:

| File | Contents |
| --- | --- |
| `SYSFILE.ZPU` | Private statically linked file helpers, used automatically by the compiler |
| `CRT.ZPU` | Screen, cursor, color, `Delay` and `ReadKey`; see the [CRT reference](crt-library.md) |
| `CRT.PAS` | Interface reference only, not the implementation source |
| `ZVB.ZPU` | Zeal Video Board graphics, sprites, CRC32, sound and DMA; see the [ZVB reference](zvb-library.md) |
| `ZVB.LIC` | Apache-2.0 license for ZVB |
| `ZVB.NTC` | ZVB SDK attribution and Pascal changes |

**Redistribution.** A saved program that uses ZVB contains ZVB code. Ship
`ZVB.LIC` and `ZVB.NTC` with it. The Snake demo also embeds JROB774's Pixel Block
Font under CC BY 3.0; keep its title, author, adaptation note and license and
source links from the [asset credits](../examples/zvb/CREDITS.md).

ZVB's source is `libraries/ZVB.PAS` with private include files. Maintainers
rebuild and test it, and package the TF card, using the
[demo maintenance steps](../examples/zvb/README.md#maintaining-the-stock-unit)
and the existing ZDE environment.

## Errors without source

A runtime error in unit code still reports its error code and PC when no source
is present. **O → F** and compile-error navigation open the unit's source only
when its text matches what was compiled. If the text is missing or has changed,
Work is left untouched.

## Verification status

Results come from the native Zeal emulator, plus sanitized host tests of
components. Display, audio and DMA timing have **not** been qualified on a
physical board.

Test evidence: [unit storage](unit-storage-evidence.json),
[unsigned integers](unsigned-integers-evidence.json) and
[ZVB](zvb-library-evidence.json). Review decisions:
[hardware/compiler](../reviews/hardware-units-v1.md),
[unsigned scalars](../reviews/unsigned-scalars-v1.md),
[ZVB code](../reviews/zvb-pascal-v1.md) and
[documentation](../reviews/zvb-docs-v1.md). Test results and review records are
kept separately; an evidence file is not an approval.

## Not yet supported

- public variables and typed constants in an interface;
- console, file, heap and callback services inside unit bodies;
- unit source split across include files;
- finalization, and unit state shared between a program and its CHN.

These are open design questions. None is enabled by this guide.
