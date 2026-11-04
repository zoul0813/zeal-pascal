# Review: disk-handles-20261104

## Opus turn 20261104T110752-0f2ac15b

Mode: code; read-only

# Review: operation-scoped native disk handles (disk-handles-20261104)

**Bottom line:** the core design holds up. The two medium findings are a Close regression introduced by this change and README text that now contradicts the new failed-Execute behavior; both have small fixes. I ran no tests. All statements about passing checks come from the supplied evidence JSON.

## Traces that are correct

- **Acquisition and release.** `zf_dispatch` resets `native_fd`, and `disk_release` always runs before returning to Pascal. That includes failed `activate`, where `native_fd` is set, then `close_record`, then release.
- **No create/truncate on resume.** `disk_open` derives `O_RDONLY`, `O_WRONLY` or `O_RDWR` from `state` and never adds `CREAT` or `TRUNC`. The Reset read-only fallback is preserved because only `READABLE` is set.
- **Text cursor.** `text_peek` now advances and remembers the physical cursor past the cached lookahead byte. Resume seeks there, so the lookahead and physical position stay consistent.
- **Typed header.** The header is read only in `activate`. Flush and Close write it through `disk_open(1)`.
- **Forged fd.** `record->fd` is never read. The host test with a borrowed fd at 0xc012 checks this correctly.
- **Standard-block pointer.** In `standards()`, `record` is restored on success. The `invalid` path is reached only before `record` changes.
- **Descriptor values.** Values up to 127 fit the `int8_t native_fd` given the 16..128 kernel configuration.
- **Close-error precedence.** `disk_release` uses `fail()`, so the first error wins (`resume_faults` checks 0x99 over 0xff).
- **Assertion accounting.** The groups sum to 460, plus 6 setup checks gives 466. The baseline of 480 is retained via `baseline_legacy_assertions`.

## Established defects

**1. Medium — `file_runtime.c` `close_record`: Close now permanently loses finalization when the handle cannot be acquired.**
- **Trigger:**
  - a writable Text file or a DIRTY typed file is Closed, and
  - `disk_open` fails, for example with 0xf3 under fd pressure (the IDE or other devices hold slots), a transient TF error, or the path having been erased.
- **Impact:** `record->state=0` runs unconditionally. Ctrl-Z or the typed count/header is never written, and Close cannot be retried after `IOResult`.
  - Before this change, Close needed no new resource, because its fd was already held.
  - This contradicts the stated `$I-` contract that the caller can retrieve `IOResult` and continue.
- **Smallest fix:** in `close_record`, return without clearing `state` when `disk_open` fails, for both the Text path and `flush()`. Keep clearing it after an attempted transfer, as today.
- **Test to add:** `model_fail_open_at` on Close, then `IOResult`, then a second Close succeeds with exact finalized bytes.

**2. Medium — README Execute paragraph vs new behavior; `chain-files.c` edits hide the change.**
- **Contradiction:** README still says "An attempted OS replacement closes ordinary files but retains filename associations if it fails, so Reset can reopen them."
- **Actual behavior:** files now stay logically open (`OPENED`), so a Reset after a failed Execute returns I/O 0x20 from `activate`.
- **Test edits:** `execute_files()` and `faults()` were changed from `select()` to `named()`, a re-Assign that discards the open state. That edit exists precisely because Reset alone now fails.
- **Assessment:** keeping files open is arguably closer to CP/M TP3 and matches the spec note. However, the user-facing documentation is now wrong.
- **Fix:**
  - Update the README: after a failed Execute, files remain open. Continue using them or Close them; Reset requires Close or Assign first.
  - Add a host test that, after `END_EXECUTE`, continues to Write and then Close on the *same* FIB without re-Assign, with exact bytes.
- **Question:** what do `execute-retry.pas` and `execute-failure.pas` currently do? The RETAIN.DAT and RECOVER.DAT oracles suggest continued use, but their sources were not supplied.

