# Building and installing Zeal Pascal

For normal use, download the prebuilt ZIP from the
[latest release](https://github.com/zoul0813/zeal-pascal/releases/latest) and
follow the [README installation steps](../README.md#download-and-install).
Source builds use the ZDE target toolchain and compile the Pascal stock Units
in the native Zeal emulator.

## Install the distribution

The release ZIP contains `bin/` and `examples/`. The files in `bin/` are the
same installation bundle produced by a successful local `make`. The bundle
contains `bin/zpascal.bin` and supporting files under `bin/lib/`. Create
`T:/bin/` and `T:/lib/` on your Zeal TF card if needed. Copy `bin/zpascal.bin`
into `T:/bin/`, and the contents of `bin/lib/` into `T:/lib/`. The installed
layout is:

```text
T:/bin/zpascal.bin
T:/lib/zpascal.zpl
T:/lib/SYSFILE.ZPU
T:/lib/CRT.ZPU
T:/lib/ZVB.ZPU
T:/lib/ZOS.ZPU
T:/lib/CRT.PAS
T:/lib/ZVB.PAS
T:/lib/ZOS.PAS
T:/lib/ZVB.LIC
```

Launch from ZShell:

```text
T:/bin/zpascal.bin
```

Without a bank-path argument, the launcher first looks for `lib/zpascal.zpl`
under the current working directory, then uses `T:/lib/zpascal.zpl` if the local
file is missing. Keep the four ZPUs
beside that bank file and install matching files from one release or build.
Keep the bank file available throughout an IDE session. Saved COM programs
contain their selected runtime and Unit code and run independently.

For a portable installation, keep `zpascal.bin` and `lib/` together and change
to that directory before launching. Relative lookup uses the working directory,
not the executable's directory. A corrupt or incompatible local bank file
reports an error without falling back.

An explicit bank path overrides both search locations, for example
`H:/bin/zpascal.bin H:/lib/zpascal.zpl`.

The [distributable file reference](#distributable-hardware-files) explains source
lookup and licensing. See [hardware requirements](../tests/hardware/README.md)
for physical Zeal use, and [TF-image setup](#emulator-tf-card-image) for
an emulator card image.

## Source-build prerequisites

Use the existing [Zeal development environment (ZDE)](https://github.com/zoul0813/zeal-dev-environment)
and its SDCC toolchain, with `zde` available in your shell and its container
runtime running. The host also needs Python 3 and Make, plus an existing
[Zeal Native Emulator](https://github.com/Zeal8bit/Zeal-NativeEmulator)
with debugger/HostFS support and its platform runtime libraries. The build resolves
`zeal-native` from your shell's `PATH`.

The Unit automation expects this development layout by default:

```text
<zeal-root>/
  zeal-pascal/                           # this checkout
  zeal-dev-environment/home/
    Zeal-8-bit-OS/build/os_with_romdisk.img
    Zeal-8-bit-OS/build/os.map
    tf.base.img.gz
    zeal-coreutils/bin/
```

`<zeal-root>` is the nearest ancestor containing `zeal-dev-environment`, so
nested worktrees reuse the existing development assets. It falls back to this
checkout's parent if none is found. Set `ZP_ZEAL_ROOT` to your existing development
root when different; this controls ROM and media paths, independently of the
emulator on `PATH` and ZDE's `ZDE_PATH`. Reuse the installed emulator and ZDE
assets. To build the emulation-enabled OS ROM and bundled shell through ZDE:

```sh
zde --no-tty kernel zeal8bit/emu
```

Run the build commands below from the `zeal-pascal` checkout on the host.
The native target build runs inside ZDE; the emulator automation runs on the
host. For the complete hosted setup, emulator dependencies and downloadable
build artifacts, see [GitHub builds and releases](releasing.md) and the
[build workflow](../.github/workflows/build.yml).

## Fresh compiler and Pascal units

For a full source build, including freshly compiled stock Pascal Units, run the
host Makefile:

```sh
make
# Override the native toolchain worker count (default: 6):
make JOBS=4
```

The Makefile configures through ZDE if the build cache is missing, then runs `zde --no-tty cmake build --target zpascal_bin sysfile_zpu zos_zpu --parallel 6`
(with `JOBS` controlling the last value). It then compiles CRT and ZVB concurrently
in two isolated emulator processes using that completed production compiler.
It bypasses the CMake targets that stage tracked Pascal prebuilts. Successful
outputs are `build/CRT.ZPU` and `build/ZVB.ZPU`, with matching Pascal sources
beside them. A failed compilation returns nonzero and preserves the previous
output; never package a build after `make` fails. This command builds the release
files and copies the distributable bundle to `bin/`. It does not run the behavior
acceptance suite or create a distribution archive.

`make native` builds only the compiler, SYSFILE and ZOS. Do not run another
build or the acceptance orchestrator while this workflow is active: compiler
inputs must remain unchanged until both emulator processes exit.

To rebuild and test the general examples after `make` finishes:

```sh
zde --no-tty cmake build --target zexampletest_bin -j6
python3 tests/acceptance/test-examples.py --package
```

The harness uses the freshly compiled `build/CRT.ZPU`; it does not stage the
tracked stock prebuilt. ZealCalc has a separate build and acceptance command:
`python3 tests/acceptance/test-zealcalc.py --package`.

To compile another Pascal unit with the same compiler:

```sh
python3 tools/compile-unit.py path/to/Example.pas --output build/EXAMPLE.ZPU
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
change offsets. `--emulator PATH` selects another installed emulator and
`--timeout SECONDS` overrides the 1800-second emulator limit.

The script checks successful emulator completion and the fresh ZPU's header,
name, size and format checksum before replacing output atomically. It does not
update tracked prebuilts/manifests or claim behavior acceptance. The explicit
stock-artifact maintenance workflows below remain available for `zde cmake`
builds that reuse tracked prebuilts. A subsequent plain `zde cmake` runs those
staging targets again: it replaces freshly compiled CRT/ZVB with the tracked
prebuilts when their validation passes, or fails if they are stale. Package after
a successful `make` without running that default CMake build in between.

## Distributable hardware files

A successful `make` (or `make dist`) finishes by copying these files to `bin/`:

- `zpascal.bin`: native IDE/compiler launcher.
- `lib/zpascal.zpl`: matching compiler banks.
- `lib/SYSFILE.ZPU`, `lib/CRT.ZPU`, `lib/ZVB.ZPU`, `lib/ZOS.ZPU`: stock statically linked Units.
- `lib/CRT.PAS`, `lib/ZVB.PAS`, `lib/ZOS.PAS`: matching Pascal sources/interfaces for rebuilding and error lookup.
- `lib/ZVB.LIC`: the ZVB licence with its attribution notice included.

The six binary files are required for the IDE/compiler installation; the Pascal
sources support development and source lookup. Retain `ZVB.LIC`
when distributing ZVB. Source navigation currently requires matching text at the
source path recorded inside the ZPU. The IDE does not yet search beside the
installed ZPU, so copying the Pascal sources to a different TF-card path alone
does not enable navigation. The distributed ZVB.PAS is the exact flattened,
compacted file compiled by the build, rather than the repository's include tree.

`bin/` is generated and ignored by Git. After copying succeeds, `make` removes
the known library files left at the top of `bin/` by the previous flat layout.
Other existing files are preserved; inspect the bundle before creating a
download archive, and never package after a failed build/copy step.

## Emulator TF-card image

Install the release or locally built `bin/` files on the emulator's TF image
using the [same directory layout](#install-the-distribution) as a physical card.
For ZDE image staging and creation, see the
[ZDE image reference](https://github.com/zoul0813/zeal-dev-environment/blob/main/docs/image.md).
The release ZIP is a manual installation bundle, not a ZDE staging package.
The separate `zde image tf add bin` command copies only top-level files and
does not install this directory tree. Community image staging is a separate
workflow.

The separate [`tools/package-tf.py` workflow](#project-tf-card-image) below reads
`build/` and validates the tracked ZVB prebuilt; it does not consume the release
or locally generated `bin/` bundle. It installs the launcher in `T:/bin/` and
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
2. Build the current compiler and native units once with `make native`.
3. Compile the new unit, retaining the exact prepared source for error lookup:

   ```sh
   python3 tools/compile-unit.py units/example/EXAMPLE.PAS \
       --output build/EXAMPLE.ZPU --source-output build/EXAMPLE.PAS
   ```

   Relative includes are flattened. `{$Idefs}` uses `defs.PAS` by default;
   `{$I defs.inc}` uses the given extension. Add `--compact` only when needed
   to reduce source memory. Compile imported units first; additional compiled
   dependency directories can be supplied with repeated `--unit-path DIR`.
   A publication failure normally restores prior outputs; if rollback also
   fails, the command reports any retained original recovery file. Resolve the
   failure before packaging.
4. Run the unit's relevant behavior tests and compile a program that uses it.
5. To include it in future distribution builds, add `_example` to
   `PASCAL_TARGETS`, add `EXAMPLE.ZPU EXAMPLE.PAS` to `DIST_LIB_FILES`, and add this
   recipe to the Makefile:

```make
_example:
	$(PYTHON) tools/compile-unit.py units/example/EXAMPLE.PAS --output build/EXAMPLE.ZPU --source-output build/EXAMPLE.PAS
```

If the unit imports CRT, declare `_example: _crt` instead; list every unit
prerequisite the same way. Independent unit targets run concurrently, up
to `JOBS`; dependency edges ensure imports use the newly compiled outputs.
Run `make` to rebuild and populate `bin/`. These fresh-build steps do not
update the legacy tracked prebuilt manifests.

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

For compiler development using the tracked CRT/ZVB prebuilts, the default
CMake build is available through ZDE:

```sh
zde cmake
```

This builds the compiler, SYSFILE and ZOS, and validates/stages the tracked
CRT/ZVB artifacts. It does **not** recompile those Pascal Units in the emulator;
stale tracked artifacts can cause validation to fail. Use the full
[`make` workflow](#fresh-compiler-and-pascal-units) to rebuild them with the current
compiler instead. Do not run the default CMake build between `make` and packaging,
as it stages tracked prebuilts over the fresh CRT/ZVB outputs.

The default build has five product entry points:

| Target | Output |
| --- | --- |
| `zpascal_bin` | `zpascal.bin` and its matching `zpascal.zpl` |
| `crt_zpu` | `CRT.ZPU` and `CRT.PAS` |
| `sysfile_zpu` | `SYSFILE.ZPU` |
| `zvb_zpu` | Validated stock `ZVB.ZPU` |
| `zos_zpu` | Native syscall `ZOS.ZPU` and its `ZOS.PAS` interface |

The root `CMakeLists.txt` sets up the project and build layout, includes
four production modules, and conditionally includes `cmake/Tests.cmake`:

- `Sources.cmake`: source lists, bank placement and header dependencies.
- `Runtime.cmake`: numeric runtime templates, execution handoff and asset generation.
- `Compiler.cmake`: compiler object groups and BIN/ZPL linking.
- `Units.cmake`: CRT, SYSFILE and ZVB generation or staging.

`Units.cmake` includes `SysFile.cmake` for the file backend's build steps.
The assembler and standard linker build `units/sysfile/` directly into a ZPU
byte stream, including its exports, fragments and relocation tables. SYSFILE
has no production C routines, SDK archive extraction, or custom object linker.
A small host C utility, `build/assets/sysfile-seal`, adds the required content
checksum and runtime fingerprint; it does not generate, place or relocate code.
The assembly preserves the existing private calling convention internally and
uses direct Zeal OS syscall adapters. Several hot helpers are hand-tuned; moving
code to ASM does not imply every retained stack frame has been optimized away.
Other compiler/runtime asset builders and stock CRT/ZVB validation use Python.

Internal targets are excluded from `ALL` and run when
required by a product dependency. CMake still reports those compile/link steps
as “Built target …”; those messages do not identify extra shipped programs.

Conversions and packaging use file-based rules in `Outputs.cmake`. An unchanged
`zde cmake --parallel 4` skips compilation, linking and asset/header generation;
changing a generator input or deleting an output schedules the required work.
Stock CRT/ZVB integrity checks still run on each default build and preserve
unchanged output timestamps. “Built target …” alone does not mean work ran.
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

## Stock ZVB Unit

`units/zvb/` contains the readable Pascal source, inline Z80 implementation,
licences and `prebuilt/` native-authored artifact. Normal builds validate that
artifact against its source and compiler/runtime bindings, then stage `ZVB.ZPU`.
Validation runs on every default build, including incremental builds.
They do not run an emulator or compile test executables. CRT uses the same
validated stock-artifact approach; SYSFILE is assembled and checksum-sealed from maintained ASM sources.

The stock manifest binds the source components and generator, exact compiler
BIN/ZPL and private runtime profile. A changed binding fails the build instead
of silently shipping a stale Unit. `zpascal_bin` remains independently buildable
so a developer can build the compiler needed for fresh native authoring.

To refresh ZVB after an implementation or compiler change:

```sh
zde --no-tty exec cmake -S . -B build -DZP_BUILD_TESTS=ON
zde --no-tty cmake build --target zvb_authoring -j4
python3 tests/acceptance/test-zvb.py --skip-hardware --output-library build/ZVB.ZPU
python3 tools/package-zvb.py build --record-authoring analysis/native-slice/RUN/zvb-report.json
```

Use the exact report path printed by the native authoring run. Select an
installed emulator with `--emulator /path/to/zeal-native` if needed. Recording
rejects failed runs, prebuilt-client reports, changed compiler/runtime inputs,
a different Unit payload, or a different flattened Pascal source. Commit the
updated Unit and manifest together, validate clients, and switch tests off again
for a normal release build.

## Stock CRT Unit

`units/crt/CRT.PAS` contains ordinary Pascal screen logic, named register constants
and small annotated native kernels for bounded VRAM access and OS/runtime
bridges. The default `crt_zpu` target validates the precompiled Unit against that
source, its authoring tool, the compiler BIN/ZPL and runtime profile, then stages
`CRT.ZPU` and the complete `CRT.PAS`. It never runs the historical C emitter.

To re-author after a CRT source or compiler change:

```sh
zde --no-tty exec cmake -S . -B build -DZP_BUILD_TESTS=ON
zde --no-tty cmake build --target zpascal_bin -j4
python3 tools/author-crt.py
python3 tools/package-crt.py build --record-authoring analysis/native-slice/RUN/crt-authoring-report.json
zde --no-tty cmake build --target zscreentest_bin -j4
python3 tests/acceptance/test-screen.py
python3 tests/acceptance/test-crt-readkey.py
```

Use the exact authoring report path printed by the run. Commit the Unit and
manifest together only after framebuffer, error-policy, keyboard and standalone
checks pass. `tools/measure-crt.py` compares the Pascal implementation with the
historical emitter using the same current compiler/runtime. It reports payload
and COM sizes and standalone instruction-cycle measurements with interrupts
masked during the timed calls. Restore `ZP_BUILD_TESTS=OFF` for release builds.
