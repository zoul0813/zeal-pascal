# Review: runtime-placement-v1

## Opus turn 20261107T193543-29ffa4ee

Mode: code; read-only

# Review: runtime-placement-v1 (baseline c2af3c6)

The symbolic stream design is sound in the parts I could inspect:
- **Record format:** the 8-byte record and its validation in `cc_link_reference` and `cc_link_apply` match the specification.
- **Ordering:** `DATA_ADJUST` ordering after the coefficient fixups is additive, so it is order-safe.
- **High-byte carry:** carry is preserved because bytes are extracted after placement.
- **Group catalog:** the new layout still fits (48 + 2000 + 7140 + 256 = 9444 bytes).

The material risks are bank residency after the code moves, total RAM capacity, and regression evidence that has not yet been supplied.

## Established defects / material risks

**1. High: every program with globals now consumes link-arena pages and loads the linker bank.**
- **Where:** `compiler_front.c` `storage_address`, `compiler_inline.c`, `compiler_build.c` `cc_place_code`.
- **Trigger:** in the production `ZP_UNITS_EXPERIMENT` build, every non-unit global load or store emits a `CL_DATA` record.
  - So `cc.links>0` for almost every program.
- **Impact on memory:** each global reference adds 8 bytes to `TABLE_LINK`.
  - These pages come from the same 32-page physical pool as the image, staging and source pages.
  - About 10K references would take roughly 80 KiB, which is five pages that the baseline did not need.
- **Impact on behaviour:** with `data_origin==GLOBAL_BASE`, every `CL_DATA` rewrite is a no-op today.
- **Impact on I/O:** every compile now runs `code_phase_unit_finish_begin`/`end`. That drops and reloads the file and expression banks and loads `BANK19`, about 48 KiB of extra disk reads and allocations per compile.
- **Capacity claim:** the statement "unchanged capacity" holds only for the 4-byte relocation stream, not for total compile memory.
  - Large programs that compiled at baseline can now fail with error 99 or `COMPILE_PLATFORM_FAILURE`.
- **Fix or verification:**
  - Run the existing macro-on corpora and record peak compiler pages against baseline: `zoptionalcorpus`, `zunitcorpus`, `zunitrealtest`, `zunitinittest`, `zzvbtest`, `zscreentest`.
  - Include the largest-globals program.
  - Alternatively, defer `CL_DATA` recording until compaction actually needs it, or record it only when `data_origin` can differ. Then skip the phase swap when no `CL_CODE` byte or adjust records exist.

**2. High (verify): metadata tables moved from a permanent bank to deferred `BANK16`.**
- **Where:** `compiler_tables.c` `#pragma codeseg BANK16`, and `compiler_build.c` `build_close`.
- **Trigger:** `build_close` is entered through `code_select(CODE_BUILD)` with no `code_phase_restore`. When `b==cc.image` it calls `cc_tables_end`, which now lives in `CODE_UNITS`, a deferred bank.
  - Previously this code was in `BANK9`, which is always loaded.
  - The call becomes dangerous if `code_phase_return` failed after a RAM run or Execute, or on any IDE path that closes the build while phases are parked.
- **Impact:** a banked call into `code_pages[15]==255`, which maps a foreign page and crashes the IDE.
- **Verification:**
  - Audit every caller of a `BANK16` table or member symbol that is reachable without `code_phase_restore`: `build_close`, `compiler_artifact.c`, `save_build`, and build location.
  - Add a fault test for restore failure after Run, followed by `build_close`.
  - Or keep `cc_tables_end` (or a guard) in a resident or permanent bank.

**3. High (verify): `unit_fragment_core` moved to `BANK19`, but one call site runs outside the linker window.**
- **Where:** `compiler_units_graph.c` `cc_units_graph_validate`.
- **Trigger:** the function calls `cc_unit_descriptor` (`BANK9`), which calls `cc_fragment_source_capture`. This happens before `code_phase_unit_link_begin()`.
  - If that symbol is in `compiler_fragment_source.c`, it is now in an unloaded bank.
  - The native test passing suggests it may live elsewhere, but I cannot confirm that from the supplied files.
- **Verification:**
  - Provide `zpascal.map` bank assignments for all `_cc_fragment_*` symbols.
  - Better, add a build-time check that every `BANK19` symbol is referenced only from the linker windows: `link_begin`/`link_end` and `finish_begin`/`finish_end`.
  - The same check should cover `compiler_fragment_access.c` callers in the reader and ranges code (`BANK18`/`BANK1`).