**3. Low — close failure after a committed transfer.**
- **Trigger:** `disk_release` fails after Read, Write or BlockWrite succeeded.
- **Impact:** the operation reports 0xf0 or 0xff even though position, count, DIRTY and the destination bytes were already committed (the test asserts `actual==1` with error 0xf0).
  - Under `$I-`, a naive retry duplicates a Write or skips a record.
  - `flags()` is also skipped on error, so the TP3 flags byte at offset 0 goes stale after state changes.
- **Smallest fix:**
  - Document this as an adapter consequence.
  - Run `flags()` whenever the record is valid, regardless of `job->error`.

## Questions / deliberate adapter consequences

**4. Low — resume trusts the FIB path.**
- `bound()` checks only for a NUL terminator. A corrupted or forged FIB with `OPENED|WRITABLE` and a path such as `#SER0` (or any non-normalized name) is opened with O_RDWR or O_WRONLY. This bypasses Assign's `path_resolve`/logical-device split and `activate`'s directory stat.
- Raw memory, Port and inline already grant equal power, so this is not an escalation. However, the README's "no native authority" claim covers only the fd field.
- **Cheap guard:** in `disk_open`, require a normalized drive path (`path[1]==':'&&path[2]=='/'`). Otherwise, document the limitation.

**5. Low — Erase/Rename while another FIB has the path logically open.**
- With no fd held, nothing reflects the open state at the OS level. The other FIB's next access fails with I/O 01, or silently writes into a recreated file of the same name.
- TP3 warns never to Erase an open file, so this is probably acceptable. Please confirm and document it.

**6. Low — stale comments and docs.**
- `transfer.c` `transfer_cleanup`: "reports native descriptor failures".
- `runtime.asm` `file_cleanup`: "releases native handles".
- `chain-files.c` header comment.
- `test-files.py` report scope: "native terminal descriptor cleanup".
- `file-core.c`:
  - the `basic()` copy-Close comment;
  - the `lifetime()` comment "must still close its OS handle";
  - the `faults()` comment "Terminal cleanup releases descriptors".
- The spec note still says "verification in progress / Opus review pending".
- The `chunk-coverage.json` reason says assertion count is "retained" and also that 14 were removed. Call order is retained; the inner count changed.

## Missing proof

- **Text writer resume across `$A-` recursion:** an outer Rewrite, an inner reassign of the same static variable, then the outer continues writing and closes. `text_resume` and `diskscope.pas` cover readers only.
- **Close-path acquisition failure** (finding 1) and Close finalization after a failed Execute without re-Assign (finding 2).
- **Physical-cursor clamp:** resume where `physical` exceeds the current file size, for example when another FIB truncated the file. This currently reports 0x99 even for writes; is that intended?
- **Performance:** each Text Read/Write dispatch, including per-Char Read, now costs open+seek+close. The controlled native or hardware timing is still pending. The 4→261 open count is host-only, and the tradeoff is not yet quantified on TF.

No blocker- or high-severity faults found within the supplied diff and context.

### Codex reconciliation

1. Accepted. Close now acquires before attempting finalization and leaves the logical state/DIRTY/cursor intact if acquisition or resume seek fails. Host Close-retry tests fail that acquisition (including descriptor pressure) and then require exact typed count/header/data and Text Ctrl-Z after a second Close. After an attempted transfer, existing partial-I/O semantics remain; no rollback is invented.
2. Accepted. README now says failed Execute preserves logically open bindings; Reset requires Close/Assign first. The Chain-file owner test continues Write and finalizes the same FIB after END_EXECUTE, without named/re-Assign. The production execute-failure fixture now also leaves data open across the forced OS commit failure, appends another component after rollback and verifies both values and exact finalized bytes. execute-retry.pas and execute-failure.pas are supplied in follow-up.
3. Accepted. Flags refresh at validated state-owner exits, including error paths; native close failure happens afterward and cannot leave flags stale. This avoids a new repeated validation pass in zf_dispatch. Added successful-transfer/failed-close proof with actual1, advanced FilePos and DIRTY flags. README warns that a native-close error can follow a committed transfer and must not trigger blind replay.
4. Documented limitation, guard not added. Only the FIB fd word has no native authority. Path is ordinary Pascal memory; unchecked Mem/Move/absolute/Inline operations retain their existing power. The supported Assign path is normalized once. No new canonical-path revalidation or sandbox guarantee is introduced.
5. Accepted adapter limitation. Keep a logically open path stable; another binding's Erase/Rename/replacement can cause a later resume error or access the replacement. Added cursor-clamp test after another FIB truncates the path: IO99, no stale/silent seek and zero retained handles. No live-file identity registry/cache is introduced; no new claim about unsupported original open-file mutations is made.
6. Accepted. Corrected stale comments in transfer/runtime, file/Chain tests and report scope. Coverage metadata now distinguishes top-level call order from the14 retired error-location assertions and retains480 as the historical baseline.

