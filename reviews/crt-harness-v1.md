# Review: crt-harness-v1

## Opus turn 20261108T124835-35550386

Mode: code; read-only

# Review: CRT harness repair (crt-harness-v1)

## Summary

I found no blocker or high-severity defects in the supplied change. The bank/pointer lifetime reasoning holds against the existing precedents in `screen-main.c`. The new `checked_error` lookup gives the same position as `strstr` for every current call site, and CMake picks up `harness-cases.c` in every target that links `harness.c`. I have four low-severity findings and two questions.

I have no evidence that any test ran. The 502-assertion count, the 16373-byte resident figure and the map placement are all reported, not shown.

## Established findings

**1. Low — `CMakeLists.txt`, missing `OBJECT_DEPENDS` for `harness-cases.c`**
- **Trigger:** an incremental ZDE build after `src/zp.h` or `src/library.h` changes.
- **Impact:** a stale `harness-cases.c` object may remain. It touches `Source s` (`s.length`) and calls `source_import`, `source_replace` and `load_source`, so a stale object could give a wrong struct layout or ABI.
- **Evidence:**
  - `tests/native/harness.c` has `APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_SOURCE_DIR}/src/zp.h ${CMAKE_SOURCE_DIR}/src/library.h`.
  - The diff adds only the `tests/native/harness.h` dependency for `harness-cases.c`.
  - The file's own comment says "This SDK disables compiler depfiles."
- **Fix:** add `tests/native/harness-cases.c` to the existing `set_property(SOURCE tests/native/harness.c APPEND PROPERTY OBJECT_DEPENDS …zp.h …library.h)` line.

**2. Low — `tools/test-screen.py`, placement checks use bare `assert`**
- **Trigger:** running the script under `python -O` or `PYTHONOPTIMIZE`.
- **Impact:** the bank placement checks (`_app_main`, the four helpers, `_check`/`_seed` in bank 0) are silently skipped. The report would also not show them.
- **Evidence:** they are plain `assert` statements, run before the staleness check. They are not entered in `checks{}`.
- **Fix:** raise `RuntimeError` explicitly, or add a `checks['harness helpers share BANK1; check/seed resident']` entry so the report captures the result.

**3. Low — `tools/test-screen.py`, placement is checked before the staleness guard**
- **Trigger:** the `.map` is newer or older than `.bin`/`.zpl` after a partial rebuild.
- **Impact:** the symbol assertions may pass against a map that does not match the packaged binary. This is minor, because `.map` and `.ihx` are produced together by the link.
- **Fix:** move the placement assertions after the `Stale Screen test artifacts` check.

**4. Low — `docs/hardware-testing.md`, ambiguous wording "some qualified COMs predate…"**
- **Impact:** "qualified" can be read as hardware-qualified. The assignment requires that the document make no physical-qualification claims.
- **Fix:** write "some emulator-verified COMs predate…", or name the verification source.

## Verification of reviewed areas (no defect found)

**Bank and pointer lifetime**
- `load()`'s `static const prefix[]` now lives in BANK1 and is passed to `source_import`. `app_main` already passes BANK1 literals directly to `source_import` (`"begin clrscr end."`) and to `strcpy`/`compile_unit` paths, so the lifetime is unchanged.
- The `text` and `path` arguments were already BANK1 pointers before the split.
- `execute_loaded` is `SOURCE_BLOCK_CALL` in BANK9 and is now called from BANK1. BANK1 already calls the `SOURCE_BLOCK_CALL` function `cached_code_pages()` and the banked `run_build`/`run_saved` functions, so return-to-BANK1 is already exercised.
- `check` and `seed` remain resident for BANK9 callers.
- `memcmp` and `strlen` are SDCC library routines in the resident area, so a BANK1-to-resident call is fine.

**CMake target coverage**
- The loop's `IN_LIST` test covers every looped target that includes `harness.c`, including `zcompacttest`.
- `zunittest`, `zunitcorpus`, `zoptionalcorpus` and `zscreentest` add the file explicitly.
- No other `add_executable` links `harness.c`.
- `ZP_SCREEN_LIBRARY` is defined only on `zscreentest`, so the BANK1 pragmas are scoped to that target as stated.

