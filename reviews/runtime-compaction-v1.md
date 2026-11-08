# Review: runtime-compaction-v1

## Opus turn 20261107T225008-757d8f68

Mode: code; read-only

# Review: runtime-compaction-v1 (initial, read-only, against ebdb7e7)

I find no blocker that is proven by the supplied evidence. I found one likely repository-integrity defect, three substantive correctness and robustness issues, and several gaps in the evidence. I ran nothing; no test results are claimed here.

## Established or near-established defects

**1. High — `.gitignore`: two new source files are not whitelisted.**
- **Trigger:** the next commit, or a clean checkout and build.
- **Evidence:** `.gitignore` uses an explicit whitelist for individual files (`!/src/compiler_table_arena.c`). The diff whitelists `compiler_layout.{c,h}`, `compiler_layout_emit.c`, `package-runtime-layout.py`, `test-compact-programs.py` and `package-file-unit.py`.
  - `src/compiler_file_link.c` is missing from the whitelist. It is listed in `PASCAL_CORE` in `CMakeLists.txt`.
  - `src/file_direct.asm` is missing from the whitelist. It is a `DEPENDS` input of `SYSFILE.ZPU` and is assembled by `package-file-unit.py`.
- **Impact:** if they are ignored, the commit omits them and the clean build breaks.
- **Verify/fix:** run `git check-ignore -v src/compiler_file_link.c src/file_direct.asm`, then add both whitelist entries. Use a leading slash on the `tools/package-file-unit.py` entry for consistency.

**2. Medium — page selection ignores call depth and recursion (`compiler_bridges.c` `cc_entry_reference`, `compiler_front.c` `cc_native_reserve`).**
- **Change:** local calls no longer call `cc_require_full_memory()`. `cc.stack_bytes` is now the maximum of individual reservations, not the sum along a call chain.
- **Trigger:** a `{$A+}` program that recurses or nests calls deeply, for example a recursive procedure with String value parameters. `push_string` places each copy, up to 256 bytes, on the CPU stack.
- **Old capacity:** such programs ran with full memory, roughly 15–30 KiB of stack.
- **New capacity:** they can now be selected for one page. Stack space is then only about `0x7C00 − image_end` (about 9 KiB for a small program).
- **Impact:** the guards fail cleanly with error FF rather than corrupting memory. Still, valid programs that previously ran now abort, in both RAM Run and saved mode. No test exercises depth. `RECURSE` only recurses 5 levels.
- **Fix:** keep conservative windows for possibly recursive calls. For example, call `cc_require_full_memory()` when `entry==0` (a self, enclosing or forward call; this is the existing `R_CALLS` path). Alternatively, sum frame demand over an acyclic call graph. Add a deep-recursion test with a String parameter.

**3. Medium — `SYSFILE.ZPU` lookup and compiler binding (`compiler_file_link.c` `cc_file_native_prepare` → `cc_units_graph("sysfile",0)`).**
- **Lookup order:** the documented order is the compilation root first, then the ZPL directory. Only an explicit `uses SysFile` is rejected (`visible`).
- **Shadowing triggers:**
  - A user unit named `SysFile` writes `SYSFILE.ZPU` beside its source.
  - A copied or stale `SYSFILE.ZPU` sits in a project directory.
- **Impact of shadowing:** every file program in that directory links that unit as its native file backend, or fails with a confusing STALE/FORMAT error at end-of-program.
- **Missing binding:** `SYSFILE.ZPU` is bound only to the runtime profile. Nothing ties it to this compiler's `FileJob` layout, operation numbering or export set. `package-native.py` checks it only at packaging time, so a mismatched TF install is accepted.
- **Fix:**
  - Resolve `sysfile` only beside the loaded ZPL.
  - Reject authoring `unit SysFile`.
  - Embed the expected `SYSFILE.ZPU` checksum in the ZPL and compare it with `stream_hash`.
  - Emit a specific diagnostic when the file is missing.

