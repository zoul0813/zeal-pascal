# Building Zeal Pascal

Use the existing Zeal development environment (ZDE) and its SDCC toolchain.
A fresh checkout builds only the product and the runtime assets it needs:

```sh
zde cmake
```

Ship these files from `build/`:

- `zpascal.bin`: native IDE/compiler launcher.
- `zpascal.zpl`: matching compiler banks.
- `SYSFILE.ZPU`, `CRT.ZPU`, `ZVB.ZPU`, `ZOS.ZPU`: stock statically linked Units.
- `CRT.PAS`: the complete Pascal CRT source; `ZOS.PAS`: the generated ZOS interface.

Keep the four ZPU files beside the loaded ZPL. Include the ZVB licence and
notice from `units/zvb/` when distributing its Unit. Other runtime ZPLs, IHX,
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

## Build targets

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
`units/zvb/`. Historical review and measurement records retain paths from the
revision they describe.

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
native prerequisites declared in `tools/test-plan.json`. Every planned driver
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
The [layout-only milestone evidence](build-layout-evidence.json) records that
cleanup's file list and unchanged binaries. Current SYSFILE products and savings
are recorded in [the ASM migration evidence](sysfile-asm-evidence.json).

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
python3 tools/test-zvb.py --skip-hardware --output-library build/ZVB.ZPU
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
python3 tools/test-screen.py
python3 tools/test-crt-readkey.py
```

Use the exact authoring report path printed by the run. Commit the Unit and
manifest together only after framebuffer, error-policy, keyboard and standalone
checks pass. `tools/measure-crt.py` compares the Pascal implementation with the
historical emitter using the same current compiler/runtime. It reports payload
and COM sizes and standalone instruction-cycle measurements with interrupts
masked during the timed calls. Restore `ZP_BUILD_TESTS=OFF` for release builds.
