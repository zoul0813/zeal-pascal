# Review: no-z80-hashes-v1

## Opus turn 20261101T040934-424896da

Mode: code; read-only

# Review: removal of Z80-side hashing (no-z80-hashes-v1)

**Verdict:** one blocker in the transfer caller's stack frame, plus two real compatibility regressions. I ran nothing, and none of the supplied test results cover the defective path.

## Established defects

### 1. BLOCKER — `src/transfer_call.asm`, `finish:` epilogue uses the old frame size

- **Trigger:** every path that reaches `finish`. That includes:
  - path I/O failure (`rel_path`);
  - `palloc` failure (`rel_alloc`);
  - prepare/commit errors (`finish_job`);
  - input-cleanup failure followed by discard.
- **Evidence:** the prologue now reserves `-152` (24-byte job + 128-byte path), and the reload at `module_ready` uses `#152`. The epilogue still does `ld hl,#184 / add hl,sp / ld sp,hl / pop iy`. 184 was the old 182-byte frame plus the pushed HL. The new value must be `152+2 = 154`.
- **Impact:** SP ends up 30 bytes too high. `pop iy` restores garbage into the caller's With register, and the following `ret` or `jp 0x415e`/`0x4161` runs on a corrupted stack. Every failed Chain/Execute transfer would crash the generated program instead of reporting the I/O error.
- **Coverage gap:** `transfer-host.c` and `transfer-driver.asm` drive `transfer_dispatch` directly, not this caller. That is why the reported host passes do not exercise it.
- **Fix:** change `ld hl,#184` to `ld hl,#154`. Better, define the frame size as one `.equ` (`ZT_FRAME = 24+128`) and use it in all three places.
- **Verification:** add a native case that makes Chain fail after `native:`, for example a missing CHN or a rejected profile. It should assert that the program receives the I/O error and continues with its With pointer intact.

### 2. HIGH — Chain private-ABI profile is now a hand-maintained constant (`tools/package-native.py: runtime_profile`, `CHAIN_PROFILE_BYTES 2`)

- **Change:** the profile used to be derived from the exact private runtime, file and transfer assets. Now `runtime_profile()` returns `struct.pack('<H',2)` for every build.
- **Impact:** a CHN built by a different compiler build at the same format version is accepted by `chain_prepare`. This happens whenever `pascal_runtime.private` addresses, the file-runtime protocol or the transfer protocol differ between the two builds. The CHN then jumps to stale private helper addresses inside the caller's runtime and crashes.
  - This is a real cross-build risk during development and between releases.
  - Nothing enforces "bump when contracts change". `package()` only checks that the profile equals the constant.
- **Fix (no Z80 hashing needed):** have the host packager derive the tag deterministically from the assets it already reads, and keep the Z80 side as a plain byte compare. Two options:
  - **Raw descriptor:** make the profile the raw private address table (`pascal_runtime.private`, 48 bytes) plus the protocol version bytes. This involves no hash at all.
  - **Host-derived tag:** a host-computed build tag. This falls under the allowed host build provenance.

  If the user rejects any host-derived tag, at minimum fail the build when `pascal_runtime.private` changes without a profile bump, for example by comparing against a checked-in expected table.

### 3. HIGH — Launcher/library pairing is no longer enforced (`src/library.c: code_load`, `src/startup.asm`, `package-native.py`)

- **Change:** `code_digest` previously bound `zpascal.bin` to its exact `zpascal.zpl`. Now any v2 ZPL is accepted if it has the same bank count, consistent internal lengths and EOF.
- **Impact:** a BIN and ZPL from two different builds with the same bank count will load, and the launcher will far-call into mismatched bank code.
  - The README now only says "Install the BIN and ZPL from the same build".
  - The `mixed-build` and `metadata-digest` negative cases were deleted from `tools/test-native.py` rather than replaced.
- **Fix (structural, no hashing):** have the packager fill, alongside `_code_required`, the expected `code_sizes[]` taken from the `l__BANKn` link sizes. `code_load` then compares each declared size.
  - Optionally also store a host-generated build tag in ZPL header bytes 12..15, which are currently required zero, and in resident HOME, and compare them.
  - Restore a mixed-build negative test.

### 4. LOW — Packed-catalog boundary coverage lost (`tests/native/core-host.c: packed_catalog_cases`)

- **Change:** the old lengths `PAGE_SIZE-190` and `PAGE_SIZE-189` sat on either side of the 189-byte record boundary. The new pair is `PAGE_SIZE-190` and `PAGE_SIZE-157`, so the "just fits" case `PAGE_SIZE-158` is no longer tested.
- **Fix:** use `PAGE_SIZE-RECORD_BYTES-1` and `PAGE_SIZE-RECORD_BYTES`.

### 5. LOW — Deleted rather than updated assertions

- `core-host.c` removed `check(initialization_polls==(mode==2?268:322))`. This was cancellation/poll-cadence coverage. Pin the new counts instead of dropping the check.
- `tools/test-ide-sources.py`: the check named "compiled Main retains exact unsaved root hash" now only tests `build+4==1`. That proves nothing about the snapshot. Use the `build+15`/`work+8` revision comparison used elsewhere, or export the snapshot bytes for a host comparison.
- Several check names still say "authenticate" or "hash" (`test-chain.py`, `test-ide-sources.py`, `test-members.py`). This is cosmetic, but misleading in reports.