**4. Medium — full-Real data base relies on an unchecked assumption (`tools/package-runtime-layout.py`).**
- **Current code:** `full_end = s__REAL_DATA + l__REAL_DATA` (0x7FFF) becomes `RUNTIME_FULL_END`, the Real global base.
- **Missing check:** the integer path computes `end` as the maximum extent of all areas and asserts gap separation. The full-runtime path has no equivalent check that `_REAL_DATA` is the highest-placed area, and none that `full_end ≤ 0x8000`.
- **Impact:** if any other full-runtime BSS, data or code area lies above it, now or after a future runtime edit, Real-program globals silently overlay runtime state.
- **Fix:** compute `full_end` as the maximum area end, as the integer path does, or assert equality. Fail the build otherwise.

## Lower-severity defects and hardening

**5. Low — `compiler_link.c` `cc_link_apply`: zero-width last global.** The CL_DATA check is `offset >= globals_bytes → invalid`. A zero-width global declared last (for example an empty record) gets an address equal to `globals_bytes`. Taking its address or passing it by reference then fails with error 99. Allow `offset == globals_bytes` only when the referenced width is 0, or allow `offset <= globals_bytes`.

**6. Low — `cc_original_entry` and `tools/screen-library.c`: operand classification by address range.**
- Whether an operand is a runtime relocation is decided only by `value ∈ [GAP_LAST, OWNED_END)`.
- A copied first instruction whose immediate is a constant in that range would be wrongly relocated.
- Use the generated `runtime_relocations` sites instead, or assert that each copied adapter instruction's operand site appears in that table.
- For consistency, also route `RP_PROGRAM_ENTER` in `cc_place_code` through `cc_runtime_address`.

**7. Low — `compiler_files.c` `cc_file_transfer`: unnecessary handlers retained.**
- `job_start(ZF_READ/ZF_WRITE)` marks the typed-component handler even for Text transfers, where the opcode is immediately replaced.
- This retains `component()` and its callees in every Text-only program, which works against the recovered-space goal.
- Mark the operation only when it is actually dispatched.

**8. Low — `package-file-unit.py`: initialized C state is not rejected.**
- It rejects initialized `_DATA/_BSS/_INITIALIZED` text, but `_INITIALIZED` never carries text.
- Any nonzero `_INITIALIZED` area, or any `_INITIALIZER`/`_GSINIT` area, in `file_runtime.c`, `path.c` or a selected `z80.lib` member would ship with zero-initialized state at runtime.
- Fail closed when any of those areas has nonzero size.

**9. Low — test strength weakened.** `test-unit-init.py` and `test-unit-real.py` no longer byte-compare non-Real prefixes; they only check that header bytes 132–133 equal 6042. The build already emits `linked_integer_runtime.bin`. Compare the saved prefix against it, excluding the known hook, keyboard and `program_enter` patch sites.

## Questions (not established)

**Q1. RAM-mode file handles.** `ZP_DIRECT_FILE` forces `MEMORY_MODE 0`. In RAM Run, descriptors are therefore opened by the program directly rather than through the parent-owned `zf_service` path, which the rename comment still describes. Each handle is closed within the synchronous call. Please confirm there is no abnormal termination path inside `zf_dispatch`, such as a worker stop, that could leak a descriptor into the IDE process.

**Q2. NMI exposure in CRT.** CRT helpers now map VRAM into page0 with DI. Is there any NMI source on the supported board? An NMI vectors to 0x0066 inside VRAM and cannot be masked.

**Q3. `cc.links` rewinds.** Every global access now records a CL_DATA link. Do all existing paths that rewind `cc.emitted` (other than `shared_startup`, which does this correctly) also rewind `cc.links`? A stale link with `site < emitted` would patch unrelated code.

**Q4. User units plus file I/O.** `cc_file_native_prepare` calls `cc_units_begin`, then link and close, then `sources_finish`. Is this safe when user units were already linked and closed, or are still open? Is a second `sources_finish` call permitted?

## Missing proofs to supply in the follow-up