Missing proof addressed: diskscope now includes recursive Text writers (separate paths, same local FIB address, outer continues Write after inner returns, then exact AB/EOF readback), in addition to readers. Native/host, real Execute failure and timing results will be supplied with the follow-up.

Performance adaptation: the first native128-write/128-read benchmark passed byte-for-byte but used8545921->10053706 HostFS cycles and28820178->91247355 TF cycles. Inspection of pinned ZealFS close shows write-capable handles write directory size metadata on close. Resume now opens O_RDONLY for reads and O_WRONLY for writes; Pascal access rights remain checked in the FIB. This removes unnecessary read-side metadata writes without caching, kernel changes or new persistent state. Final timing/verification is recorded in the evidence artifact. No whole-suite/hardware or original failed-Execute equivalence claim.

## Opus turn 20261104T114931-be008997

Mode: code; read-only

# Follow-up review: operation-scoped native disk handles (disk-handles-20261104)

I traced the final sources, diff and supplied evidence. I ran no tests; every pass count comes from the evidence JSON or the reconciliation notes. All six prior findings are adequately resolved. No blocker, high or medium faults remain. What is left is two low documentation items, one low question and four gaps in proof.

## Prior findings

1. **Close acquisition retry: resolved.**
   - `close_record` now finalizes only a writable Text file or a DIRTY typed file. If `disk_open(1)` fails, it returns before `state=0`.
   - A seek clamp inside `disk_open` takes the same path, and `disk_release` still closes the fd, so no handle leaks.
   - Untyped files and clean typed files clear state without acquiring a handle, which is correct.
   - The `close_retries` host tests check exact bytes after open failure and descriptor pressure. Native `diskscope` covers retry after a forced missing path.

2. **Failed Execute documentation and same-FIB proof: resolved.**
   - The README now matches the code.
   - `chain-files.c` `execute_files()` now asserts Reset returns 0x20, then continues Write and Close on the same FIB with exact counts.
   - `execute-failure.pas` and `execute-retry.pas` keep `data` open across real OS failures. Their oracles (`RECOVER.DAT`/`RETAIN.DAT` count 2) are consistent with the Pascal sources.

3. **Flags and committed-transfer close errors: resolved.**
   - `flags()` now runs at every validated state-owner exit, including after transfer failures. `disk_release` runs later and does not change state, so flags cannot go stale.
   - The test asserting `actual==1`, FilePos 1 and the DIRTY bit after a forced close error is correct.
   - The README states that a blind replay is not safe.

4. **Path trust: accepted as a documented limitation.**
   - The README now limits the "no native authority" claim to the fd field. This is consistent with the existing raw Mem/Inline powers.

5. **Shared-path mutation: accepted as a documented limitation.**
   - The clamp test is correct. A reader at cursor 3 sees the other FIB's Rewrite and Close leave a 1-byte file (Ctrl-Z), so the resume seek clamps and reports 0x99 with zero handles retained.

6. **Stale comments: resolved in the touched files.** See item 7 below for one remaining JSON file.

## O_RDONLY/O_WRONLY resume

The trace is correct.
- Every write path checks `access(1)` before acquiring a handle:
  - `component`, `blocks`, `text_write`;
  - `flush`, and `close_record` through its WRITABLE/DIRTY guard.
- Read paths acquire O_RDONLY only after `access(0)`.
- The read-only Reset fallback never reaches `disk_open(1)`.
- The `write_bytes` bookkeeping workaround only uses `seek`, which is valid on O_WRONLY handles.
- `model_writeonly` correctly fails any non-O_WRONLY open, which proves that write paths never request read access.
- The ZealFS close excerpt supports the rationale: only write-capable handles rewrite the size field.