### 6. LOW — The no-SHA proof is incomplete (`tools/test-no-sha.py`)

- **Gaps in the current check:**
  - It inspects only 8 targets. It misses `zcorpus`, `zoverlaytest`, `zexecuteprobe`, `zedittest`, every `z*test` target and `file_runtime`.
  - The prefix list would not catch `_transform`, `_test_source_hash`, `_test_image_hash` or `_sha256`-style names.
- **Fix:** iterate over every `build/*.map` produced by `zde cmake` and reject `_sha_`, `sha_rr`, `_transform`, `hash` symbols, or objects from `tests/host/`.

## Questions / needs confirmation

### 7. Is the ZMAP header page zero-initialised at offsets 20..83?

- **Why it matters:** `cc_seal` no longer writes those bytes; previously it overwrote them with digests. `compiler_result_ready` and `saved_map` now require them to be zero.
- **Risk:** if the map page comes from `page_new` without a clear, it is fail-safe but every compile fails. Host models typically hand out zeroed memory and would mask this.
- **Suggested fix:** an explicit `for(i=0;i<96;++i)map8(i,0);` at the start of `cc_seal`. The same reasoning applies to ZP80 bytes 36..131, which are currently zero only because of the template's `.ds 96`.

### 8. Is `member_byte` pure and idempotent? (`src/compiler_members.c`)

- **Change:** the read-back loop now calls `member_byte(...,late,...)` a second time, interleaved with `output_read_buffer(store->fd,...)`.
- **Risk:** if `member_byte` touches the late journal, performs I/O on `store->fd`, or can set `late->failed`, the second pass can desynchronise. `late->failed` is not re-checked after the verify loop.
- **Request:** please confirm, or add the check.

### 9. Files not supplied, so remaining `test_hash` or 32-byte profile references cannot be ruled out

- `tests/native/harness.c`
- `src/transfer.c`
- `src/execute_handoff.h`
- `tests/native/editor-main.c`
- `tools/test-options.py`

### 10. `options-main.c` hard-codes `trampoline_copy[358]`

- The startup `memcpy` copies 358 bytes before the size check runs, so a shorter trampoline over-reads RODATA.
- **Suggested fix:** check `trampoline_end-trampoline_start` before copying.

## Checked and consistent

- `TransferJob` field offsets:
  - in `transfer_call.asm`: 16 (profile), 18 (path), 20 (error), 22 (detail);
  - in `transfer-driver.asm`: `0xf610`/`0xf612`/`0xf614`.
- `Build` layout as 51 bytes, with revision at +15 and `sources` at +39.
- Source record offsets (157 bytes) and `SOURCE_COUNT` usage.
- `saved_map` `RECORD=136` path/offset layout and NUL storage.
- `MEMBER_BYTES=24` with `M_OFFSET` ending at 24.
- The `execute_entry` reserved-zero loop over `0xfc0a..0xfc29`, matching the zeroed record.
- Overlay index reserved range 20..95 and the `files*9` minimum.
- PC-range validation added to `compiler_result_ready`.
- `code_capture` propagates open/write/close failures and uses stack buffers, not the 8000–BFFF window.

## Limitations

- I executed nothing, and the ZDE build and native results are not yet available.
- Finding 1 should be confirmed by a native transfer-failure run.

### Codex reconciliation

Accepted1: fixed the unwind defect and consolidated job/frame/unwind constants. TransferJob is now26 bytes (host-derived ABI identifier4 bytes); frame154 and unwind156. Existing tests/native/chain-call.pas already exercises missing/invalid CHN retries inside With and verifies marker42/calls2 before successful handoff. Run its native acceptance after rebuilding.

Accepted2/3 with the host-generated tag option: host packaging derives four-byte ABI/build identifiers from the exact assets and stores them in both producer/consumer headers. Z80 only compares those four bytes. Library also compares every bank length to the linked expected code_sizes table. Restored mixed-build and balanced-length-mutation negative tests. No hash algorithm is linked into ANY Z80 target.

Accepted4: added the new just-fits source catalogue boundary (PAGE_SIZE-158 vs PAGE_SIZE-157). Accepted5: restored exact observed initialization poll counts250/261; source observer now compares compiled Main revision with the restored dirty Main revision, rather than testing valid alone. Header/model check names will be made explicit in the final evidence report. Accepted6: no-SHA proof now inspects every top-level build/*.map for algorithm symbols/modules and test-only SHA symbols/host object paths.

Clarification7: compiler_compile.c explicitly clears the first96 map bytes before parsing; runtime.asm deliberately reserves96 zero bytes in every template. Host models allocate poisoned pages and the passing core suite exercises this. No redundant clear is needed.

Accepted8: member_byte only reads immutable page bytes and cc_late_byte, with no syscall/cursor effect; added late->failed check after each read-back buffer as well as the existing final cc_late_range check. Accepted9: additional surrounding files will be supplied in the follow-up. Accepted10: check trampoline extent before the snapshot memcpy.

Current checks after removing native SHA: host front-end74,788 assertions; source-validation45,809; key202;23 runner tests and6 chunk tests pass. Publication/fault checks1,208,125; image staging1,228,982 plus44,577 decoder comparisons; saved-map cases and overlay30,411,570 assertions pass. Final ZDE rebuild and native acceptance/timing remain pending. No full-suite pass is claimed yet.