**Error-location oracle**
- For a found needle, the bounded loop returns the same first-match index as `strstr`. This includes an empty needle, which gives 0 in both.
- The `needle > length` guard prevents unsigned underflow.
- I checked each needle at the call sites (`clrscr`, `if`, `delay`, `gotoxy`×3, `textcolor`, `textbackground`×2, `writeln`). Each one's first occurrence is the intended token, case-sensitively. For example, no earlier `if` appears in the `WhereX` case.
- The CRT prefix offset is a compile-time `sizeof`, so it adds no storage.

**Assertion coverage**
- When the needle is found, the sequence of `check` calls is identical to before.
- When it is missing, the old code passed a pointer-difference of NULL minus `text` as the position, which is undefined behaviour. The new code records one explicit failure. This change is an improvement.

## Questions / limitations

- **Q1.** Does `tools/test-native.py` derive anything from `EMULATOR` at import time, such as `ROM`, environment variables or default arguments? `--emulator` reassigns `n.EMULATOR` only after the module has been executed. If `n.ROM` or the `execute()` defaults depend on the original path, the run could mix the two emulators while the report records the new one. Please supply the relevant part of `test-native.py`.
- **Q2.** Please confirm that `#pragma codeseg BANK1` produces the same `_BANK1` area name as `--codeseg BANK1`. The new map assertion would catch a mismatch, but only once the run evidence is supplied.
- **Limitations:**
  - Not supplied: `test-native.py`, the `SOURCE_BLOCK_CALL` macro definition, the build map or `banks.json`, and native test output.
  - With 11 bytes of resident headroom, any later resident addition will fail packaging. The packager enforces this correctly, but it is an operational risk.
  - The hardware package and manifest are not in scope yet, so the per-file provenance claims cannot be checked.

### Codex reconciliation

1. Accepted. harness-cases.c now has the same explicit zp.h/library.h dependencies as harness.c, plus harness.h. Final ZDE build passed after this change.
2. Accepted. Placement checks now use an explicit RuntimeError and are included in the report's checks dictionary, remaining active with Python optimization.
3. Accepted. Placement checks follow the existing artifact staleness guard.
4. Accepted. Hardware checklist says emulator-verified, not hardware-qualified.
Q1. Verified. EMULATOR is read dynamically by execute when launching the process. ROM, kernel map and TF template are independently derived from ZP_ZEAL_ROOT, not from EMULATOR. Relevant test-native.py code will be included in follow-up.
Q2. Verified actual linked map: app_main and load/execution/execution_file/checked_error in BANK1; check/seed in resident TEXT. Package metadata: resident16,373 (11bytes free), BANK1 6,032 bytes. The unchanged packager enforces all page limits. Exact metadata/map excerpts will be supplied in follow-up.

The first full CRT run exposed an additional stale test-oracle assumption: crt.pas and colors.pas temporarily mapped VRAM over CPU page1, replacing arrays/globals now validly placed below8000. Fixed those Pascal-only oracles to use bounded page0 Inline copies, preserving IFF and the original page0 mapping. Character/attribute offsets are0/1000, zero-word operands use explicit > width, and bottom-row addresses are relative to page0. No CRT implementation or compiler change. All existing check calls and output expectations remain. The native suite is rerunning on stable final sources; no pass is claimed yet. These oracle fixes will be included in the one follow-up.

Additional pre-follow-up verification: the corrected page0 oracles produced the exact expected complete CRT output (both CRT-OK:-7885 and COLOR-OK:1815 twice), all sleep arguments matched, and final owned/cache counts were15/15. That run passed501 native assertions and failed only assertion11: the unused-import case still expected old flag2. Native direct plain/unused-Crt clients independently pass61 assertions, produce identical6655-byte COMs, and both have flag4 (single-page). Replaced the hard-coded flag with direct size+flag equivalence and used named native-layout masks for the hardware case. Added native_image.h dependency. Final ZDE build passes with resident16373 and BANK1 6051. Full502-check acceptance and standalone Chain are running; Codex requires both before finalizing.

## Opus turn 20261108T131353-9f06da28

Mode: code; read-only

# Follow-up review: CRT harness repair (crt-harness-v1)

## Summary

I found **no blocker, high or medium defects**. All four earlier findings are fixed, and both questions are answered by the supplied files. The new Inline sequences are correct at the byte level. Three low-severity items and two verification points remain.

**Acceptance is still open.** The supplied run is 501/502 and is marked non-final, and standalone Chain has no result yet.