**4. Medium: the overlay member journal changed layout without a version change, and member link records have no consumer.**
- **Where:** `compiler_members.c` (header byte 4 still `2`; link section inserted before the late references).
- **Trigger:** any reader that ignores word20 will misalign the late-reference section whenever `links>0`.
- **Truncation:** `cc_group_member_end` truncates `cc.links` back to `link_first`, so member link records live only in the spool.
  - Nothing shown applies them at overlay placement.
  - This is harmless only while `CL_DATA`/`DATA_ADJUST` are no-ops. It becomes a defect once compaction or byte imports reach members.
- **Fix:** bump the member format version, or reject `links>0` in the readers until they apply the records. Add an `output-host` case with `links>0`; the current `member_setup` always uses 0.

## Questions requiring evidence

**5. Code rewinds versus `cc.links`.**
- `cc_group_*` resets `cc.links`, but any other path that rewinds `cc.emitted` (and possibly `cc.relocations`) would leave stale link sites.
- At final placement, `cc_link_apply` would then overwrite unrelated re-emitted bytes. Candidate paths: constant folding, operand reordering using `before`, and `cc_original_entry` copies.
- Please grep for assignments to `cc.emitted=` and `cc.relocations=` outside groups and units.

**6. Bank-lifetime details.**
- **Physical page reuse:** `code_phase_unit_finish_begin` reloads banks into possibly different physical pages, while banked return tokens are physical pages. Confirm that `cc_place_code`'s caller chain contains no `BANK6` or `BANK10` frame, and that the same holds for `graph_validate`'s caller (`cc_units_imports`).
- **Nesting:** `link_end`/`finish_end` drop `LINKER` unconditionally. Confirm that no `cc_units_types_ready`/`graph_validate` call can occur inside a `finish_begin` window, for example through `cc_unit_types_write` or import-symbol materialisation. A nested call would unload the outer linker.
- **Failure state:** if `drop_bank(CODE_LINKER)` fails in the end functions, the file and expression banks remain unloaded and the error is returned. Confirm that no cleanup path calls into those banks before the next `code_phase_restore`.

**7. `CL_DATA` bound.**
- `offset>=globals_bytes` is rejected.
- Can a zero-width global, such as an empty record or an empty-range type, be the last allocation? If so, a valid program fails with error 99.

**8. Byte-import addends.**
- Byte references require `value-CC_CODE_BASE < emitted`, whereas word references accept any addend.
- Intended? An end-of-fragment label or a negative addend would be rejected only for byte kinds.

## Test gaps

**9. Missing regression evidence.**
- The macro-on native corpora are not in the evidence, and this change touches every program. Positive IDE-unit and cancellation runs are still pending.
- `test-typed-unit-rejection.py` checks only banks `(16,17)` for release, not `18`.
- `DATA_ADJUST` with a non-zero data delta and member journaling with links are covered only on the host, or not at all.

## Limitations

- I could not see `compiler_fragments*.c`, `compiler_late.c`, the compile/seal callers, or the overlay publisher.
- No tests were executed by me. The evidence cited is only the supplied checks JSON and the assignment's own claims.

### Codex reconciliation

1. **Accepted; reduced scope.** Removed program-global and Inline DATA producers
   until compact data placement is activated. The only production record creator
   is final unit metadata for retained low/high references. Ordinary programs
   keep their existing relocation memory and skip the linker finish window when
   no byte references exist. The original four-byte relocation ledger is intact.
   This removes the reported per-global capacity regression instead of relying
   on small corpora to dismiss it. Native corpus qualification is still required
   when compact globals are activated.
2. **Accepted; fixed at the owner.** `cc_tables_end` alone stays in permanent
   BANK9 in its own `compiler_table_arena.c` translation unit. A shared private
   arena state header keeps disposal independent of table traversal. Its retained
   data and page APIs need no deferred compiler code. Other
   table operations remain BANK16, called only during compilation after restore.
   `build_close`, artifact output, and source location cannot call a deferred
   arena disposer. The native test verifies both `b_cc_tables_end == 9` and the actual
   function address in BANK9. A trial using a mid-file codeseg pragma changed
   only the advertised bank; map/assembly inspection caught that mismatch before
   qualification. It was replaced with the separate translation unit.
