# Review: tp3-catchup-runtime-20261103

## Opus turn 20261103T201108-252b7c3a

Mode: code; read-only

# Opus review: TP3 catch-up, runtime and language paths (013e79b → e9003b5)

I could not execute anything. This review is static reading of the supplied diff, current files and Codex's reported evidence. Every claim about runtime behaviour is reasoned from the code and still needs native confirmation.

There is one blocker (finding 1), plus two high-severity findings and several medium, low and question items below.

## Established defects

### 1. BLOCKER: capacity-sized string snapshots and Copy results can overrun the CPU stack

**Location:**
- `src/compiler_io.c` `cc_string_snapshot`
- Its callers: `field()`, `compiler_expr.c` `hold_left`, `compiler_strings.c` `string_value` and `cc_string_function` (CC_STR_COPY)

**What changed.** The temporary is now `CCT(type,width)`, which is capacity+1, instead of 256 bytes. The copy is still `LD C,(HL); LD B,0; INC BC; call RT_MOVE`. That copies the *actual* length byte+1, with no clamp. Likewise `RT_SUBSTRING` writes up to `source_len-pos+1` bytes into the new `bytes`-sized slot.

**Trigger.** Any string variable whose length byte exceeds its declared capacity, then used in Write, `+`, a comparison, Copy, Pos, Assign or Rename. Ordinary TP3 code can produce this:
- `var s:string[20]; ... FillChar(s,SizeOf(s),' '); writeln(s)` sets the length byte to 32.
- `s[0]:=#200` under the default `{$R-}`.
- BlockRead or Move into a string.
- An `absolute` string over arbitrary memory.
- A `{$V-}` VAR `string[10]` formal receiving a `string[255]` actual.

**Impact.** In the FillChar example, 33 bytes are copied into an 11-byte slot. That overwrites caller temporaries, saved IX and return addresses above SP. In Memory Run this can take down the shared IDE. Before this change, every snapshot slot was 256 bytes, which bounds any length byte, so this is a new memory-safety regression rather than TP3 behaviour.

**Smallest fix.** In `cc_string_snapshot` (and therefore every bounded caller), clamp before the move:

```
LD A,(HL); CP bytes; JR C,ok; LD A,bytes-1; ok: LD C,A; LD B,0; INC BC
```

Also store the clamped length into the destination's first byte after the move.

For Copy, pass `bytes-1` as an additional clip to RT_SUBSTRING. Alternatively, keep 256 for Copy whenever the source is not a fresh owned temporary.

The fixed-width 2/6/7/66 numeric snapshots and ParamStr's 128 are safe.

**Test to add.** Extend strlife with:
- a FillChar-overlong `string[5]`;
- `s[0]:=#200` under `$R-`;
- a `$V-` VAR formal.

Each should be used in Write, concatenation, Copy and Pos. Assert StackPtr is recovered and a canary local is intact.

### 2. HIGH: file, Aux and Chain/Execute services always fail inside an active `{$A-}` activation at the default RecurPtr split

**Location:**
- `runtime.asm` `check_boundary` (the new `recursion_ceiling` test)
- `compiler_files.c` `engine` (`guard(1024)` on the ready path)
- `cc_file_default_output` (`guard(1024)`)
- `compiler_chain.c` (`cc_reserve(1280)`)

**Reasoning.**
- `checked_guard` requires `callerSP-64-bytes >= ceiling`.
- At the first recursive save, `ceiling` is set to RecurPtr, which defaults to `SP0-1024`.
- Inside any recursive activation, `callerSP < SP0`.
- So `callerSP-1088 < ceiling` always holds, giving runtime error FF.

**Trigger.** `{$A-} procedure walk(n:integer); begin writeln(f,n); if n>0 then walk(n-1) end;` fails on its first write. The same applies to Assign, Close, Reset, `Writeln(Lst/Aux,...)` and Chain/Execute from within recursion.

**Impact.** TP3 permits this with the documented default split, and the baseline did as well, since frames lived on the CPU stack. `tp3-fidelity.json` concedes that the fixtures reposition RecurPtr first. That is a fixture workaround, not TP3 fidelity.

**Smallest compatible fix.** Do not count the C service stack against the Pascal split. The file gate (`file_gate.asm`, which already owns the mapping switch) could switch SP to a private stack inside the mapped module page (for example `0x7F00`) around `call 0x4000/0x4003`. Restore SP before remapping.

The file engine's call-site guard would then drop back to the old 64 bytes. The same applies to the transfer coordinator's C portion.