- **Unrelocated operands:** an audit of unchanged emitters that may still emit raw global or runtime-private operands via `cc_op16`. Candidates include `compiler_params.c`, `compiler_random.c`, `compiler_strings.c`, `compiler_expr.c`, `compiler_routines.c`, `compiler_frames.c`, `compiler_module.c` and `compiler_late.c`. Also check `S_ADDRESS` variables declared `absolute <global>`. A full native run of the existing program corpus under compact layout would settle most of this.
- **Bank budget:** final bank-map and headroom evidence (`zpascal.banks.json`) for:
  - the new `BANK19` pragma in `compiler_layout.c`;
  - `compiler_layout_emit.c` and `compiler_file_link.c` in BANK9;
  - `compiler_chain.c` moving from BANK2 to BANK10;
  - the extra BANK11 unpack asset;
  - compiler C-stack growth from `UNIT_LINK_ROOTS` rising from 128 to 162.
- **Snake and link capacity:** re-measured Snake, since docs still state 171 bytes free. Include link-record arena capacity on a large program, because global accesses now cost 8-byte link records.
- **Final-source native qualification**, covering:
  - one-page programs with console input: ReadLn, KeyPressed, ParamStr, Random, Ctrl-C break;
  - Chain and Execute parents that also perform file I/O;
  - a CHN child performing file I/O;
  - Execute into a compact one-page target;
  - Real programs with explicit Start below `RUNTIME_FULL_END` (expect a clear error, currently error 99).
- **Staleness and shadowing:** a SYSFILE staleness or shadowing test, after finding 3 is fixed.

## Overall assessment

The architecture fits the assignment without unnecessary new mechanism:
- static runtime fixups derived at build time from objects;
- CL_DATA and CL_RUNTIME link records;
- a `CF_RUNTIME16` unit reference kind;
- SYSFILE linked through the existing fragment linker;
- alias-aware file validation;
- one-, two- or three-page startup with cold page freeing that preserves IFF.

Header validation, disk staging translation and fixup ordering (runtime move, then hooks and keyboard patches) look consistent. Readiness depends on findings 1–4 and on the final native evidence above.

### Codex reconciliation

Initial dispositions (final native rerun and follow-up still to come):

1. Adapted. `git status` already listed both files as untracked and individual `git check-ignore -q` did not ignore them, so an omitted-source defect was not established. Added explicit whitelist entries anyway and normalized the tool entry; both sources will be explicitly staged.
2. Accepted, with a more conservative implementation than the proposed entry-zero test. A root-to-defined-leaf call uses its checked recorded budget; any call made from a Pascal routine, or an unresolved forward call, retains independent windows. This covers self/enclosing/forward cycles and deep acyclic nesting without adding a call-graph subsystem. Added a 50-level, 255-byte String-parameter recursion case; small recursion now explicitly expects three pages.
3. Accepted. Private SYSFILE bypasses project-directory lookup, reserved authoring is rejected, and a generated `file-unit.h` embeds the exact expected artifact checksum in the compiler. The existing UNIT_MISSING/UNIT_STALE diagnostics remain the actionable failure contract; docs name the required stock file. Added project shadow, valid-but-mismatched checksum, missing file, and reserved-authoring probes, including preservation of the shipped artifact.
4. Accepted. Full extent is now the maximum nonempty area end from the full link map, bounded at $8000; no assumption that REAL_DATA is last remains.
5. Accepted. CL_DATA permits the one-past address for zero-width final storage. Sanitized placement tests pass 83 assertions, including one-past acceptance and beyond-end rejection. Added a native empty-record/Addr case.
6. Accepted. Copied immutable callback prefixes are classified by actual generated relocation sites for the three permitted source entries, with build-time shape assertions. CRT generation records only explicitly semantic runtime operands; ordinary immediates and code references stay literal/code references. PROGRAM_ENTER patching also uses cc_runtime_address.
7. Accepted. Only actual invoke/transfer/default-output dispatch paths retain operations. Text preparation/fields/newline operations and partial block variants each retain the exact handler, without retaining the overwritten initial typed opcode.
8. Accepted. Any nonzero INITIALIZED, INITIALIZER, GSINIT or GSFINAL area fails packaging, as do initialized/relocated writable state bytes.
9. Accepted. Added an exact comparison with linked_integer_runtime.bin outside enumerated header, CBreak and three-byte hook/keyboard/program-enter patch sites. It matches all five sampled earlier WIP saved prefixes (MIN/INT/CRT/BLOCKIO/TEXTIO) and is part of the final compact and unit suites.

