# Zeal Pascal

A native Pascal IDE, compiler and runtime for the MMU Zeal 8-bit Computer.
It follows the CP/M-80 Turbo Pascal 3 source-language style and adds native
Units, Word/LongWord, Zeal hardware access and ZOS system calls. Generated COM
files are native Zeal executables, not CP/M programs.

The IDE, compiler and your programs all run on the Zeal itself. Build and install
it below, write a first program, then continue with the
[user manual](docs/README.md).

## Build and install

Use the existing Zeal development environment and its target toolchain:

```sh
zde cmake
```

For a fresh build that recompiles CRT and ZVB using the newly built compiler,
run `make` on the host. It builds the native prerequisites with six toolchain
jobs, then compiles the Pascal units in two isolated emulator processes and
copies the distributable hardware files into `bin/`. Stage them with
`zde image tf add bin`. See
[the fresh-build workflow](docs/building.md#fresh-compiler-and-pascal-units).

The default CMake build creates the product and its required runtime/stock-unit assets.
Install matching `zpascal.bin` and `zpascal.zpl` from one build, with
`SYSFILE.ZPU`, `CRT.ZPU`, `ZVB.ZPU` and `ZOS.ZPU` beside the bank file.
The [build guide](docs/building.md) lists outputs and maintenance procedures.

From ZShell, the project TF layout starts with `T:/bin/zpascal.bin`. For loose
files an explicit bank path is supported:

```text
H:/zpascal.bin H:/zpascal.zpl
```

The bank file must remain available while using the IDE. Saved programs contain
their selected runtime and Unit code and do not need the IDE or ZPU files to run.
Keep the supplied licenses and credits with redistributed ZVB programs/assets.

## Write a first program

```pascal
program Hello;
begin
  WriteLn('Hello from Zeal Pascal')
end.
```

1. Answer the startup compiler-message prompt with **Y** or **N**. Press **W**, enter `T:/HELLO.PAS`, then press **E** and type the program.
2. Press **Ctrl-K**, then **D**, to return to the menu. The text stays in RAM.
3. Press **R** to compile and run it in Memory, then **S** to save the source.
4. For a standalone program press **O**, **C**, **Q**, then **C** to write a COM.

[Example applications](examples/README.md) include complete runnable programs,
source, input transcripts and instructions. [ZVB demos](examples/zvb/README.md)
cover graphics, sprites, sound, DMA and CRC32.

## Documentation

| Guide | Use it for |
| --- | --- |
| [IDE and editor](docs/ide.md) | Menus, every editor key, compile modes, saving and error lookup |
| [Pascal language](docs/language.md) | Syntax, types, operators, statements, routines, directives and machine access |
| [Runtime](docs/runtime.md) | Every predefined routine and variable, console input, files and memory |
| [Errors](docs/errors.md) | Compiler, runtime and I/O codes, with common causes |
| [CP/M TP3 compatibility](docs/compatibility.md) | Porting checklist, differences and unsupported features |
| [Units](docs/units.md), [CRT](docs/crt-library.md), [ZVB](docs/zvb-library.md), [ZOS](docs/zos-syscalls.md) | Libraries, screen, graphics, sound and system calls |

## Scope and limitations

The compiler, editor and runtime use native Zeal APIs and memory layouts.
Complete historical Turbo Pascal equivalence, compile-speed parity and all
physical-hardware behavior are not claimed. See [compatibility](docs/compatibility.md)
for deliberate differences and unsupported features, and [ZOS](docs/zos-syscalls.md)
for known pinned-kernel/emulator limitations. Compiler/runtime errors retain the
original-style source-location workflow; a modern debugger is not supplied.

Before physical use check [Zeal OS requirements and hardware acceptance](tests/hardware/README.md).
Emulator results do not establish physical display, audio, controller or serial
behavior. Programs must explicitly Close Pascal files to finalize buffered data.

## Development and testing

Native builds use `zde cmake`. Tests and experimental targets are opt-in with
`ZP_BUILD_TESTS=ON`; see [building](docs/building.md#opt-in-test-targets).
The [testing guide](tests/README.md) describes the acceptance runner, fixtures,
CP/M reference prerequisites and result locations. The scheduled commands and
native prerequisites are defined in [the test plan](tests/acceptance/test-plan.json).

[GitHub builds and releases](docs/releasing.md) explains building a downloadable
distribution and releasing its exact build commit with a chosen version.

User documentation is maintained in `docs/`. Generated test output belongs in
`analysis/` or `build/`. Research material is maintainer-local and is not
distributed with the repository.