If the pinned PIO ISR does not touch page 1, this is safe. Codex should confirm that. The 1024-byte RecurPtr default is unchanged.

**Test to add.** A recursive Text/typed-file writer at default RecurPtr, in both RAM and COM.

### 3. HIGH (needs evidence): Execute of a non-Pascal native image leaks owned pages

**Location:** `chain.c` `execute_prepare`, `execute_override` and `execute_attempt`, together with `transfer.c` commit.

**What changed.**
- Preflight no longer rejects images where `handoff==0`.
- `execute_snapshot` allocates three backup pages.
- The retained coordinator page (`ZT_PAGE`) and file module page are passed as `owners`.
- When `handoff==0`, the handoff record that lists all of these pages is not written. The only code that ever frees them is the target's `execute_entry` release.

**Trigger.** `Execute` a plain native utility, or an older version-1 ZP80 image. A version-1 image fails `native_decode` and is treated as opaque.

**Impact.** Up to five 16 KiB pages stay owned, unless the OS reclaims child-owned pages at exit or exec. The `execute_entry` probe ("pfree returns 6 when not owned") implies ownership transfers to the child. Whether the OS frees those pages when the child terminates is not established in the supplied material.

**What I need.** Evidence of the pinned kernel's exec/exit page-reclamation behaviour.

**Fix options if the OS does not reclaim:**
- Before override, free everything not needed for rollback. That is difficult, because the backups are needed if exec fails.
- Or document and bound the leak.
- Or restore the preflight rejection for opaque images while keeping the extent-based acceptance only where release is provable.

**Test to add.** Repeated Execute of a non-Pascal native image, then compare page counts on return to the shell.

### 4. MEDIUM: TypeId truncated to `uint8_t` in the type-cast path

**Location:** `compiler_expr.c` `factor`, in the `S_TYPE` / predefined-type cast branch: `uint8_t negative; ... negative=cc_text_value(expression());`.

**Reasoning.** Type IDs are now 16-bit, up to `0x7FFC`. The expression's type ID is truncated modulo 256 before `cc_family(negative)` and `cc_ordinal(negative)`.

**Trigger.** A program with 256 or more types, followed by a cast such as `Integer(x)` where `x`'s type ID is at least 256. The result can be a spurious error 44/31, or acceptance of a non-ordinal operand (for example a record whose ID matches an ordinal ID modulo 256). Acceptance makes the cast operate on an address.

Set-expression kinds (`0x80xx`) are mangled the same way.

**Fix.** Declare a separate `TypeId cast_source`. `kind=...:(uint8_t)fn` has the same latent truncation for a lazily created Text ID, so make it `(TypeId)fn`.

**Test to add.** A capacity fixture with 300 or more types, followed by ordinal and record casts.

## Lower severity and optional items

### 5. LOW: the fixed-False CBreak omission can be bypassed without any trigger

**Location:** `compiler_console.c` `cc_console_finish`.

A forged pointer via a variant record, or an `{$R-}` out-of-range array store reaching `0x4152`, can set CBreak at runtime. In that case the omitted flow wrappers mean Ctrl-S/Ctrl-C are silently ignored. This is not a safety issue. Acceptable, but say so in the TP3-30 notes.

### 6. LOW: overlapping file and standard blocks across Chain leak descriptors

**Location:** `file_runtime.c` `standards()` across Chain.

If a CHN's standard-table address overlaps a caller global holding an open Text FIB, `initialize()` overwrites it. The fd stays in `handles[]` until terminal cleanup. This is consistent with TP3's "identical globals" rule. Note it only if the leak cap matters.

### 7. LOW: no upper-bound check on RecurPtr wraparound

**Location:** `compiler_recursion.c` save, with `check_boundary`.

If the user sets RecurPtr to less than the workspace size, `RecurPtr-bytes` wraps to `0xFFxx`. That passes the `>= heap_frontier` test, and the LDIR then writes the tail/history area and page 0.

This needs a user-assigned bogus RecurPtr. `Mem[]` already allows equivalent damage, so it is optional. One `JP C` after the `SBC` in save would close it.

## Questions (cannot be resolved from the supplied material)

