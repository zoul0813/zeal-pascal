# Review: source-layout-v1

## Opus turn 20261108T234334-f644ec48

Mode: code; read-only

# Source-layout stage 1: initial review

**Result:** I found no blocker or high-severity defect in the supplied stage 1 diff, and this is not a readiness claim. The bank-relevant `COMPILE_OPTIONS` assignments and object-list order in the supplied CMake diff map one-to-one. The include rewrites I checked also match `plan.json`.

## Established defects (stage 1)

**1. Low: `tools/test-examples.py`, `compiler_digest`**
- **Trigger and impact:** The recursive inventory still hashes `path.name`. Moving a file between directories without changing its sort position leaves `compiler_sources_sha256` unchanged, so the manifest no longer binds source location.
- **Evidence:** `compiler_digest.update(path.name.encode() + ...)` now runs over `rglob('*')`, and the tree already has duplicates such as `codegen/emit.c` and `link/emit.c`.
- **Fix:** Hash `path.relative_to(ROOT/'src').as_posix()`.

**2. Low: `src/compiler/codegen/builtins/crt.c`, screen-engine include**
- **Trigger and impact:** `"../libraries/screen-engine.inc"` is no longer correct relative to the file. It resolves only through `-Isrc` (`src/../libraries`), so a non-Units target without `-Isrc` would fail.
- **Evidence:** The include is unchanged while the file moved three directories deeper.
- **Fix:** Use `../../../../libraries/screen-engine.inc`, or add an explicit project-root include path.

**3. Low: `tools/check-startup-banks.py`, object inventory**
- **Trigger and impact:** Moving to `link.txt` correctly avoids stale objects, but it also adds `unit_fragment_core`, which the old five-target list did not scan. Its objects are absent from `allowed` and `providers`, so any cold-bank reference or definition now fails.
- **Fix:** Keep the broader scope if intended, record it as a deliberate checker change, and run the checker on the final link.

## Verification gaps (stage 1)

**4. Medium: `cmake/Tests.cmake` and renamed host commands**
- **Trigger and impact:** Identity from the default build will not exercise test-only targets such as `zcompacttest`, `zfragmenttest`, `unit_core`, and `optional_real_core`.
- **New `-Isrc` dependency:** Rooted includes such as `compiler/jobs/job.h` in `src/banks.c` previously resolved as siblings. Every test target compiling `banks.c`, `image.c`, or `platform.c` now needs `-Isrc`, and the full `Tests.cmake` was not supplied.
- **Reported evidence gap:** No results were supplied for `test-native-host`, `test-staging-limits`, `test-linked-addresses`, `test-compiler-messages`, `test-fragments`, or either bank checker.
- **Verification:** Build the affected ZDE test targets and run these commands before relying on them as migration evidence.

## Stage 2 plan risks

**5. Medium: targets without `-Isrc`**
- **Trigger and impact:** `ide_paths`, `real_compiler`, `real_runtime`, `key_runtime`, and `compact_integer` declare no `src` include directory in the supplied CMake. Rewriting their includes to forms such as `common/path.h` or `common/zp.h` would fail unless an include path is set elsewhere.
- **Fix:** Add the include directory to each target, or keep valid sibling includes. Check the host commands for chain, transfer, and file runtime as well.

**6. Medium: assembler include paths, needs ASM contents**
- **Trigger and impact:** `transfer_call` depends on `build/transfer_call_data.inc`, but its target declares no include directory. If `src/transfer_call.asm` uses a source-relative path, moving it to `src/runtime/process/` breaks that path.
- **Fix:** Audit every `.include` and `.incbin` across the 69 stage 2 files, including sibling references split by the plan.

**7. Medium: stale paths can fail silently**
- **Trigger and impact:** `set_property(SOURCE ...)` on an old path can silently stop applying dependencies. Identity may catch lost bank options, but not lost `OBJECT_DEPENDS`, which only affects incremental rebuilds. Tool lists filtered by `if p.is_file()` also drop stale inputs from evidence.
- **Known flat-path consumers:**
  - `package-file-unit.py`: `ROOT/'src'/source` and its CMake dependencies
  - `check-unit-banks.py`: `src/library.h` and `src/library_phase.c`
  - `test-linked-addresses.py`, `test-runtime-layout.py`, and `test-fragments.py`
  - `generate-wide` dependencies and `Units.cmake`
