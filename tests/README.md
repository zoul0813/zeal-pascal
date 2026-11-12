# Testing Zeal Pascal

Enable native harnesses through ZDE as described in the
[build guide](../docs/building.md#opt-in-test-targets). The executable acceptance
plan is [tests/acceptance/test-plan.json](acceptance/test-plan.json); run it through
`python3 tools/run-tests.py`. Runner selectors and chunking are described below.
[Physical acceptance](hardware/README.md) is a separate procedure.

Acceptance drivers and their test-only helpers live in `tests/acceptance/`.
`tests/host/`, `tests/native/`, and `tests/fixtures/` hold harnesses and fixtures;
top-level `test_*.py` files check host utilities. Build, packaging, generation,
and measurement utilities remain in `tools/`, along with the acceptance runner
entry point.

## Syscall checks and measurements

Build with `zde cmake build --target zpascal_bin zos_zpu zsyscalltest_bin -j4`
after enabling ZP_BUILD_TESTS. Run `python3 tests/acceptance/test-syscalls.py` for native
acceptance, or add `--host-only` for sanitized transport checks. Runtime profile,
production unit acceptance and loader regressions cover the private runtime
and unit-format integration.

`python3 tools/measure-syscalls.py PATH/TO/ZBENCH.COM` measures the saved benchmark
produced by acceptance. Results are written under `build/`. Timings are emulator
CPU cycles, not physical storage throughput. Stack sentinels measure observed
writes, not an upper bound on reserved stack.

## Fixtures and local reference research

`fixtures/snake/` contains the frozen SDK trace and pixel oracles consumed by
`tests/acceptance/test-snake-parity.py`. Their source binary hashes are part of the fixture
contents. The capture scripts regenerate these fixtures only when deliberately
refreshing the reference; ordinary runs write results under `analysis/`.

CP/M language conformance additionally requires the preserved local corpus in
`tests/cpm/`, captured original sessions and research records under
`research/docs/`, plus lexical oracles under `analysis/language-conformance/`.
Those inputs are intentionally outside Git. A normal checkout alone cannot
reproduce that reference comparison. Missing or changed oracles fail validation.
The optional `tests/acceptance/test-native-host.py --research-spec` comparison uses the
local archived specifications. It is distinct from ordinary host behavior
acceptance and currently rejects the archived syscall readiness report because it is not
the historical normative language contract. A matching preserved normative pair
is required for that optional comparison to pass.

Generated reports belong in `analysis/`, `build/`, or the ignored `research/`
archive. User-facing `docs/` contains guides and API references. Historical
records retain their original hashes and paths as capture-time provenance;
`research/docs-archive.json` maps the moved files to their local archive paths.


## Acceptance runner

```sh
python3 tools/run-tests.py --list
python3 tools/run-tests.py --only editor-native:find
python3 tools/run-tests.py --only 'language-conformance:*' --jobs 6
python3 tools/run-tests.py --group publication --jobs 4
```

With no selector the runner uses the full scheduled plan. Repeat `--only` and
`--group` to take their union; quote wildcard selectors. Runner names select all
of their planned modes/chunks; exact IDs from `--list` select one. Unknown
selectors fail. Groups and their membership are defined in test-plan.json.
`--list` shows selection without running. Required host checks are included once,
and prerequisite producers run before their dependent observers.

`--jobs` is a shared worker budget (default 4); use 1 for serial diagnosis.
Independent isolated chunks can overlap, while commands sharing output paths
are serialized. Do not rebuild or run a second orchestrator in the checkout
while acceptance is active. Native selections configure tests ON and build
prerequisites before freezing inputs; restore tests OFF for release builds.

By default the first failure cancels outstanding work. `--keep-going` collects
independent failures and explicitly skips failed dependencies; it does not waive
failures. Logs, input hashes and results are retained under analysis/test-runs/.
The runner reports active work periodically and stops owned children on
interruption. It does not cache passes or resume an old result. Focused passes
are marked passed-focused and do not imply full acceptance.

`--monolithic` selects the unchunked plan for every runner that has chunks.
`--include-redundant` repeats the optional diagnostic subsets. Native immutable
media fixtures can be prepared independently with:

```sh
python3 tests/acceptance/test-native.py --prepare-media
```

The default plan expands component, editor and language selections into the
chunks declared in the plan. Use `--list` for current counts rather than relying
on a historical total. Individual drivers retain their focused modes, such as
`tests/acceptance/test-editor-native.py --case find` and
`tests/acceptance/test-language-conformance.py --shard 1/4`.

## Canonical compiler corpus

`zptest_bin` builds the 18-family native language corpus against the same
compiler, runtime, unit and image objects as `zpascal`. The `zptest` name remains
for existing callers; its former `zp_core` compiler configuration is no longer
used by this target. Other specialized harnesses still use `zp_core`.
The duplicate `zoptionalcorpus` and `zunitcorpus` targets/drivers are retired.

Run `python3 tests/acceptance/test-native.py --core-only` for all families,
or use `--case control` (and `--list-cases`) for independent diagnosis.
The scheduled runner expands all 18 families and eight platform/IDE workflows.
The test supervisor occupies spare existing banks and is not linked into the
shipped compiler. Pointer-taking helpers capture caller-bank strings before
switching banks; machine-state observations remain resident.

Production supports unsigned decimal constants, so the basic family accepts
32768 and -(32768), closes those builds, and rejects 4294967296. The historical
assertion ledger remains recorded alongside four added production checks.

ABI fixtures install their exact independent machine bytes at `$B800` instead
of assuming a fixed global-data address. Host checks pin those bytes in each
first typed constant and verify the program ends below the test arena. Heap
pressure cases preallocate 16 KiB before exhausting the larger production heap;
availability comparisons preserve the documented unsigned bit patterns.

## Example applications

The entire examples
tree, including its prebuilt `.bin` files and binary provenance, is intended
for source control. Generated `.COM`/`.ZMP`/backup files and the working
`parts.dat` database are ignored.

Enable the opt-in test targets as described above, then run these commands
from the repository root.

From the repository, build and run the native acceptance check:

```sh
zde --no-tty cmake --target zexampletest_bin
python3 tests/acceptance/test-examples.py
```

The check compiles these exact sources on the pinned, unmodified Zeal emulator,
runs their standalone `.bin` programs, and checks puzzle completion, database
persistence, zero-rate and ordinary loan schedules, word counts, Life evolution,
heap reclamation, recursive expressions and error recovery, every queen count,
and the Measure Lab handoff/return, EOF finalization and missing-program recovery. Its temporary executables, media, output captures and
hash-bound report stay under `analysis/native-slice/`. It does not install the
examples on a user's card or establish physical-hardware/full TP3 conformance.

To refresh the shipped executables after changing sources, run the same native
check with `--package`. Only after the behavior checks pass does it copy the
native-tested images into these folders and update `binaries.json`. A normal
run verifies that the shipped binaries and source hashes match fresh compiler
output. The `.bin` bytes are unchanged from the compiler's published native
COM format; the test launches the `.bin` filenames themselves.

The shipped executables record their compiler base revision, compiler-source
digest and runtime digest in `binaries.json`.
After changing the compiler or runtime, rebuild them all with `--package`
rather than mixing builds. The manifest records the compiler base revision and
a digest of its source contents alongside the source/binary hashes.

ZealCalc uses a separate focused driver and `examples/zealcalc/binary.json`
manifest. After `make` builds the native compiler through ZDE and authors its stock
Pascal Units, run `python3 tests/acceptance/test-zealcalc.py` (or `--package` to refresh
its binaries). It compiles the three real Pascal programs, checks formulas,
bounded storage and reopening through the backend pipeline, and replays application
keyboard entry, cancel, Blank, navigation and corrupt-file rejection.