1. **`cc_storage` placement.** It is now outside the supplied diff. Does it allocate recursive-scope locals statically, and stop adding them to `R_LOCALS`? `compiler_recursion.c` assumes static addresses, and `cc_slot`/`indirect_symbol` no longer support IX-relative locals. Double counting would only waste frame bytes; frame-relative addresses would be wrong code.
2. **Generic `K_STRING` (TypeId 1) width.** `string_value` and `cc_string_function` use `CCT(K_STRING,width)` for concatenation, ParamStr and Char-array results. This is only safe if that width is 0 (which falls back to 256) or 256.
3. **`Random(N)` for N ≤ 0.** The native code emits `RT_RANGE 1..32767`, which is a runtime error. The fidelity record pins the algorithm but not the original N ≤ 0 behaviour.
4. **`CHAIN_PROFILE_BYTES=4`.** Is the identifier derived from build content, or manually versioned? A stale or colliding value lets a CHN from a different runtime layout jump to wrong private helper offsets.

## Checked with no finding

- **Random step:** `129*state+$361962E9` carry chain, bounded result `(state shr 17) mod N`, zero handling and Real6 normalization, and Randomize high byte from R.
- **Recursion workspace:** save happens before value-parameter copy. Restore runs after result capture, including String and Real results. Exit joins before restore. The ceiling is reset at startup, CHN commit and the final restore.
- **Runtime scratch reuse:** `Pos` rewrite, `set_mask` (B/HL preservation), the `fd6x` string/heap scratch overlap, `fe85` text/set overlap, `heap_limit`, and `stack_set` carry handling.
- **Transfer ABI:** 26-byte TransferJob offsets, 154/156-byte frame unwind, 4-byte profile copy.
- **Cleanup ABI:** 6-byte FileCleanup push/pop order. The file gate always uses terminal mode, and every remaining `file_cleanup` caller is op 19.
- **FIB:** 176-byte layout asserts, and the `handles[]` ownership check that prevents closing or using borrowed or copied descriptors.
- **Images:** CHN/ZP80 version 2 with zero reserved slots, and the `execute_entry` zero-reserved check.

## Missing test coverage

1. Overlong-length string operands (finding 1).
2. File I/O and Chain inside recursion at the default RecurPtr (finding 2).
3. Repeated Execute of a non-Pascal or version-1 image with page accounting (finding 3).
4. 300+ type casts and set constructors (finding 4).
5. Native evidence that a failed Execute leaves typed/Text files reporting "not open" rather than corrupt.

## Scope limits

- Reviewed only the runtime/language production paths and the supplied current files.
- Storage, disk-emission, loader and banking owners (including `cc_storage`, `cc_reloc_*`, `cc_symbol_*` and `image_get` under disk emission) were not supplied and are not covered.
- Overlays remain deferred and were reviewed only for header compatibility.
- No whole-repository or test-suite audit is claimed.

### Codex reconciliation

1. Accepted blocker, adapted fix to preserve TP3 semantics: source String snapshots and Copy reserve256 bytes, preserving the actual length byte even for V- mismatches and unchecked overlong values. Clipping to declared capacity would change the accepted TP3 behavior. Proven numeric/Char-array/ParamStr bounds remain reduced. New native strbounds covers FillChar, length200, Copy/Pos/concat/Write and V- with a stack canary. Existing strlife gap thresholds changed to account for256+frame; exact restoration assertions remain. Native core2109/0 and error-location regressions pass.
2. Accepted high finding. File and transfer module entry wrappers save caller SP in module-owned data, run C at8000 using private tail stacks, restore SP before returning to retained gates. Packager reserves file7C00..8000 and transfer C7C00..8000 plus Execute alias7800..7C00. No extra page is allocated. Pinned PIO handler keeps page1 mapped. Transfer's nested six-byte FileCleanup is relayed through FE68..FE6D and a temporary FC80 stack, then copied back after coordinator mapping/SP restoration; it is only used by normal transfers, never Memory Run. Packager now bounds worker_end atFE68. Pascal call guards drop to actual64/256-byte gate/job frames. DiskLife/DiskWide no longer lower RecurPtr and produce correct RAM/COM output. The old file negative test assumed the removed C-stack requirement; headroom128 now exercises actual Pascal gate exhaustion, preserving assertion count/order and historical coverage ledger.
3. Rejected leak as disproven, no product change: pinned kernel zos_loader_exit calls zos_loader_free_user_pages, scanning every page owned by the terminating process, independent of private handoff. Supplied immutable kernel excerpts/hashes plus native raw four-byte and legacy-Pascal Execute tests cover exact page inventory and rollback. Final Execute report passes. This is OS cleanup, not a Pascal-entry assumption.
4. Accepted medium finding: cast_source is TypeId; target uses (TypeId)fn. Host300-type ordinal cast succeeds and record type ID256 is rejected44. Native300-type ordinal cast succeeds.
5. Accepted limitation/documented: normal CBreak/hooks/absolute/raw/Ptr/External/transfers retain wrappers. Arbitrary unchecked out-of-object stores reaching private runtime addresses are outside fixed-False omission proof. Do not reintroduce dynamic validation for arbitrary private-memory corruption.
6. Accepted TP3 identical-globals precondition: mismatched Chain globals can overwrite live FIBs; terminal bounded descriptor cleanup still owns closure. No modern registry is reintroduced.
7. Rejected as unreachable under existing guards: cc_recursion_save calls cc_guard before subtraction; check_boundary requires RecurPtr >= heap_frontier >= heap_floor >=8000. Workspace is bounded by actual static storage below8000. Thus RecurPtr < workspace cannot pass that first guard, and the following guard also checks the resulting frontier. No duplicate carry guard added.