3. **Verified, no defect.** `compiler_fragment_source.c` explicitly selects
   BANK11; `compiler_fragment_access.c` selects resident TEXT. Only generic
   `compiler_fragments.c` moves to BANK19. The map's bank symbols and resident
   addresses confirm capture/children/overlap/read stay in BANK11 and live/byte
   remain resident. Unit ranges call layout lookup within the outer finish
   window. Source-reader validation runs inside the loader's link window.
4. **Accepted; removed premature format change.** Reverted all new overlay
   member/group fields and journal serialization. There is no parser-stage
   symbolic emitter at this checkpoint; final unit linking occurs after groups
   close. The dormant journal retains its original format and tests. Its code
   retains the proposed BANK16 placement to leave space in BANK12 for native
   test observers; no member layout or serialization is extended. Supporting
   compact globals during parsing will require explicit rewind/journal handling.
5. **Audited.** Emission resets occur at initialization, dormant group closure,
   and the monotonic final unit append. Symbolic records are created only by
   `append_metadata` after the final unit append, so none spans a parser rewind.
   `cc_original_entry` runs before final placement and copies generated helper
   instructions, not imported byte fragments; there is no new symbolic operand
   for it to copy. This is a checkpoint invariant, not a general rewind API.
6. **Audited.** Finish windows start from BANK9 coordinators or BANK7 placement,
   called by BANK14 after parsing; no BANK6/10 return frames survive. Graph
   validation closes its link window before calling `cc_units_types_ready`.
   Metadata writing/serialization and link-object paths contain no nested
   graph/types coordinator call. On an end/drop failure the caller immediately
   returns a platform failure; compile cleanup uses retained/BANK16 owners and
   retries finish-end after releasing tables/images. It does not resume file or
   expression helpers before restore. Existing cancellation/retry regression
   passes; pfree failure of BANK19 specifically has not been fault-injected.
7. **Deferred activation, no production trigger.** No production DATA reference
   is emitted in this checkpoint, including zero-width globals. Before activating
   DATA emission, define whether an address of zero-width terminal storage is
   legal and test it against the chosen data-extent contract.
8. **Verified consistent artifact contract.** Generic fragment `target()`
   rejects every addend outside its target fragment, for all three kinds. An
   end label or wrapping/negative addend is not a valid input even for word
   references. The sparse CODE bound therefore does not reject an artifact
   accepted by the existing word path. Converter documentation already records
   signed/wrapping addends as unqualified.
9. **Accepted.** Native importer rejection now checks BANK19 release too. Positive
   IDE unit and cancellation suites both passed on the initial implementation;
   sanitized publication and original reference suites passed after scope
   reduction. Final rebuilt native evidence is recorded below before follow-up.
   The optional test-execution supervisor moves from overflowing BANK3 to
   spare permanent BANK9 capacity. It stays callable while compiler banks are
   parked. No new bank is added for this fixture placement fix.

Remaining scope: this checkpoint does not reclaim program RAM or change the
fixed runtime prefix. Runtime relocation/compaction, file overlay removal, and
requirement-based execution pages remain explicit later stages.


### Final pre-follow-up checks

- Production byte placement: 16 native checks and 80 sanitized host assertions.
- Real/dependency native corpus: 1,761 assertions, zero failed.
- Initialization/Chain native corpus: 642 assertions, zero failed.
- Typed rejection: 17 checks; IDE cancellation/retry: 51 checks.
- Host frontend: 179,718 checks; original ledger: 1,824 checks.
- Publication: 366,841 assertions across 2,528 fault/cancellation scenarios.
- Supplemental standalone CRT fixture is blocked by a 311-byte resident overflow.
  Test BANK3/BANK12 pressure was resolved using existing banks, but its resident
  capacity still needs a separate fixture change. No full CRT or full language
  corpus qualification is claimed. ZVB selected clients are still running.

## Opus turn 20261107T200858-410342fe

Mode: code; read-only

# Follow-up review: runtime-placement-v1 (baseline c2af3c6)

The fixes resolve the initial findings, and I found no new code defect in this checkpoint. Before preserving it, two items need evidence, not code changes: whether the default build now fails on `zscreentest_bin`, and a positive IDE-unit run against the final library.

## Disposition of prior findings

