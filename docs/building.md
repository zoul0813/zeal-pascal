# Building and installing Zeal Pascal

For normal use, download the prebuilt ZIP from the
[latest release](https://github.com/zoul0813/zeal-pascal/releases/latest) and
follow the [README installation steps](../README.md#download-and-install).
Source builds use the ZDE target toolchain and compile the Pascal stock Units
in the native Zeal emulator.

## Install the distribution

The release ZIP contains `bin/` and `examples/`. The files in `bin/` are the
same installation bundle produced by a successful local `make`. The bundle
contains `bin/apps/zpascal` and supporting files under `bin/lib/`. Create
`T:/apps/` and `T:/lib/` on your Zeal TF card if needed. Copy `bin/apps/zpascal`
into `T:/apps/`, and the contents of `bin/lib/` into `T:/lib/`. The installed
layout is:

```text
T:/apps/zpascal
T:/lib/zpascal.zpl
T:/lib/SYSFILE.ZPU
T:/lib/CRT.ZPU
T:/lib/ZVB.ZPU
T:/lib/ZOS.ZPU
T:/lib/CRT.PAS
T:/lib/ZVB.PAS
T:/lib/ZOS.PAS
T:/lib/ZVB.LIC
T:/lib/ZVB.NTC
```

Launch from ZShell:

```text
T:/apps/zpascal
```

Without a bank-path argument, the launcher first looks for `lib/zpascal.zpl`
under the current working directory, then uses `T:/lib/zpascal.zpl` if the local
file is missing. Keep the four ZPUs
beside that bank file and install matching files from one release or build.
Keep the bank file available throughout an IDE session. Saved COM programs
contain their selected runtime and Unit code and run independently.

For a portable installation, keep `apps/` and `lib/` under the same
installation root and change to that root before launching `apps/zpascal`. Relative lookup uses the working directory,
not the executable's directory. A corrupt or incompatible local bank file
reports an error without falling back.

An explicit bank path overrides both search locations, for example
`H:/apps/zpascal H:/lib/zpascal.zpl`.

The [distributable file reference](#distributable-hardware-files) explains source
lookup and licensing. See [hardware requirements](../tests/hardware/README.md)
for physical Zeal use, and [TF-image setup](#emulator-tf-card-image) for
an emulator card image.

## Shared installation layout

The normal `zde cmake` build and `make` both produce one generated `bin/`
installation root, containing `apps/zpascal`, `lib/zpascal.zpl`, stock units,
Pascal interfaces and ZVB license/attribution files. Both builds generate
CRT and ZVB from Pascal source using `zeal-headless`.

For HostFS, run `zeal-native -H bin/`. From `H:/`, launch
`H:/apps/zpascal`; its relative `lib/zpascal.zpl` lookup finds `H:/lib/`.
From another working directory, pass `H:/lib/zpascal.zpl` explicitly.

For Software Collection staging, use `build.root: /` and the single directory
artifact `bin/`. Its contents are merged into the TF root,
giving `T:/apps/zpascal` and `T:/lib/`. The installed launcher is extensionless
so ZShell can find it as `zpascal` on the community PATH. You can also launch
it by its full path.

The distribution target runs after the shipping outputs, validates that the
launcher and bank file match, and replaces the generated installation tree.
Do not store personal files in `bin/`. Failed input validation preserves the
previous tree. Publish these Pascal build changes before merging the SC entry.

## Source-build prerequisites

Use [ZDE](https://github.com/zoul0813/zeal-dev-environment) with its container
runtime running and Zeal OS installed. The
[build workflow](../.github/workflows/build.yml) uses ZDE's `main` branch and
its default `latest` runtime image. For builds that run unit compilers inside
ZDE, install its headless dependency once:

```sh
zde --no-tty deps install Zeal8bit/Zeal-NativeEmulator
```

## Fresh compiler and Pascal units

From the `zeal-pascal` checkout, choose where to run the unit compilers:

| Command | Unit compilation and staging |
| --- | --- |
| `zde --no-tty make dist JOBS=6` | Inside ZDE, using its `zeal-headless` and Python |
| `make dist JOBS=6` | Inside ZDE, launched through the host Makefile |
| `make dist UNIT_RUNNER=host JOBS=6` | On the host, using a native `zeal-headless` on `PATH` and host Python |

All three commands build the Z80 compiler, SYSFILE, ZOS and the emulator boot
ROM with ZDE's toolchain. The build generates `build/unit-rom.img` from Zeal OS's
HostFS-enabled configuration in a private copy of the OS sources. An existing
emulator ROM or a separate `zde kernel` step is unnecessary.

`JOBS` defaults to 6 and sets the native compiler's worker count.
`UNIT_JOBS` defaults to `JOBS` and can be set independently, for example
`make dist UNIT_RUNNER=host JOBS=4 UNIT_JOBS=2`. CRT and ZVB each run in a
separate emulator process, so two unit workers can run at once. Host mode runs
those processes outside the container VM. Set `PYTHON` for a different host
interpreter and `ZDE_PYTHON` for a different ZDE interpreter.

The build compiles CRT and ZVB from source with the just-built Pascal compiler
and stages the production distribution under `bin/`. It retains the exact
prepared sources next to the fresh `build/CRT.ZPU` and `build/ZVB.ZPU`. Failed
unit builds return nonzero and preserve the staged distribution. `make native`
and `zde make native` build only the compiler, SYSFILE and ZOS. Run one build
per checkout at a time so its unit inputs remain stable.

The Software Collection can use the ZDE Make path with `build.tool: make`,
`dist` in `build.args`, and a dependency on `Zeal8bit/Zeal-NativeEmulator`.
This uses the same `bin/` artifact and root as the CMake staging path.
See [GitHub builds and releases](releasing.md) for downloadable artifacts.

The acceptance tools use the separate ZDE emulator ROM, kernel map and media
fixtures in the ZDE installation. For host acceptance runs, provide a native
`zeal-headless` on `PATH` and prepare the acceptance ROM with
`zde --no-tty kernel zeal8bit/emu`.

To rebuild and test the general examples after the distribution build finishes:

```sh
zde --no-tty cmake build --target zexampletest_bin -j6
python3 tests/acceptance/test-examples.py --package
```

The harness uses the source-built `build/CRT.ZPU`. ZealCalc has a separate build and acceptance command:
`python3 tests/acceptance/test-zealcalc.py --package`.

To compile another Pascal unit with the same compiler:

```sh
zde --no-tty exec python3 tools/compile-unit.py path/to/Example.pas \
    --rom build/unit-rom.img --output build/EXAMPLE.ZPU
```

The script expands relative `{$I file}` and `(*$I file*)` includes into one
source file, as required by Unit-file mode. Include expansion is textual;
compiler directives remain in the flattened source. `--compact` additionally
removes ordinary comments and empty lines to reduce source RAM use; compiler
directives, string literals and SPDX comments remain. The Makefile uses this
option for ZVB. Native source-size, line-length and unit-feature limits still apply.

Compiled dependencies are searched beside the source, then in repeated
`--unit-path DIRECTORY` locations, then in `build/`. Build dependencies first;
the script does not compile them recursively. Each invocation has independent
HostFS, ROM, TF and EEPROM copies and retains its source and logs beneath
`build/unit-runs/`. The build script and acceptance tests share the emulator
automation in `tools/zeal_emulator.py`; unit builds do not load the acceptance
workflow. Runs are retained for diagnostics and can be removed when no builds
are using them. `--source-output PATH` also publishes the exact prepared
source for source/error lookup. Use that source when includes or compaction
change offsets. `--emulator PATH` selects another `zeal-headless` executable.
`--rom PATH` supplies the boot ROM, and `--timeout SECONDS` overrides the
1800-second emulator limit. With no `--rom`, standalone tools use the emulator
ROM from the existing ZDE installation. Inside ZDE they use `ZDE_HOME`. On the
host they look for an ancestor containing `zeal-dev-environment`, with
`ZP_ZEAL_ROOT` available to override that lookup.

The script checks successful emulator completion and the fresh ZPU's header,
name, size and checksum before replacing output atomically. Stock builds also
save build-local records that bind the payload to the prepared source and the
compiler. Keep the resulting `.ZPU` files and records in the build directory.

## Distributable hardware files

A successful `make` (or `make dist`) finishes by copying these files to `bin/`:

- `apps/zpascal`: native IDE/compiler launcher.
- `lib/zpascal.zpl`: matching compiler banks.
- `lib/SYSFILE.ZPU`, `lib/CRT.ZPU`, `lib/ZVB.ZPU`, `lib/ZOS.ZPU`: stock statically linked Units.
- `lib/CRT.PAS`, `lib/ZVB.PAS`, `lib/ZOS.PAS`: matching Pascal sources/interfaces for rebuilding and error lookup.
- `lib/ZVB.LIC` and `lib/ZVB.NTC`: the ZVB license and attribution notice.

The six binary files are required for the IDE/compiler installation; the Pascal
sources support development and source lookup. Retain `ZVB.LIC` and `ZVB.NTC`
when distributing ZVB. Source navigation currently requires matching text at the
source path recorded inside the ZPU. The IDE does not yet search beside the
installed ZPU, so copying the Pascal sources to a different TF-card path alone
does not enable navigation. The distributed ZVB.PAS is the exact flattened,
compacted file compiled by the build, rather than the repository's include tree.

`bin/` is generated and ignored by Git. Both build paths replace this tree
only after validating all inputs; old layout files are removed as part of that
replacement. Keep personal files elsewhere.

## Emulator TF-card image

Install the release or locally built `bin/` files on the emulator's TF image
using the [same directory layout](#install-the-distribution) as a physical card.
For ZDE image staging and creation, see the
[ZDE image reference](https://github.com/zoul0813/zeal-dev-environment/blob/main/docs/image.md).
The same `bin/` directory can be staged recursively with ZDE.
The separate `zde image tf add bin` command copies only top-level files and
does not install this directory tree. Community image staging is a separate
workflow.

The separate [`tools/package-tf.py` workflow](#project-tf-card-image) below reads
`build/` and checks the CRT and ZVB build records against the source, compiler
and compiled units. It installs the launcher in `T:/bin/` and
the supporting files in `T:/lib/`.

## Build-output details

Keep the four ZPU files beside the loaded ZPL. Include `ZVB.LIC` when distributing its Unit. Other runtime ZPLs, IHX,
REL, map files, headers and JSON files in `build/` are intermediate build assets;
they are not additional files the user needs to install.

In a release configuration, the only top-level `.bin` is `zpascal.bin`.
`build/assets/` holds the raw integer runtime fragments, optional Real patch
and linked integer verification image. The integer head/tail and Real patch are
embedded into compiler assets. `linked_integer_runtime.bin` is an independent
byte-comparison image emitted by the layout generator for verification; it is
not embedded or installed. None of these raw files is an application.
The `ram_trampoline_bin` target is an SDK conversion-helper name: it produces
`ram_trampoline.zpl`, a 358-byte transition stub embedded in the compiler for
Memory Run and return to the IDE. It is not another installed program.

## Compiling and adding a Pascal unit

1. Add the source under `units/`, for example `units/example/EXAMPLE.PAS`,
   declaring `unit Example;`. The supported native Unit features are described
   in [Units](units.md). The output basename must match the declared name,
   case-insensitively: `EXAMPLE.ZPU` for `unit Example`.
2. Run `zde --no-tty make units` to build the current compiler, ROM and stock units.
3. Compile the new unit, retaining the exact prepared source for error lookup:

   ```sh
   zde --no-tty exec python3 tools/compile-unit.py units/example/EXAMPLE.PAS \
       --rom build/unit-rom.img --output build/EXAMPLE.ZPU --source-output build/EXAMPLE.PAS
   ```

   Relative includes are flattened. `{$Idefs}` uses `defs.PAS` by default;
   `{$I defs.inc}` uses the given extension. Add `--compact` only when needed
   to reduce source memory. Compile imported units first; additional compiled
   dependency directories can be supplied with repeated `--unit-path DIR`.
   A publication failure normally restores prior outputs; if rollback also
   fails, the command reports any retained original recovery file. Resolve the
   failure before packaging.
4. Run the unit's relevant behavior tests and compile a program that uses it.
5. To ship a new Pascal unit, register it with `zp_pascal_unit` in
   `cmake/Units.cmake`, add its recipe to the host unit build in `Makefile`,
   and list the distributable files in `tools/stage-distribution.py`. Declare
   any imported units as prerequisites in both build graphs so their outputs
   exist before compilation.

## Project TF-card image

After building the matching compiler and stock units, create or refresh the
project's TF image with:

```sh
python3 tools/package-tf.py
```

Use `build/tf.img` as the emulator's TF card, then launch `T:/bin/zpascal.bin`
from ZShell. The image installs the matching bank file and units under
`T:/lib/`, matching the launcher's default path.
Initial creation uses the existing development environment's base image.
Refresh preserves other card files and checks their contents; it does not alter
your separate system image. An existing project image is backed up under
`build/recovery/` before replacement. Close an emulator using the project card
before refreshing it. Building loose files and refreshing this image are
separate steps.

## Build targets

The default CMake build generates the production compiler, stock units and
distribution. CRT and ZVB use the same unit compiler as the Make paths and
are regenerated when their build inputs change.

```sh
zde cmake
```

Use the `--parallel` option to build independent targets concurrently. The
`pascal_units` target builds CRT and ZVB together; `zpascal_bin` builds just
the compiler and its native prerequisites without an emulator.

The default build has six product entry points:

| Target | Output |
| --- | --- |
| `zpascal_bin` | `zpascal.bin` and its matching `zpascal.zpl` |
| `crt_zpu` | `CRT.ZPU` and `CRT.PAS` |
| `sysfile_zpu` | `SYSFILE.ZPU` |
| `zvb_zpu` | `ZVB.ZPU` and its prepared `ZVB.PAS` |
| `zos_zpu` | Native syscall `ZOS.ZPU` and its `ZOS.PAS` interface |
| `distribution` | Shared `bin/apps/` and `bin/lib/` installation tree |

The root `CMakeLists.txt` sets up the project and build layout, includes
four production modules, and conditionally includes `cmake/Tests.cmake`:

- `Sources.cmake`: source lists, bank placement and header dependencies.
- `Runtime.cmake`: numeric runtime templates, execution handoff and asset generation.
- `Compiler.cmake`: compiler object groups and BIN/ZPL linking.
- `Units.cmake`: CRT, SYSFILE, ZVB and ZOS generation or staging.

`Units.cmake` includes `SysFile.cmake` for the file backend's build steps.
The assembler and standard linker build `units/sysfile/` directly into a ZPU
byte stream, including its exports, fragments and relocation tables. SYSFILE
has no production C routines, SDK archive extraction, or custom object linker.
A small host C utility, `build/assets/sysfile-seal`, adds the required content
checksum and runtime fingerprint; it does not generate, place or relocate code.
The assembly preserves the existing private calling convention internally and
uses direct Zeal OS syscall adapters. Several hot helpers are hand-tuned; moving
code to ASM does not imply every retained stack frame has been optimized away.
Other compiler/runtime asset builders and the stock Pascal unit tools use Python.

Internal targets are excluded from `ALL` and run when
required by a product dependency. CMake still reports those compile/link steps
as “Built target …”; those messages do not identify extra shipped programs.

Conversions and packaging use file-based rules in `Outputs.cmake`. An unchanged
`zde cmake --parallel 4` skips compilation, linking and asset/header generation;
changing a generator input or deleting an output schedules the required work.
CRT and ZVB are recompiled when their build inputs change or an output is
missing. The `distribution` target runs each time, validating the unit records
and other inputs before replacing the staging tree.
“Built target …” alone does not mean compilation or linking ran.
ZDE/VM startup and dependency checks still contribute to elapsed time.

The old full `file_runtime` module is not part of the production graph or
runtime fingerprint. Its builders live in `Tests.cmake` for explicit legacy
probes. Production file support is generated as selectively linked SYSFILE.ZPU
functions from `units/sysfile/`. The C implementation in
`src/runtime/files/operations.c` remains for legacy probes and the host behavior
oracle. The direct ASM backend also runs the existing native file conformance suite.
`runtime_profile` fingerprints the numeric runtime, handoff assets and compiled
SYSFILE payload. SYSFILE compiles once into a payload with a neutral fingerprint;
its final ZPU header is stamped after that payload has been fingerprinted.
Stamping verifies the neutral payload's checksum and SHA-256 report before
publishing the bound Unit and compiler-private checksum header. This preserves
selective linking and adds no code or memory to generated programs.

The BIN and ZPL are produced together so they remain a matching compiler pair.

## Source ownership

| Directory | Responsibility |
| --- | --- |
| `src/app/` | Startup and bank-call dispatch |
| `src/compiler/` | Parsing, types, code generation, final linking and compiler jobs |
| `src/units/` | ZPU reading, metadata, symbols and fragment linking |
| `src/runtime/` | Generated-program runtime, numeric/file operations and process handoff |
| `src/editor/` | Editor core, IDE commands and display |
| `src/source/` | Shared source catalog, buffers, editing and validation |
| `src/platform/` | Zeal memory/I/O, keyboard, bank loader and parent worker services |
| `src/storage/` | Source/program file publication and transport |
| `src/program/` | Program image encoding and validation |
| `src/common/` | Shared declarations and pure path helpers |

Compiler subgroups include `codegen/builtins`, `codegen/unsigned`, `link`,
`tables`, `groups` and `jobs`. The type parser remains `compiler/types.c`;
create a types directory when there is a meaningful group to put there.
Compiler phase policy lives in `compiler/phase.c`, while physical bank loading
lives in `platform/banks/`. Final compiler linking deliberately consumes the
ZPU fragment model defined under `src/units/linker/`.

Use includes relative to the `src/` include root, such as
`compiler/internal.h` and `common/zp.h`. Source directories express ownership;
CMake still controls bank placement independently. Preserve source/object list
order when changing paths because that order affects the linked image.

`src/units/` implements the compiler's Unit support. The Pascal Unit sources
users can compile or use remain in the top-level `units/` tree, including
`units/zvb/`.

## Opt-in test targets

`ZP_BUILD_TESTS` defaults to `OFF`. The root CMake file includes the product definition;
`cmake/Tests.cmake` is included only when this option is on. It contains native
acceptance executables, legacy/experimental compiler variants, independent
runtime probes and generated test fixtures.

Enable them explicitly through ZDE:

```sh
zde --no-tty exec cmake -S . -B build -DZP_BUILD_TESTS=ON
zde --no-tty cmake build --target zptest_bin zzvbtest_bin -j4
python3 tools/run-tests.py --only native:basic
```

The central test runner enables the option itself before building its selected
native prerequisites declared in `tests/acceptance/test-plan.json`. Every planned driver
has an explicit prerequisite list, including the host suite’s shared ABI fixtures. Direct Python test drivers normally expect those binaries
to have been built. The fragment, low-file and Unit-corpus drivers that perform
their own builds also enable the option explicitly.

CMake options persist in the build cache. After testing, return to a product-only
configuration with:

```sh
zde --no-tty exec cmake -S . -B build -DZP_BUILD_TESTS=OFF
zde cmake
```

Turning the option off removes test targets from the build graph and archives
recognized stale test executables, banks and link metadata together under a new
`build/tests/<generation>/` directory. Existing snapshots are preserved. These
are workspace snapshots, not newly validated packages; remove unneeded snapshots
manually. Other binaries, sources,
directories and symlinks are left alone. Required runtime assets still build;
turning tests off does not disable the compiler's runtime generators. Use a fresh
build directory when checking a clean release build. Reconfiguring is through
`zde exec`; actual target builds use `zde cmake`.

## Testing the stock Pascal units

The default build compiles CRT and ZVB with the production Pascal compiler.
To qualify changes with the extended native harnesses:

```sh
zde --no-tty exec cmake -S . -B build -DZP_BUILD_TESTS=ON
zde --no-tty cmake build --target distribution zzvbtest_bin zscreentest_bin -j4
python3 tests/acceptance/test-zvb.py --skip-hardware --prebuilt build/ZVB.ZPU --clients
python3 tests/acceptance/test-screen.py
python3 tests/acceptance/test-crt-readkey.py
```

`tools/measure-crt.py` compares the Pascal implementation with the historical
emitter using the same current compiler/runtime. Restore `ZP_BUILD_TESTS=OFF`
for release builds.