Q1 resolved: compiler_build.c cc_storage allocates every Pascal local/value/result in static globals storage. Recursion scans those scoped symbols and copies static ranges; no R_LOCALS frame allocation is added there.
Q2 resolved: compiler_compile initializes type1/K_STRING width0; generic String results use fallback256. Source snapshots are now256 independently.
Q3 not a new compatibility claim: N<=0 keeps the existing emitted range check/runtime91; original nonpositive behavior remains unverified. No random semantic change was made by this review.
Q4 resolved: four-byte CHAIN_PROFILE is derived on the host from the exact runtime/private assets by package-native.py, not manually versioned or target-hashed. Supplied packager and packaging63-check evidence bind it to the artifacts. Four bytes are the existing ABI identifier, not a cryptographic collision guarantee.

Verification: sanitized host178035 compiler checks plus4010 editor/45809 source; native core2109/0, files483/0 and raw/legacy Execute/rollback pass. Current follow-up evidence includes final artifact and focused tests. No whole-repository, exhaustive recursion-depth or physical-hardware qualification is claimed. Only the stated original Random nonpositive behavior and deliberately unsupported unchecked-private-memory effects remain limitations before follow-up.

## Opus turn 20261103T212224-585ef1fe

Mode: code; read-only

# Follow-up review: TP3 catch-up, runtime and language scope (013e79b → working tree)

**Verdict:** I found no blockers within the supplied scope. All seven prior findings are resolved or acceptably dispositioned. One medium reset gap remains, plus two low items and one question. The storage cross-boundary fixes check out.

**Limitations:**
- I could not execute anything. All test results cited here are Codex's recorded evidence, not mine.
- I reviewed only the supplied diff, current files, the kernel excerpts and the storage report.

## Dispositions of prior findings

| # | Disposition | Assessment |
|---|---|---|
| 1. Overlong String snapshots/Copy | 256-byte source and Copy slots | **Resolved.** `cc_string_snapshot` and Copy are bounded: snapshot copies length+1 ≤ 256, and `RT_SUBSTRING` writes at most `len-pos+2` ≤ 256. Concat is bounded by `hold_string` and its carry check. Char-array (width+1 ≤ 256), numeric (2/6/7/66) and ParamStr (128) slots stay bounded. Keeping actual lengths rather than clipping is the right TP3 call. `strbounds.pas` covers FillChar, length 200, a `$V-` formal and a canary. |
| 2. Services inside `{$A-}` at default RecurPtr | Module-tail C stacks plus relay | **Resolved.** Details below. |
| 3. Execute page leak | Rejected, citing kernel evidence | **Accepted.** `zos_loader_exit → zos_loader_free_user_pages` scans every page owned by the exiting process. Override keeps ownership with that process, and raw and legacy native Execute inventories are covered. |
| 4. TypeId truncation in casts | `TypeId cast_source`; `(TypeId)fn` | **Resolved.** The 300-type host and native cases cover it. The remaining `kind=(uint8_t)fn` after `cc_file_function` only ever yields K_INTEGER or K_BOOLEAN, so it is harmless. |
| 5–6 | Documented limitations | **Accepted.** |
| 7. RecurPtr wrap | Rejected as unreachable | **Accepted.** The leading `cc_guard()` → `check_boundary` requires `RecurPtr ≥ heap_frontier ≥ 0x8000`, and the static workspace is smaller than that. |
| Q1, Q2, Q4 | Resolved | **Agree.** |
| Q3 (Random N≤0) | Still unverified | **Acknowledged.** |

### Specific checks for this follow-up