## Remaining findings

**7. Low — stale status in `tests/native/tp3-fidelity.json` (documentation).**
- The `file_blocks.native_adaptation` text still says "A32-byte16-handle address ledger prevents closing borrowed OS descriptors".
- Its `change` text says terminal and Execute cleanup "raw-close OS descriptors".
- `opus_catchup_20261103.unresolved[0]` still lists "Repeated Assign without Close can exhaust16 native descriptors" as unresolved.
- The new `operation_scoped_handles_20261104` entry supersedes all three, but an unresolved list that still shows a resolved item is misleading.
- **Fix:** add a `resolved_by: operation_scoped_handles_20261104` annotation, or move the item into a resolved list. Keep the historical text.

**8. Low — README wording.**
- "The prototype resolves retained handles and keeps the existing banks" is ambiguous. Suggested wording: "removes retained native handles without adding banks".
- The pre-existing sentence "User break closes program files" is now only true in the sense that no handles are open. Consider: "User break performs no Pascal Close; no disk handles remain open."

**9. Question — Execute commit still maps the file module only for a no-op.**
- `transfer_dispatch` still calls `transfer_file_call(1,&executing)` for `ZF_END_EXECUTE`. That maps the file module and runs a cleanup that now does nothing except clear its fields.
- This is harmless, and the ABI is deliberately retained. Removing the call is optional and would change the transfer coordinator bytes, so it is not recommended in this milestone. I raise it only for awareness.

## Missing proof

- **Text formatted output cost.** The benchmark measures typed two-byte records only. `WriteLn(t,a,b,c)` issues one dispatch per field plus the newline, each doing open, seek, write and close. Per-char `Read(t,c)` loops are also one open per character. Text-heavy TF workloads are therefore likely worse than 2.95x. A Text variant of the timing source, for example 128 `WriteLn(t,i:6)` lines, would bound this.
- **Interior overwrite on real ZealFS (TF) through O_WRONLY resume.** Typed overwrite after Seek is covered on HostFS (`basic()`/`TYPED.DAT`), but the TF benchmark only appends.
  - Concern: close with a write handle rewrites the 16-bit size field.
  - Needed: a case of Seek(f,1), Write, Close on T: with an exact-byte oracle, proving the size and later components are unchanged.
- **Memory-mode (RAM Run) timing.** In this mode, opens and closes go through the parent `zf_service` gate. The two `DISK-SCOPE-OK` markers prove correctness, but there is no cost figure.
- **Coverage gaps the evidence already acknowledges.** No hardware stopwatch measurement and no full-plan run were done.

No actionable correctness regressions remain in the supplied product code or tests. The 2.95x TF cost is explicitly disclosed as an experimental tradeoff, and whether to accept it is the user's decision.

### Codex reconciliation

The follow-up reports no actionable correctness regressions in the supplied product code or tests. All six initial findings are resolved and verified; production source remains unchanged after the follow-up.

Accepted findings7–8: preserved the historical file-block account with an explicit supersession annotation, moved descriptor exhaustion from the old unresolved list to a subsequently-resolved record, and clarified README handle release and User break behavior. Finding9 is deferred: retain the existing END_EXECUTE ABI and coordinator call in this milestone.

Supplemental proof closes the three focused gaps: TF transfers pass51 assertions with exact typed interior-overwrite, block and wide-record bytes; Text TF COM costs31074087->93916761 cycles (3.02x); typed RAM Run costs8624949->24024436 HostFS cycles (2.79x) and28899206->99309093 TF cycles (3.44x). Every timing case verifies both markers and exact output. Captures and counts are in [the evidence](disk-handles-evidence-20261104.json).

The small-operation cost remains a material tradeoff for user review. No full-plan or physical hardware run is claimed. Keep logically open paths stable; no identity registry or descriptor cache is added. Original Random/typeahead research remains outside this milestone.

README/fidelity documentation and supplemental evidence changed after the follow-up. Their formal review fingerprints are therefore stale; no third Opus pass or unchanged-input finalization is claimed. The implementation remains uncommitted.