Q1: The direct C dispatcher has no Pascal callbacks or compiler polling/nonlocal stop path. Logical streams return an action to Pascal after C returns; every native disk handle is closed within the synchronous C path. The pinned OS kernel/vfs.asm zos_vfs_close always clears the descriptor entry after its file/driver close, even when close reports an error; the existing injected-close-error suites model that retirement and pass. Keyboard IRQ delivery alone does not unwind C. No added backing-page/descriptor service is required.
Q2: The configured emulator has no NMI generator caller outside its CPU API and a standalone CPU timing test, and the pinned OS has no NMI service contract. The established SDK/our existing ZVB leaves already use DI-protected page-zero mapping. This review does not establish physical-board/NMI acceptance; that limitation stays explicit.
Q3: Audited every cc.emitted assignment. Production reset/initialization starts with zero links, unit linking only advances, and shared_startup rewinds emitted/relocations/links together. The other rewind is in dormant compiler_groups, whose open/begin APIs have no production caller and whose groups pointer is never assigned by the parser. Its unsupported group persistence format is not newly advertised or activated by this change; no production stale link is introduced.
Q4: The normal user catalog is linked and closed before late helpers. SYSFILE opens a fresh catalog and closes it after its own link. sources_finish is idempotent for its retired pending/text arenas (buffer_drop(255) succeeds), and appended unit source descriptors are then finalized again. Native typed/block/Text cases already exercise the second catalog; added a retained user-unit-plus-file case to explicitly exercise both in one program.

Additional Codex findings resolved: standard-file tables needed CL_DATA records and their C validator needed the actual header data base; recursion workspace copies needed data relocation; large disk scratch files needed the verified overwrite-status range extended beyond the old 48 KiB ceiling (captured failure status D6 matched the scratch extent); two-page aliases must clamp the stack floor to $8000 for images ending below it. Added the corresponding low-standard-stream, workspace, recovered-space and malicious-header boundary checks. Stack-limit arithmetic now lives in compile_limits_at, its existing owner, rather than increasing the live frame of cc_place_code and overflowing BANK7 under SDCC. Final bank budgets and native evidence will be recorded before follow-up.


Final Codex evidence supplied for the one follow-up:

- Runtime/linker checkpoint `287e183` is committed against baseline `ebdb7e7`. Product build17 fits all nineteen banks (maximum16247 bytes), resident16115, ZPL276597. Support bank11 is11360 and Inline/transfer bank15 is11488 bytes.
- The large ZVB unit exposed a genuine serialization peak: no page was available for its artifact. Source access (including declaration names and replayed constants) now lives in retained bank11. Finalization lends only inactive expression/file/Inline-transfer banks. The active compiler coordinator stays mapped: SDCC banked-return tokens contain physical pages. The revised production phase passes1917 sanitized assertions across six injected native failure boundaries, preserving active coordinator/source pages and restoring unique ownership.
- Final compact suite:316/316 checks across23 programs, with stable input hashes. RAM and cold physical page counts match; saved runs delete all compiler/ZPL/ZPU/PAS/INC files. Exact immutable integer-prefix comparisons, startup IFF/rejected-pfree cleanup, low Start/standard streams, one-past empty globals, private SYSFILE binding and the recovered-space test all pass. The latter uses27004 globals+11939 code,9247 bytes beyond old capacity.
- Native unit suite1763/0, initialization/Chain643/0, unsigned1246/0; all completion images authenticated. Large known root argument frames now use recorded demand rather than the old automatic third-page rule. Self/enclosing/forward calls retain full mappings. The deep String recursion proof uses32 levels; the original50-level fixture exceeded the existing C800-FC00 CPU-stack window and correctly hit its guard, so it was not a valid success expectation.
- Final actual-IDE error/edit/recompile, Ctrl-C ReadLn cleanup, and file-error exact source recovery all pass. Host front-end180362, editor4010, source45809, placement83, fragments1020, unit references1824/types80, direct files1538 (legacy1522/experimental12583), image staging1642949/header44577, publication366841 across2528 scenarios passed.
- Fresh ZVB authoring produces60330 bytes (30578 code,908 globals). Com-file Snake is35699 bytes versus the same-source46041-byte preserved checkpoint: exactly10342 recovered, with10381 static bytes free. It passes saved180-frame execution without unit/source files. All controls, checkerboard, score strip/digits, restart/collision and Escape pass in installed `/opt/homebrew/bin/zeal-native`, with every compiler/unit/source file deleted from its test directory. The remaining all-client ZVB regression is still running on unchanged sources; finalization and the documentation commit will wait for its completion.
- Test observers now authenticate the supervisor image instead of mistaking a child's reused logical address for a return. Private diagnostics read the banked stack argument rather than A. Post-Run exact-prefix comparisons additionally admit only enumerated mutable PC/heap/return-scratch/I/O-target/mode bytes; ordinary newly saved programs still use the strict comparison. The installed-emulator VRAM observer reads at most256 bytes per console command.