**Cross-bank caller stack and job lifetime:**
- File C now runs at `SP=0x8000` inside the mapped module page. The packager bounds HEADER through BSS below 0x7C00, so the stack has 1 KiB, the same as the old 1024-byte budget. The module page is zero-filled before unpack.
- `FileJob` and the file objects stay on the Pascal stack or in globals (page 2/3), which remain mapped. The saved SP lives in module `_DATA`, which is not reentered because stream callbacks run only after the prefix is restored.
- The transfer relay is correct:
  - It saves the 6-byte job to FE68..FE6D, below `worker_end ≤ FE68` and outside the FE70 control block.
  - It switches SP to FC80. The relay frame occupies about FC6E..FC7F, below the transfer gate and above the FC1E PC slot that `cc_error_finish` reads later.
  - The gate restores F1 from `0xfe76` (the transfer page) before returning, then the relay restores SP and copies the job back.
  - Transfers are refused in Memory mode, so FC00..FC7F is never a RAM-entry context while this runs.
- The Execute alias stack (module 7800..7BFF, seen as F800..FBFF under F3) is disjoint from the C stack at 7C00..7FFF.

**Guards:**
- The file engine now uses 64+64, Aux default output 64, and transfer 256 plus the ~158-byte frame.
- The transfer preflight `heap_frontier ≤ SP-256` matches that.
- `transfer_install` sets budget 64 and RecurPtr to `end+1-1024`, and the commit clears the ceiling.

**IRQ preservation:** The pinned PIO handler pushes onto the live stack and remaps only page 3. That is safe whether the live stack is in module page 1 (7Cxx), FC70 in page 3, or the F3 alias.

**Memory-run reset:** Startup clears `io_status`, error code and namespace, the input block, key state and the ceiling. It re-derives the heap, RecurPtr and FE70 control.
- The `input_reset_count` arithmetic zeroes `input_state_begin..end-1` inclusive and then re-sets `echo_fd`.
- Stale FIBs (OPENED, but `handles[fd]≠object` in the fresh module) correctly degrade to not-open.
- Exception: see finding 1.

## Remaining findings

### 1. MEDIUM — `io_frame` survives across Memory Runs

**Where:** `src/runtime.asm` `start:`; `io_frame` at the end of the jump table.

**Trigger:**
1. Run 1 ends with a checked I/O error raised inside `io_call`, for example a failing `Writeln` under `{$I+}`. `io_check` → `call_error` abandons the frame without `io_return`, so `io_frame` keeps that run's stack address.
2. Memory Run now reuses the same Build pages, so run 2 starts with the stale value.
3. Inside run 2, any `RT_IOFAIL` or `io_check` taken outside an `RT_IOCALL` wrapper reads the stale frame. If the stale checking byte is 0, it executes `ld sp,hl; jr io_return` into dead stack, then returns to a garbage address. A crash in Memory Run shares the IDE session.

**Reachability:** The candidate path is the hook `invocation()` tail. Its `pending()` runs after its own `io_return`, in the caller's context. For ConStPtr through KeyPressed, the only caller-side wrapper is `cc_keypressed`, which was not supplied, so I cannot confirm whether that context is itself inside an `RT_IOCALL`. On the first run after compilation the value is 0, so the fault appears only on a repeated run.

**Why it is a defect:** `io_frame` is runtime-owned state, which is exactly the category this reset was meant to cover.

**Fix:** One store at `start:`, next to the ceiling clear: `ld hl,#0` / `ld (recursion_ceiling),hl` / `ld (io_frame),hl`.

**Test:** Run 1 ends in a checked I/O error inside Write. Run 2 installs a ConStPtr callback that leaves an `{$I-}` status pending, then calls KeyPressed. Assert a normal I/O error report or an IOResult, not a crash.

### 2. LOW — Assign on an open file strands its descriptor until terminal cleanup

**Where:** `src/file_runtime.c` `dispatch`, `ZF_ASSIGN` branch.

**Problem:** `initialize()` sets `fd=0xffff` but leaves `handles[old fd]=object`.

**Trigger:** `Rewrite(f); Assign(f,'other'); Rewrite(f)` repeated about 16 times. Every handle stays leaked until program end, after which `open_path` rejects new opens.

**Fix:** In the Assign branch, if `valid&&(state&OPENED)`, raw-`close_file(fd)` without Pascal finalization. That matches TP3 losing unflushed data rather than reverting to the old error 0x20.

### 3. LOW (documentation) — Stale `tp3-fidelity.json` text