**1. Per-global link records and capacity: resolved.**
- `storage_address` and `cc_inline` are back to baseline behaviour.
- The only production producer of link records is `append_metadata`, for byte kinds in unit references.
- `cc.links` is cleared by `memset(&cc,0)` at the start of each compile.
- `cc_place_code` enters the linker window only when `cc.links` is non-zero, so ordinary programs pay no extra arena space or bank reloads.

**2. Arena disposal reachable from a deferred bank: resolved.**
- `cc_tables_end` now lives alone in `compiler_table_arena.c`, with a file-top `BANK9` pragma.
- Its ownership state is shared through a near struct, so `build_close` never calls into the deferred `BANK16`.
- The map assertion in `test-linked-addresses.py` (bank tag and actual address) is a good guard against the earlier pragma-tag-only failure.
- The new host case shows that a failed page drop keeps the owner for retry.

**3. Fragment source capture outside the linker window: resolved.**
- `compiler_fragment_source.c` selects `BANK11` and `compiler_fragment_access.c` selects `TEXT`, both via file-top pragmas.
- Only `compiler_fragments.c` remains in `BANK19`.
- Its entry points (`validate`, `validate_source`, `plan`, layout get/close, image close) are reached only inside `link_begin`/`end` or `finish_begin`/`end`, as the call sites show.

**4. Member journal format change: resolved by reverting it.**
- `compiler_groups.c/.h`, `compiler_members.h` and `output-host.c` match baseline.
- Only the bank pragma on `compiler_members.c` changed.

**5. Code rewinds versus link records: accepted as a checkpoint invariant.**
- Records are created only after `cc_units_link` appends unit code. That happens after `cc_block`, after groups close, and before any later rewind.
- The `docs/runtime-compaction.md` note correctly records the obligation for future parser-time emitters.

**6. Bank lifetimes: accepted.**
- The call sites are consistent with the stated invariants.
- The failure path in `compile_project` retries `code_phase_unit_finish_end()` after cleanup.
- That cleanup (`build_discard_image`, `compiler_capture_failure`/`cc_sources_error`, `build_close`) does not touch `BANK6` or `BANK10`.
- A `BANK19` `pfree` failure remains untested; see finding 3 below.

**7 and 8: deferred and verified respectively.**
- There is no `CL_DATA` producer, so the zero-width-global question is moot for now.
- Generic `target()` rejects any addend outside its target fragment for every kind, so `CL_CODE`'s offset bound accepts no less than the word path does.

**9: tests updated.**
- The rejection, positive IDE and media tests now check `BANK19` release.
- The rejection test covers the reserved bits 12/13 and kind 3.

## Remaining items

**1. High (verify; possible blocker): the default build may now fail on `zscreentest_bin`.**
- `zscreentest_bin` is declared `ALL`.
- `package()` raises `ValueError` for any loaded byte outside `0x4000..0x8000`, or any resident area beyond page 1.
- You report `zscreentest`'s resident payload is 311 bytes over 16 KiB, so a plain `zde cmake` build of all targets would stop there.
- **Question:** did `zscreentest` already overflow at c2af3c6? This stage adds only a few resident bytes (`_code_sizes`/`_code_pages` grow by 3, plus the `BANK19` area in `startup.asm`).
  - If it was already over, record that as pre-existing.
  - If this stage pushed it over, the checkpoint breaks the default build. In that case, either fix it or remove `ALL` from this test-only target, with a recorded reason.

**2. Medium (evidence gap): no positive IDE-unit run against the final library.**
- The only positive IDE-unit evidence is `ide_units_before_cleanup_fix` (capture `…194407…`), which predates the arena split and the `optional_test_execution` move.
- The final hash-bound evidence covers byte placement, Real/dependencies, init/Chain, typed rejection and cancellation, but not the positive IDE flow. That flow is the one that runs `build_close` across Run, Execute, park and restore.
- **Fix:** rerun `test-ide-units.py` against `zpascal.zpl` `d5990e70…` before preserving.

