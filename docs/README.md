# Zeal Pascal documentation

Start with the [IDE guide](ide.md) to edit, compile, run and save programs.
The [language reference](language.md) and [runtime reference](runtime.md) describe
the current dialect and predefined services. The
[example applications](../examples/README.md) include runnable programs, source
and short demonstrations.

## Find it quickly

| I want to… | Go to |
| --- | --- |
| Install and start the IDE | [Install and start](ide.md#install-and-start) |
| Look up an editor key | [Editor commands](ide.md#editor-commands) |
| Choose RAM, COM, CHN or Unit output | [Compilation modes](ide.md#compilation-modes) |
| Trace a crash back to source | [Errors and source lookup](ide.md#errors-and-source-lookup) |
| Check a grammar rule | [Syntax summary](language.md#syntax-summary) |
| See what a `{$...}` directive does | [Directives](language.md#compiler-directives-and-includes) |
| Look up a built-in by name | [Predefined name index](runtime.md#predefined-name-index) |
| Read input from the console | [Text input and output](runtime.md#text-input-and-output) |
| Read or write a file | [Files](runtime.md#files) |
| Decode an error number | [Errors](errors.md) |
| Port a CP/M Turbo Pascal program | [Porting checklist](compatibility.md#porting-checklist) |
| Write a reusable library | [Creating a unit](units.md#creating-a-unit) |
| Control the text screen | [CRT](crt-library.md) |
| Draw graphics or play sound | [ZVB](zvb-library.md) |
| Call the operating system | [ZOS syscalls](zos-syscalls.md) |

## Build and write Pascal

| Guide | Contents |
| --- | --- |
| [IDE and editor](ide.md) | Startup, menus, editing keys, compilation modes and error lookup |
| [Language](language.md) | Syntax, declarations, types, expressions, statements, routines and directives |
| [Runtime](runtime.md) | Built-ins, files, console, heap, hooks and program control |
| [Errors](errors.md) | Compiler, runtime and I/O diagnostics |
| [CP/M TP3 compatibility](compatibility.md) | Preserved behavior, Zeal extensions, corrections and omissions |
| [Building](building.md) | Building with ZDE, shipped files, test options and stock Units |
| [Pascal units](units.md) | Authoring, importing, linking, memory and limitations |
| [Word and LongWord](unsigned-integers.md) | Unsigned types, operations and conversions |
| [Hardware access](unit-hardware.md) | `Port`, `Mem` and `Inline` in Units; ZPU size limits |

## Libraries and system calls

| Guide | Contents |
| --- | --- |
| [CRT](crt-library.md) | Text console, cursor, color, keyboard and delay |
| [ZOS syscalls](zos-syscalls.md) | Checked calls, buffers, raw registers and kernel caveats |
| [ZVB](zvb-library.md) | Graphics, sprites, compression, sound, CRC32 and DMA |
| [ZVB registers](zvb-registers.md) | Named hardware ports and constants |
| [ZVB demos](../examples/zvb/README.md) | Snake, sprites, file checksums and a CRC/DMA/sound check |

## Testing and real hardware

| Guide | Contents |
| --- | --- |
| [Testing](../tests/README.md) | Native test setup, fixtures and inputs a normal checkout lacks |
| [Hardware acceptance](../tests/hardware/README.md) | Zeal OS requirements, test package and board checklist |

## Scope and status

Zeal Pascal emits native Zeal executables. Units, unsigned types, hardware access
and ZOS syscalls are deliberate Zeal features; they do not imply DOS Turbo Pascal
or Object Pascal compatibility. Current limits and differences appear in the
[compatibility guide](compatibility.md) and individual references. Complete
CP/M TP3 equivalence and physical-hardware qualification are not claimed.

This directory holds guides and references only. Test reports and historical
checkpoint records are not published, and these guides do not depend on them.