## Prior findings

| Item | Status | Evidence in current files |
|---|---|---|
| 1. `harness-cases.c` dependencies | Fixed | It now shares the `zp.h`/`library.h` `OBJECT_DEPENDS` line with `harness.c`. |
| 2. Bare `assert` placement checks | Fixed | They now raise `RuntimeError` and appear in `checks`. |
| 3. Placement check before staleness guard | Fixed | The placement check now follows the guard. |
| 4. "Qualified" wording | Fixed | The checklist now says "emulator-verified". |
| Q1. `--emulator` mixing | Resolved | `execute()` reads `EMULATOR` when launching. `ROM` and related paths come from `ZEAL`, not `EMULATOR`. `main()` reassigns it before `inputs` is hashed. |
| Q2. Pragma area name | Resolved | The map puts `_app_main` and the four helpers at `0x18050`–`0x19368`, and `_check`/`_seed` at `0x4F24`/`0x4F5C`. `s__RODATA + l__RODATA` = `0x7FF5`, which matches 16373 resident bytes and 11 free. |

## Established findings

**1. Low: `tools/test-native.py`, `inputs` list omits `harness-cases.c`**
- **Trigger:** `harness-cases.c` changes between the start and end of a `test-native.py` run, or a report is used to identify the helper source.
- **Impact:** `load`, `execution`, `execution_file` and `checked_error` moved out of a hashed file into an unhashed one. The "implementation inputs unchanged" check and `input_sha256` no longer cover them, against the AGENTS.md rule to bind findings to artifact hashes. `zptest.bin` and `.zpl` are still hashed, so the executed bytes remain bound. `test-screen.py` is unaffected because it globs `tests/native/*`.
- **Evidence:** the list names `('main.c', 'harness.c', 'harness.h', 'core-groups.h', 'core-counts.json', 'machine.asm', 'startup-fail.c')`.
- **Fix:** add `'harness-cases.c'`. `test-screen.py` hashes `test-native.py`, so editing it during the current run would fail the unchanged-inputs check. Make the edit after that run and rerun, or record it as a known gap.

**2. Low: `docs/crt-library.md`, verification claim precedes its evidence**
- **Trigger:** the document is packaged or committed before the final run passes.
- **Impact:** the text points to `crt-harness-evidence.json` for "full framebuffer verification after the test-harness repair", but that file is not generated yet and is absent from the staged manifest. The link would be dead, and the claim would be unsupported.
- **Fix:** generate the evidence only from a passing run, then rebuild the manifest so it includes the evidence file and the final `crt-library.md` hash. If the run fails, revert this sentence.

**3. Low: `tests/native/screen-main.c` assertion 11 compares size and flags, not bytes**
- **Impact:** the assignment says the assertion "compares bytes and flags", but `b.length==plain` compares only length. An unused import that changed code without changing its length would pass. The differential design is sound, and the failure index does identify this assertion: counting `check` calls from `source_new`, it is call 11.
- **Evidence:** the separate 61-assertion report shows matching `file_bytes`, `code_bytes` and `flags`. Its "identical output" result is only a boolean, and the code behind it was not supplied.
- **Fix:** describe it as size and flag equivalence, or point to a host-side comparison of the two COM files.

## Inline sequence review (no defect found)

I decoded the common wrapper used by `paint`, both `sample` procedures and `bottom`:

| Bytes | Instruction | Result |
|---|---|---|
| `ED 57`, `F5` | `LD A,I`, `PUSH AF` | Saves IFF2 in P/V. |
| `F3` | `DI` | Masks interrupts before the kernel page is unmapped. |
| `AF`, `DB F0`, `F5` | `XOR A`, `IN A,($F0)`, `PUSH AF` | Reads and saves the page 0 mapping. A=0 selects page 0, matching the high-byte convention in `physical_cell`. |
| `3E 40`, `D3 F0` | `LD A,$40`, `OUT ($F0),A` | Maps VRAM, the same physical page as the old oracle. |
| `F1`, `D3 F0` | `POP AF`, `OUT ($F0),A` | Restores the mapping. |
| `D1`, `CB 53`, `28 01`, `FB` | `POP DE`, `BIT 2,E`, `JR Z,+1`, `EI` | E holds the saved flags, so `EI` runs only if IFF2 was set. |