**3. Low (test gap): bank-ownership guards are partial.**
- **No `BANK19` free-failure test.** Fault injection of `pfree` on `CODE_LINKER` in `link_end`/`finish_end` is still absent. Use the existing `phase_free_failure` hook in `output-host.c` with an 19-bank `code_required` model.
- **Map checks cover only `cc_tables_end`.** The map assertion now proves that one symbol's placement. A small map-driven check would also catch any future slip that leaves the other pragma-placed files in their CMake-declared bank, as happened in the first `cc_tables_end` attempt:
  - `_cc_fragment_source_capture` in bank 11.
  - `_cc_fragment_source_live` in the resident area.
  - `_cc_members_flush` and `_cc_symbol_value` in bank 16.
  - `_cc_fragment_validate` in bank 19.

**4. Low (evidence gap): the deferred-bank move of the tables is not proven by a corpus run.**
- Every compile in production now depends on `BANK16` holding the table accessors.
- The suites run cover unit and program compiles, but the macro-on language corpora (`zoptionalcorpus`, `zunitcorpus`) have not run.
- Since ordinary programs no longer emit link records, this is regression assurance for the bank move rather than a capacity concern.

## Checkpoint assessment

- **Not blockers:** the deferred full goal (compact core without the Real hole, low program storage, selective file helpers, requirement-based pages) is correctly out of scope and inactive.
- **Required behaviour:** the hash-bound native byte test and detached saved execution show that low/high unit imports keep their full target through placement, including the forced carry (staged 33321, start 37079, final 37632).
- **Invariants held:** the four-byte coefficient stream is unchanged, and the cold allocation stays at ten pages.
- **Before preserving:** resolve item 1 (confirm pre-existing, or fix or reclassify the target) and rerun item 2.

## Limitations

- I could not inspect `zpascal.map`, `compiler_unit_reader.c`/`ranges.c`, `compiler_late.c`, or the callers of `cc_unit_emit_page`. The map-based claims rest on your stated inspection plus the one automated `cc_tables_end` check.
- I ran no tests. All results cited come from the supplied JSON and assignment text.

### Codex reconciliation

1. **Verified pre-existing; no new default-build regression.** An isolated
   `git archive c2af3c6` copy was rebuilt through ZDE with the existing SDK.
   Its CRT fixture fails packaging with a 307-byte resident overflow and code
   areas BANK3=16,540 and BANK12=16,946 bytes. The current fixture has a
   311-byte resident overflow and no oversized code bank. Source bytes for the
   baseline CMake/startup/compiler/table/object files match the commit. Thus
   the ALL-target failure already exists at the baseline; it is explicitly
   recorded as unqualified, not hidden or removed from ALL. IHX/map/build-log
   hashes and the exact comparison are in `docs/runtime-placement-evidence.json`.
2. **Accepted; final positive run passed.** `test-ide-units.py` passed all 66
   checks at `analysis/native-slice/20261107T201128.593535Z-65947`. Captured
   zpascal.bin and zpascal.zpl are byte-hash identical to the final build, whose
   library SHA-256 is d5990e70f27e394e81904d5e4a357df3384aed2680b338fdd5b112a3b7346799.
   This resolves the requested Run/Execute/close/restore evidence gap.
3. **Partly verified; remaining low gap deferred.** Actual map addresses and
   tags were checked for all six requested symbols: arena end in BANK9, source
   capture in BANK11, source-live resident, members and symbols in BANK16,
   generic validate in BANK19. The automated native test continues to guard
   arena-end tag AND address. BANK19-specific pfree injection remains a recorded
   low test gap; no claim of that coverage is made. The owned arena's failed
   release/retry is covered by the sanitized host test.
4. **Deferred to full runtime qualification.** Production programs, unit type
   graphs/dependencies, Real, initializer/Chain, IDE failure/retry and ordinary
   frontend suites passed, but the macro-on full language corpora and compiler
   peak pages have not been run/measured here. No total-capacity/peak claim is
   inferred from unchanged coefficient-record width or ten cold pages.

**Supplemental ZVB evidence:** the larger observer exhausted its 256-stop bound
when a child also executed at the supervisor exec breakpoint's virtual address.
Its failed report is retained as inconclusive. CRC, DMA, GFX and SOUND COM files
were each independently executed under the existing parent probe, with compiler,
ZPU, PAS and INC files absent; all eight success/absence checks passed. Raw
captures, the exact independent observer source and report are in the evidence.

No source edits followed this Opus turn. Both required pre-preservation evidence
items are resolved; no code blocker remains for this checkpoint. The pre-existing
CRT fixture overflow and the low qualification gaps remain explicit. This does
not approve or complete the deferred runtime/RAM implementation.