No unresolved production finding is known at this point. The standalone zscreentest pre-existing overflow, existing full transfer contract, and physical-board/NMI/audio acceptance limits remain explicitly bounded.

## Opus turn 20261108T005027-de760854

Mode: code; read-only

# Follow-up review: runtime-compaction-v1

The follow-up covers the committed checkpoint `287e183` plus the uncommitted documentation and evidence, against baseline `ebdb7e7`. I ran nothing. All results cited below come from the supplied decision record.

**Verdict:** I found no demonstrated remaining correctness blocker. Every initial finding has a concrete fix in the current source, and none of the dispositions is stale. One proof gap remains material, concerning the new bank-lending change. Four items are low severity.

## Initial findings: dispositions checked against the current source

| # | Disposition | What the current source shows |
|---|---|---|
| 1 `.gitignore` | Adapted (explicit whitelist entries added) | `compiler_file_link.c`, `file_direct.asm`, `unit-phase.c` and `test-unit-phase.py` are whitelisted. Closed. |
| 2 recursion/depth | Accepted | `cc_entry_reference` keeps full memory for `cc.scope \|\| !entry`. That covers self, enclosing, forward and nested calls; only a root call to a defined leaf uses its recorded budget. Unit-internal calls are still flagged per fragment. The `DEEP` case expects 3 pages. The correction to my earlier capacity estimate is noted: the old C800–FC00 guard window already bounded the stack. Closed. |
| 3 SYSFILE trust | Accepted | Local lookup is suppressed (`!strcmp(name,"sysfile")` blanks the project path). Authoring `unit SysFile` is rejected in `cc_units_header`. The embedded `SYSFILE_CHECKSUM` comes from the generated `file-unit.h`. Probes cover shadow, stale, missing and reserved cases. Closed. |
| 4 full-Real extent | Accepted | `full_end` is now the maximum of all non-empty `s__`/`l__` area ends, asserted ≤ 0x8000. Closed. |
| 5 zero-width global | Accepted for programs | `offset > globals_bytes`; 83 host assertions. See low item 4 for units. |
| 6 copied operands | Accepted | Classification uses the generated `runtime_copy_operands` table and fails closed with error 99 for an unknown entry. `PROGRAM_ENTER` goes through `cc_runtime_address`. See low item 2. |
| 7 handler retention | Accepted | `retain_operation` is called only at `invoke`, transfer and default-output dispatch. |
| 8 C initialization | Accepted | Packaging fails on any non-empty `_INITIALIZED`, `_INITIALIZER`, `_GSINIT` or `_GSFINAL` area. |
| 9 prefix comparison | Accepted | `compact_prefix_matches` compares against the linked golden prefix, excluding only enumerated patch sites; the executed variant adds an enumerated mutable set. |
| Q1–Q4 | Answered | I accept the Q3 audit that only the dormant groups path rewinds `cc.emitted`. NMI and physical-board acceptance remain explicitly out of scope. |