- **Stack:** two pushes and two pops, in matching order.
- **Operand widths:** `>$0000` and `>80` force word operands. `$1000`, `320` and `3200` exceed 255, so they are already words.
- **Offsets:** characters at `0` and attributes at `$1000` match the old `$4000`/`$5000` addresses in page 1. `bottom` adds `$1000` to a page-relative row address.
- **Mapping safety:** code, globals and the stack stay outside `0000–3FFF`, and interrupts are masked during the copy. Registers clobbered are limited to AF, BC, DE and HL.

## Questions and pending verification

- **Q1. Is `harness-cases.c` actually trackable?** `.gitignore` appears to use whitelisting, and this diff adds entries for new docs and reviews but not this source file. If it is ignored, a commit would omit a file the build requires. Please run `git check-ignore -v tests/native/harness-cases.c`.
- **Q2. Does assertion 14 pass with its wider mask?** It now rejects `NATIVE_SINGLE_PAGE` as well as `NATIVE_SHARED_STACK`. The 501/502 run only exercised the old `&2` form, so the final run is the first test of this stricter condition.

The final evidence should show:
- 502 passed and 0 failed.
- A `harness_layout` matching BANK1 6051 and library SHA `5c252792…`.
- A true standalone Chain result. The `false` in the prior report is not a Chain failure: `chain_ok` starts false and that block runs only when `_test_failed` is 0, so Chain did not execute.

## Limitations

- I reviewed static sources and supplied reports only, and ran nothing.
- The full `.gitignore`, final native report, package ZIP and COM contents were not supplied. Manifest hashes are therefore unverified against the files.
- The Inline review assumes the MMU port behavior shown by existing project code. Physical-hardware behavior remains untested, as the checklist states.

### Codex reconciliation

1. Deferred for the generic test-native.py CLI input list. Its executed binary artifacts remain hash-bound. The CRT runner exercised here already globs all native test sources, including harness-cases.c; its passing report binds the moved helper source and final binaries. No gap affects this task's CRT evidence. General runner source-provenance maintenance remains a low-priority follow-up.
2. Resolved after a passing final run. Generated docs/crt-harness-evidence.json from capture20261108T132041.079336Z-47364: **502 passed,0 failed**, all checks true, standalone saved Chain true, owned/cache15/15, exact frame/color/program outputs and all sleep arguments match. Package manifest is refreshed after commits to include the evidence and final guide hashes; ZIP contents are checked against every manifest hash.
3. Accepted wording correction. The native assertion compares SIZE and flags, not code bytes. The separate host comparison explicitly reads USES.COM and ORDER.COM and confirms byte-for-byte equality: both6655bytes, flag4, SHA256 098b2a9248cdb96a06b7857a15352c1d96bb80fc9c503c9a1f3ad28756a5eeec. The existing tools/test-zvb.py comparison also uses read_bytes()==read_bytes(); no claim that b.length itself compares bytes.
Q1. Verified trackable. git check-ignore -v tests/native/harness-cases.c reports no ignore match and git status lists it untracked. Existing /tests/native/ parent allowance covers the new file; it is explicitly staged for commit.
Q2. Final correction required: small selected CRT code also fits a single page, so rejecting both allocation flags was an unjustified new test restriction. Changed assertion14 to one>plain, proving that a selected fragment adds executable bytes without requiring extra execution pages. The next assertion still verifies that additional selected exports add more bytes. Assertion11 directly compares plain/unused-import size and flags. All original502 check calls remain. Final full502/502 and standalone Chain passed after this change.

**Review limit disclosure:** the assertion14 correction above was made after the second Opus turn. The user explicitly chose “No, finish with test results and disclose the review limit.” No extra review was run and this review is not sealed/reused as final-code approval. The final correction is test-only, independently verified by the complete passing run. There are no known current CRT implementation or harness functional failures. The generic runner input-list source-provenance item is deferred as stated above; physical-board acceptance remains for the supplied hardware-test package.

Final harness: resident16373bytes, BANK1 6072bytes, ZPL SHA256 9bdddec73b9445da7a0164df8c4ee7090b9d570108074b3b933ded848c11f6be. Source/compiler/runtime product code and CRT.ZPU implementation were unchanged by this task. All verification is emulator-based; no physical-media writes or physical-board pass is claimed.