- **`stages.strings`:** still describes capacity-sized snapshot/Copy slots and a "<128 measured stack" regression. Both are reverted: the slots are 256 bytes and `strlife` now allows 384.
- **`stages.recursion.native_stack_services`:** still says C services reserve an extra 1024 bytes and that recursive fixtures reposition RecurPtr. That contradicts the module-tail stacks, and `native_adaptation` in the same stage is already correct.

## Question

**Q1. Do Random state and generated typeahead persist across Memory Runs, and does TP3 do the same?**
- `random_state`, and apparently the keyboard queue and status at `cc.keyboard_state`, live in the relocatable program image. `start:` does not reset them, so a second Memory Run continues the Random sequence and sees leftover queued keys.
- If original TP3 re-zeroes its seed at runtime initialization (the fidelity record says "state starts 0"), a second run without Randomize will diverge from TP3.
- I need a two-run original capture of `Random` without Randomize.
- The typeahead queue is runtime input state and arguably belongs in the reset set. Please confirm where `cc_keyboard_engine` initializes it; that file was not supplied.

## Storage cross-boundary fixes

All three verified:
- F captures `native_error` straight after `compile_project`, before `build_close` and `source_discard`.
- `scratch_conflict` is reachable only from Compile and F; `lookup_failure` no longer prints it.
- `global_space` reports `cc_fail(99)` regardless of polling, and `cc_place_code` returns as soon as placement sets an error, before any prefix or hook writes.

## Missing coverage

- A two-run Memory test with a checked I/O error in run 1 (finding 1).
- Random sequence and typeahead across two Memory Runs, compared against the original (Q1).
- Repeated Assign/Rewrite without Close beyond 16 iterations (finding 2).
- A recursive file I/O depth sweep near the default split, to bound remaining Pascal-side headroom. This is regression evidence only, not a requirement.

## Scope limits

- I did not see `cc_keypressed`, `cc_keyboard_engine`, the worker gate at FDA0, the transfer gate source, the kernel syscall dispatcher, or the storage/loader owners.
- The page-1 stack-safety conclusions during syscalls and the RAM-mode worker service rely on the existing native RAM/COM file evidence (files 483/0).
- Overlays remain deferred.
- No whole-repository, exhaustive stack-depth or physical-hardware claim is made.

### Codex reconciliation

1. Accepted medium reset gap: start now clears io_frame with the same zero word already used for recursion_ceiling. A new two-run native fixture first raises checked Writeln failure, then requires both io_frame bytes zero before any second-run I/O and emits IO-FRAME-RESET-OK. The frame symbol is exported to the host linker map for the fixture; no Pascal API is added. Focused verification is recorded in reviews/tp3-catchup-verification.txt.
2. Confirmed native descriptor pressure, but rejected the proposed raw-close implementation as incompatible. The candidate caused existing file-core.c lifetime assertions138/139 to fail: a recursive/static workspace saves an outer FIB, inner Assign reuses that exact address, then restoring the outer FIB must still access its original handle. Raw-closing on Assign destroys it. The candidate and its extra tests were removed; the unchanged file-runtime host suite then passed1103/0. Existing native DiskLife verifies the same-address recursive file case. Assign must continue to abandon the current FIB without closing potentially saved outer associations. Terminal cleanup bounds the native resource retention to16 descriptors. Repeated Assign without Close can therefore exhaust Zeal handles; resolving this while preserving TP3 needs a separately researched descriptor-lifetime design. No frame/generation registry is reintroduced without necessity review. This low-severity platform limitation remains unresolved and is reported to the user.
3. Accepted documentation correction: strings now describes256-byte source/Copy slots and the384-byte strlife bound; recursion.native_stack_services describes private module stacks and the unmodified default split.
Q1 traced: cc_keyboard_engine embeds a258-byte count/head/queue plus status in persistent generated code, with no per-Run clear; cc_keypressed DOES wrap its poll in RT_IOCALL. Random's four-byte state is also persistent. The earlier phrase state starts0 describes a newly compiled image, not verified repeated-run initialization. No original two-run Random/typeahead evidence is available in this review. These remain explicit compatibility research questions; no speculative resets are introduced.

Final source reset/docs/test edits were made after the second runtime turn, so this session's input fingerprint is stale and is not finalized/reused. No third review is requested or claimed. Major prior faults and final storage fixes received the follow-up; the final three-byte io_frame store receives focused native and packaging proof. See the verification record for final results. No complete TP3 equivalence claim is made.