- **Fix:** Add a post-move check that every `src/...` token in `cmake/`, `tools/`, and `tests/` exists, and that no baseline path from `plan.json` remains.

**8. Low: ownership direction in planned groups**
- `src/platform/banks/phase.c` would include `compiler/internal.h`. The checker also requires the `code_phase_unit_finish_begin` callers to be compiler and Unit finish roots, so this is compile-phase policy rather than a pure platform service.
- `compiler/link/link.h` includes `units/linker/fragments.h`, making the core linker depend on the Units tree.
- **Suggestion:** Move the phase code under `compiler/` or `app/`, and either give fragments a neutral link location or document this edge as intentional.

## Questions before the checkpoint

1. **Are all new paths trackable?** `.gitignore` contains allowlist-style entries such as `!/tests/test_file_unit.py`. Please run `git check-ignore -v` over all new source paths, then confirm the staged rename count.
2. **Does anything depend on SDCC module basenames?** Stage 1 introduces duplicate basenames in the link, such as `image`, `emit`, and `fragments`. Binary identity covers the output, but not tools parsing map or module names. `package-native.py` was not supplied.
3. **Do unchanged consumers still contain old names?** Please provide a repository-wide grep for the 156 baseline paths, covering docs, evidence JSON, `tests/test_file_unit.py`, `tools/screen-library.c`, and flat `src` globs.
4. **Why does `worker.c` belong in `storage/`?** The supplied context shows platform and banks dependencies, but not enough to confirm its responsibility.

## Baseline note, outside this review

`tools/test-staging-limits.py` looks for the SDK at `Zeal-8-bit-OS/sdk/...`, while sibling tools use `zeal-dev-environment/home/Zeal-8-bit-OS/...`. This may stop that test from running in the expected environment.

## Limitations

- I executed nothing. Test results and the normalization proof are taken as reported.
- Final artifact identity is still pending.
- I did not have `CMakeLists.txt`, the full `Tests.cmake`, source and ASM bodies beyond the diff, or `package-native.py`.
- The stage 2 assessment covers only the plan and visible path assumptions.

### Codex reconciliation

Stage 1 reconciliation (final stage 2 verification pending):

1. Accepted: example compiler manifest now hashes source-relative POSIX paths.
2. Accepted: CRT engine include is explicitly relative to its new directory.
3. Accepted: audit intentionally checks every linked object, including fragment-core objects. Actual map/REL definitions require chain.c and units/io/loader.c as providers and units/symbols/names.c as an importer. Explicit sets updated; both bank audits pass. Release bytes unchanged, so no residency change.
4. Accepted verification gap: test-enabled configure, selected target builds and host commands will be verified after stage 2. Legacy experimental targets that already overflow are not claimed passing.
5. Accepted: stage 2 adds project-header include roots to the five identified targets.
6. Accepted: only source ASM include is generated transfer_call_data.inc, retained verbatim and assembled from build cwd; stage 2 build will verify it.
7. Accepted: stage 2 includes CMake/source path existence and stale path scans, plus incremental header-dependency comparison.
8. Adapted: move phase policy to compiler/phase.c and parent worker services to platform/worker.*. Fragments stay with Unit implementation because they encode the ZPU fragment model consumed by final linking; document this intentional compiler-to-Unit dependency rather than introduce another linker abstraction.

Questions: all 87 new paths are staged and trackable; rename-aware diff recognizes the source moves. Module basenames are preserved in generated outputs except renamed files, and probes now identify objects by full linked source-relative paths. Full artifact identity passed for BIN, ZPL, CRT, SYSFILE, ZVB and profile against 5e1ea56. Historical docs/evidence retain their original paths; living references and executable consumers are migrated. Worker manages parent-owned child page/descriptor services, hence platform ownership.

Baseline staging SDK lookup will be checked with the existing local SDK path; no install is needed. First review is not a final readiness approval.
