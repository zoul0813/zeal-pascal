# Building Zeal Pascal

Use the existing Zeal development environment (ZDE) and its SDCC toolchain.
A fresh checkout builds only the product and the runtime assets it needs:

```sh
zde cmake
```

Ship these files from `build/`:

- `zpascal.bin`: native IDE/compiler launcher.
- `zpascal.zpl`: matching compiler banks.
- `SYSFILE.ZPU`, `CRT.ZPU`, `ZVB.ZPU`: stock statically linked Units.
- `CRT.PAS`: the generated CRT interface, when distributing source interfaces.

Keep the three ZPU files beside the loaded ZPL. Include the ZVB licence and
notice from `units/zvb/` when distributing its Unit. Other runtime ZPLs, IHX,
REL, map files, headers and JSON files in `build/` are intermediate build assets;
they are not additional files the user needs to install.

## Build targets

The default build has four product entry points:

| Target | Output |
| --- | --- |
| `zpascal_bin` | `zpascal.bin` and its matching `zpascal.zpl` |
| `crt_zpu` | `CRT.ZPU` and `CRT.PAS` |
| `sysfile_zpu` | `SYSFILE.ZPU` |
| `zvb_zpu` | Validated stock `ZVB.ZPU` |

The root `CMakeLists.txt` only sets up the project, includes
four production modules, and conditionally includes `cmake/Tests.cmake`:

- `Sources.cmake`: source lists, bank placement and header dependencies.
- `Runtime.cmake`: numeric runtime templates, execution handoff and asset generation.
- `Compiler.cmake`: compiler object groups and BIN/ZPL linking.
- `Units.cmake`: CRT, SYSFILE and ZVB generation or staging.

Internal targets are excluded from `ALL` and run when
required by a product dependency. CMake still reports those compile/link steps
as “Built target …”; those messages do not identify extra shipped programs.

The old full `file_runtime` module is not part of the production graph or
runtime fingerprint. Its builders live in `Tests.cmake` for explicit legacy
probes. Production file support is generated as selectively linked SYSFILE.ZPU
functions; this still compiles the file operations it needs from `file_runtime.c`.
`runtime_profile` fingerprints the numeric runtime, handoff assets and compiled
SYSFILE payload. SYSFILE compiles once into a payload with a neutral fingerprint;
its final ZPU header is stamped after that payload has been fingerprinted.

The BIN and ZPL are produced together so they remain a matching compiler pair.

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

Turning the option off removes test targets from the build graph; it does not
delete previously built test files. Use a fresh build directory when checking a
clean release build. Reconfiguring is through `zde exec`; actual target builds
use `zde cmake`.

## Stock ZVB Unit

`units/zvb/` contains the readable Pascal source, inline Z80 implementation,
licences and `prebuilt/` native-authored artifact. Normal builds validate that
artifact against its source and compiler/runtime bindings, then stage `ZVB.ZPU`.
Validation runs on every default build, including incremental builds.
They do not run an emulator or compile test executables. CRT and SYSFILE retain
their existing build-time generators.

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