## Material missing proof (medium)

**M1. Which compiler banks are mapped during bank lending (`library_phase.c` `code_phase_unit_finish_begin` now also drops `CODE_TRANSFER`).**

The sanitized `unit-phase` test proves page ownership, uniqueness and restoration across all six failure boundaries. It does not prove code residency. Two properties need evidence:

- **Callers.** No function that is on the call stack when `finish_begin` is entered may live in the expression, file-compiler or transfer bank. SDCC banked-return tokens name physical pages, so returning into a lent bank would crash. The entry chains to check are:
  - `cc_place_code` ← `compile_project`
  - `cc_units_link` ← `cc_file_native_prepare` (BANK9) and ← the ordinary user-unit link
  - `cc_units_write` ← `compile_unit`
- **Callees.** No function reachable between `begin` and `end` may live in those banks. The reachable set includes:
  - `cc_link_apply`, `cc_layout_finish` (BANK19), `cc_units_link_object`, `cc_file_native_bind`, `append_metadata` → `cc_mapping`
  - `cc_require_real`, `cc_require_full_memory`, `cc_require_hardware`
  - `cc_units_write_object` → the export/type writers → constant and literal replay (`cc_constant_literal`, `cc_next`, any Real-literal helpers)
  - error paths reached through `cc_fail`

The native suites exercise the common paths and are strong evidence. However, the error and cancellation branches inside the finish phase, and unusual exported-constant kinds, may not be covered.

Requested evidence: a build-time check over `zpascal.map` and `banks.json` that resolves both call sets above to their banks and asserts that neither touches a lent bank. The same check should confirm that the `ZP_STATIC_LAYOUT` gating in `compiler_sources.c` (pragma to BANK11) and in `library_phase.c` is evaluated identically in the product target. Both files obtain the macro through `compiler.h`, so a target that compiled either one without `ZP_OPTIONAL_REAL` would drop `CODE_TRANSFER` while source access was still in the transfer bank.

## Low-severity items

**L1. Chain/CHN compile limit changed.**
- `compile_limits_at` now uses `cc_required_stack()` for every build with code, including `COMPILE_CHAIN`.
- The CHN header does not carry a stack field: `native_decode` assumes 2048, and the child runs under the parent's budget.
- A CHN with large known frames near its End limit can therefore newly fail compilation with error 99 where baseline accepted it.
- Confirm this is intended, or keep the 2048 floor for Chain builds to preserve the existing contract.

**L2. Fail-closed coverage of `cc_runtime_copy_operand`.**
- Any `cc_original_entry(...,0)` call with an entry other than RT_CHAR, `input_open` or `input_echo` now fails compilation with error 99.
- That is safe behaviour, but confirm that every non-staged call site in the hooks and console code uses only those three entries.
- Also confirm that a compact program exercising `ConOutPtr`/`ConStPtr` or the echo/open adapters is covered natively. No such case is in the 23-program suite.

**L3. IDE options use stale compiler state.**
- `option_limits` calls `compile_limits_at`, which calls `cc_required_stack()` and reads `cc.stack_bytes` and `compiled_data_base` from the most recent compile.
- After compiling a different program or a unit, the bounds shown in the options screen may not match the current build.
- The compile itself rechecks these limits, so the effect is user-interface accuracy only. The `LOWSTART` probe confirms the banked call is reachable from the options screen.

**L4. Zero-width final global inside a unit.** `cc_units_write_object` still rejects `UR_GLOBAL` with `delta >= globals_bytes`, which is inconsistent with fix 5. An empty record declared last in a unit implementation cannot be authored. Allow `delta == globals_bytes`, or document the restriction.

## Limits of this review

- The 15-client ZVB regression has not finished; its results are pending verification.
- The standalone `zscreentest` overflow is the documented baseline limitation, not a regression.
- Physical-board, NMI, audio and DMA acceptance are out of scope.
- I relied on the supplied test counts and bank sizes (maximum bank 16,247 bytes; resident 16,115; ZPL 276,597) and did not verify them.

Apart from M1, which is a proof request rather than a demonstrated defect, the implementation meets the authorized task without new architecture.

### Codex reconciliation

M1 accepted and closed by additional verification, with production code unchanged from287e183. `tools/check-unit-banks.py` reads only objects named in the product link command, their emitted SDCC assembly, `zpascal.map`, and `zpascal.banks.json`. It verifies map/binary agreement, resolves direct and SDK-banked calls, includes all branches of callees, rejects unresolved/indirect calls in the reachable set, and walks every direct ancestor caller. The `cc_place_code` interval stops at finish_end, so its later restored-bank activation placement is correctly excluded. It resolves16 active callers, plus69/145/213 reachable functions for place/link/write; none touches lent banks6/10/15. All specifically requested functions, including append_metadata/mapping, require flags, constant/string/Real replay, cc_next and cc_fail, are present in the closure. Actual emitted transfer-drop instructions and linked source-access placement confirm static-layout gating; both compile in unit_runtime_core with the two enabling defines. Source access remains in bank11, the active compiler coordinator remains in bank14. The full hash-bound result is `build/unit-banks.json`. The existing1917-assertion fault test supplies the complementary ownership/recovery proof.

L1 adapted as an intentional conservative bound for this unreleased compiler. CHN still borrows the parent runtime/budget and carries no new stack field. Compilation rejects a near-End image if its known demand does not fit; no historical ABI reader or acceptance promise is required. Existing Chain/Execute native checks pass. Documented in runtime-compaction-notes.md.

L2 accepted as an audit/proof request. Every non-staged clone comes from the original RT_CHAR target, RP_INPUT_OPEN or RP_INPUT_ECHO: hooks.character and console.native read the original char jump; open/echo use their private gates; generated hook/status entries use staged=1. The generated copy table is built from exactly those three canonical entries. Public hook variables deliberately retain the existing full runtime through cc_hook_variable, so a compact public-hook program is not the present contract. An added ConOutPtr/ConStPtr callback probe passes RAM, detached and cold execution (strict full-prefix expectation); ordinary compact console adapters, KeyPressed/ParamStr/Random and Ctrl-C ReadLn recovery already pass. No additional clone site was found.

L3 accepted as a documented UI limitation, not a linker correctness defect. Options preview the most recent compiled layout; final compilation recomputes and checks actual placement/demand. The supplement directs users to recompile before relying on the preview. No production change was made after the review.

L4 adapted to the precise supported boundary and documented: taking the address of a zero-width unit implementation global at the data end is unsupported. Merely exporting an empty record type is not prohibited; program one-past addresses remain supported and tested. This narrow authoring edge is not required for runtime compaction and fails closed.

Final qualification after the follow-up: all15 ZVB clients were compiled in the original completed run. Its only two native failures were the PAGE fixture's old page-alignment assertion, now necessarily false at the linked base. The corrected Addr-derived crossing passes33 targeted assertions in RAM and detached execution. The old observer's exact15 count was also wrong because it captured3 earlier hardware executions: the raw journal authenticates18 entries and18 supervisor returns. The corrected observer filters client stages. The composite `build/zvb-qualification.json` preserves the original failed report and the targeted resolution rather than rewriting it as a clean original run. CRC32 golden output and Snake180-frame execution passed in the original run. Memory and Com-file Snake bytes are identical. Installed-emulator controls/layout pass with all compiler/unit/source files removed.

The final unit suite still passes1763/0 after replacing its numeric global-base expectation with a linked-header read. TF copy/startup checks pass and preserve existing files. Current precompiled Snake/SPRITES/ZVBCHECK/FIRSTBAR/CRC32 and stock ZVB.ZPU are refreshed. New post-review code is limited to verification scripts/fixtures; reviewed production sources are byte-for-byte287e183. The human-readable supplement and evidence document all retained low-severity limits. There is no unresolved blocking finding in the production scope.
