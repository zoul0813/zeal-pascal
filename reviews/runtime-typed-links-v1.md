# Review: runtime-typed-links-v1

## Opus turn 20261107T165129-9f30195b

Mode: code; read-only

# Review: runtime-typed-links-v1 (baseline ed379d1)

The fragment format, validation and both final-placement paths look internally consistent. The carry logic is correct in each place:

- **Fragment linker:** `emit` extracts the byte after adding target and addend.
- **Final placement, high8:** `cc_place_code` adds `coefficient*delta` to the full stored target before taking the high byte.
- **Final placement, low8:** it uses the staged code byte, which is valid because a low byte does not depend on carry.

Validation also covers unreachable records in `validate`, the reserved bits and kind 3, width-aware overlap and end checks, and roots carrying `0x4000`.

I have no blocker findings. The material risks are in ledger consumers and capacity outside this diff, and in how well the tests prove the carry case.

## Established defects

1. **Medium — `tests/native/units-main.c` `byte_relocations`, and the `tools/test-units.py` `byte_unit` fixture: the test does not guarantee a high-byte carry happens.**
   - **Trigger and impact:** The only guard is `b.code_origin>=0x8100u`. A carry during final placement needs `(staged_target & 0xff) + (delta & 0xff) > 0xff`. If `delta`'s low byte is 0, or the staged data address has a small low byte, there is no carry. The test would then pass even if the full-target path were wrong. `UNIT-BYTES-OK` is therefore weaker evidence than the check name ("survive final placement") claims.
   - **Evidence:** Nothing pins the staged address of the ByteUnit data fragment. The 257-byte `Pad` affects globals, not necessarily `origin - CC_CODE_BASE`'s low byte. The host fragment test does force carries (origins 0x80fa/0x80fc, value 0x81fe→0x8200), but only in the direct linker. It does not cover the deferred `cc_place_code` path.
   - **Fix:** After compiling, compute the staged target (code origin plus program code size plus 9, or read it from the ledger) and assert the carry condition. Alternatively, sweep a few `Start` origins or `Pad` sizes so at least one case is guaranteed to carry. Ideally, add a host test of `cc_place_code` with a high8 record whose `target=0x80ff` and `delta=0x0101`.

2. **Low — `compiler_units_object.c` `cc_units_write_object`: the writer can overflow the 12-bit fragment field.**
   - **Trigger and impact:** The writer ORs `owner_id|patch_kind<<14` without checking `owner_id<=CF_FRAGMENT_MASK`. If `owner_id` had bit 14 or 15 set, a word reference would be silently reread as a typed byte reference to fragment `owner_id&0xfff`. That would be a miscompile, not a rejection. Bits 12–13 would be caught by the reserved check.
   - **Likelihood:** Low, since 4096 or more fragments is unlikely under current memory limits.
   - **Fix:** Fail with `UNIT_FORMAT` or 99 if `owner_id>CF_FRAGMENT_MASK` before encoding.

## Questions and requested context (material, not verifiable here)

3. **Medium/possible high — other ledger consumers.**
   - `compiler_members.c` `member_byte` streams `relocs*RELOCATION_BYTES` raw ledger bytes into ZPMS members. Their header version is still `header[4]=2`, so the on-disk member record changes from 4 to 8 bytes with no version change.
   - I cannot see the ZPMS reader, the overlay/group relocation code, or `cc_disk_place`/disk-image relocation. Please supply them, along with any code that walks `cc_reloc_get` with a stride of 4 or with hardcoded offsets.
   - Two things to confirm:
     - (a) Every consumer uses `RELOCATION_BYTES` and the new byte-flag/kind invariant.
     - (b) Any rebase applied to a member also rebases the stored high8 `target`. If it only adds to code bytes, carry is lost again.
   - If high8 records can never reach members (unit linking never runs under the member/disk path), say so and enforce it.

4. **Medium — relocation capacity halves.**
   - `relocate` still rejects at `p+RELOCATION_BYTES>SOURCE_LIMIT`, so the maximum number of relocations drops by half. The code span is roughly 0x7400 bytes. At one word relocation per 3–4 code bytes, that is about 7–10K records, which at 8 bytes each could exceed `SOURCE_LIMIT`. Programs that compiled before could now fail with error 99.
   - Please give `SOURCE_LIMIT`, how ledger pages are allocated (lazily or not), and the worst-case relocations observed on stock units and demos.
   - Please also confirm that peak and cold page use stays within the 10-page budget when the ledger is near full. The supplied cold-startup observation does not cover peak ledger growth.

5. **Question — does `cc_units_link_object` ever run in `unit_mode`?** This determines whether `patch_kind!=0` ever reaches the writer from real compilation. If it does not, the writer's typed serialization is exercised only by the host cursor test (`tests/host/unit-references.c`). In that case, the "author serialization retains typed targets" claim is untested end to end. The native ByteUnit artifact is hand-built in Python and only exercises the reader.

## Regression checks that look correct

- **Inline byte operands:** these become kind 1 with target 0. Final placement is unchanged, and unit mode still yields `UNIT_UNSUPPORTED` because of the `kind&&!target` check.
- **External supersede:** a superseding external now requires a word16 ledger record.
- **`cc_original_entry`:** it copies kind and target.
- **Host expectation change:** byte flag set with kind 0 now gives `UNIT_FORMAT`; that change is consistent.

## Limitations

- No tools were available, and I have no test evidence beyond what was supplied.
- Native fragment and unit runs, saved execution after removing `BYTEUNIT.ZPU`, Inline behaviour and cold-startup results are pending and not treated as evidence.
- I did not see the member, disk or overlay readers, the definitions of `SOURCE_LIMIT` and the ledger allocator, or `cc_unit_fragment`.

### Codex reconciliation

Adapted scope after tracing current ledger consumers and native build limits.
The blanket 8-byte staging ledger design was removed, including all word/byte
compiler emission and unit authoring changes. Production uses its original
4-byte ledger, with unchanged capacity, table allocation, member format,
Inline semantics and disk relocation. This avoids halving reference capacity
or growing every program's compiler workspace before integrated placement.

Finding 1: deferred the proposed high8 final-placement path and its test
because that path is no longer implemented. Direct fragment linking is covered
by explicit 81FE/8200 carry cases, full word versus byte width checks, last-byte
sites, malformed kinds, reserved bits, overlap and raw root rejection.
Finding 2: typed unit serialization was removed; the original writer remains.
The unit reader now explicitly rejects byte references before placement instead
of coercing them into the word-only final staging ledger. Native integration
and authoring will be implemented with the runtime placement model.

Questions 3–5: no ledger format or member/disk behavior changed in final scope.
No author serialization of byte references is claimed. The catalog-helper bank move was rejected by the dedicated Real asset
validator and reverted. A smaller equivalent check uses p<size before p+1,
then requires another byte for word patches; this avoids 32-bit width addition
without weakening bounds. Original bank placement is preserved. Bank16 now
fits at16,382 bytes; cold startup count must remain10.
Final production build passes all packaging validators with original bank
placement. Refreshed direct-link tests pass952 native and1013 host assertions,
including source-valid root-flag rejection. All66 IDE unit checks pass:10 cold
pages,15 warm checkpoints, author/import/initialization/Real paths, runtime
errors, source-free saved clients and T/B publication. Original unit-ledger
host checks pass1824 assertions. A new native test uses the valid Calc artifact
as control, changes only one relocation kind and checksum, and proves both
low8/high8 variants fail publication and release transient/pending owners:
all13 checks pass. Evidence and hashes are supplied for follow-up.

Residual: bank16 has only2 bytes free. This is a known layout constraint for
subsequent runtime integration; this checkpoint is within verified bounds.
No physical program RAM or empty-Real reservation savings are claimed.

## Opus turn 20261107T173610-bcae229d

Mode: code; read-only

# Follow-up review: runtime-typed-links-v1 (final narrowed scope)

## Disposition of prior findings

| Prior item | Status |
|---|---|
| 1. Deferred high8 carry not proven | **Resolved by removal.** The `cc_place_code` high8 path no longer exists. Direct-linker carry is tested at origins 0x80fa and 0x80fc (value 0x81fe→0x8200), checking both low and high bytes. |
| 2. Writer could overflow the 12-bit fragment field | **Resolved by removal.** The writer is back to emitting `owner_id` only, and the reader rejects any upper bits. |
| 3. Member/disk ledger consumers | **Resolved.** `RELOCATION_BYTES` is 4 again, and `compiler_members.c`, `compiler_build.c`, `compiler_front.c` and `compiler_unit_references.c` match the original code. |
| 4. Relocation capacity halved | **Resolved.** The original 4-byte ledger and its capacity are restored. |
| 5. Typed author serialization untested | **Resolved.** Typed author serialization is no longer claimed. |

## Narrowed change: assessment

**`compiler_fragments.c` `validate`** is correct.
- It rejects records with the reserved bits (12–13) set, or with kind 3 (bits 14–15 both set).
- The bounds check is `p<size`, then `previous=p+1`. A word patch additionally requires `p+1<size`, so it cannot overflow 16 bits.
- Overlap and ascending checks use the actual patch width.
- Coverage:
  - A word patch at `size-1` is rejected by the existing `negative()` case at offset 48 with value 4.
  - A low8 patch at the last byte is accepted (`byte_references` ref 3).
  - A low8 patch one past the end is rejected.
  - A word patch overlapping a byte patch is rejected.

**`target()`** masks the fragment index before its bounds check, and `emit` extracts the byte after adding target and addend. Roots stay unmasked, so a root of 0x4000 is rejected. The test now applies this to a source-valid object, which closes the earlier ambiguity.

**`cc_unit_validate_object`** rejects any `target_fragment & ~0x0fff` for every record, including unreachable ones. Because it runs at load, `append_metadata` and `pc_sites_valid` can safely keep their unmasked, word-only logic.

## Remaining findings (all low, non-blocking)

1. **Low — the rejection test does not assert which error it hit.** `tools/test-typed-unit-rejection.py` checks only that no `CHECK.COM` is produced and that owners are released, not the diagnostic code.
   - The word control with an identically recomputed checksum makes it very likely the kind bits caused the failure.
   - Fix: record `_unit_diagnostic` or the displayed error and assert it is `UNIT_FORMAT`. This would stop a future unrelated load failure from passing silently.

2. **Low — the error for nonzero upper fragment bits has changed.** It is now `CF_OBJECT` (reserved or kind 3) or `CF_TARGET` only after masking, where it was always `CF_TARGET` before. The Pascal path rejects these the same way at the unit level, but any internal caller that checks for a specific `CF_*` value should be audited.

3. **Low, residual risk — bank16 has 2 bytes free (16,382 of 16,384).**
   - This is within bounds and the packaging validators pass.
   - The next change to the fragment linker, the unit loader, or the SDCC version will very likely force layout work.
   - Record this as a gate before integrating typed placement.

4. **Verification note — other producers of ZPF data.** The new reserved-bit rejection also applies to:
   - `CRT.ZPU` (from `tools/package-screen.py`);
   - the build-time runtime-object converter;
   - any other generator of ZPF data.

   Any producer that sets bits 12–15 will now be rejected.
   - The 66 IDE checks cover authored units.
   - Rebuilt shipped units (CRT and the screen library) are not listed in the supplied evidence; please confirm they load.

## Conclusion

There are no actionable correctness, security or regression defects in the final scope. Typed byte references are confined to the standalone fragment linker, where validation and byte extraction are consistent. The Pascal importer rejects them before its word-only ledger, so none are coerced into word patches.

## Limitations

I have no tools. The pass counts (1013 host, 952 native, 66 IDE, 13 rejection, 1824 ledger checks), the build and packaging results, and the bank sizes come from the supplied evidence JSON and are not independently reproduced. I did not see `package-screen.py`, `compiler_unit_loader.c`, or `cc_unit_descriptor`.

### Codex reconciliation

Follow-up reports no actionable correctness/security/regression defects.
R1 accepted as a non-blocking test limitation: the controlled valid-unit test
proves publication failure and cleanup, but does not capture UNIT_FORMAT itself.
Exact diagnostic observation is deferred; no diagnostic claim is made.

R2 audited: production callers in compiler_units_link.c and loader/header paths
map non-success validation to UNIT_FORMAT (or resource failures to99), and
layout lookup's CF_TARGET meaning remains unchanged. Existing standalone
negative-status checks pass in all952 native assertions. No caller depends on
invalid reserved bits returning CF_TARGET specifically.

R3 accepted and gated in runtime-compaction.md: bank16 has2 bytes free; further
integration must make room before adding code. No new bank or allocation is
introduced, and packaging validates every extent. Runtime compaction is pending.

R4 verified after follow-up without source changes. Rebuilt screen_library
(CRT is the renamed shipped Screen unit). CRT.ZPU SHA256
84d2acd17a30687124cff470726082684f931218dca0a774888107a3cb8e92e7
and existing unchanged ZVB.ZPU SHA256
987db4d9955f065d40210af2fe3524c441f3efa0d631eb6553e1215d2d63f2bd
have144 and1230 word references respectively, all upper bits clear. The
production compiler imports both, compiles a ClrScr client, runs it in RAM,
and releases pending owners. Capture:
analysis/native-slice/20261107T174132.732780Z-37160/typed-stock-report.json.
A separate saved execution explicitly removes all compiler BIN/ZPL files and
supplies no units or Pascal sources, verifies their absence and normal exit:
analysis/native-slice/20261107T174409.810611Z-39560/typed-stock-detached.json.
That absence check supersedes the earlier stock saved-run label, which only
checked exit status. Saved image SHA256:
f07a7cb14d77fff44c4aa2f68a8bc6f78b020415ce6298c19c42e86d47ad1eed.
The build-time runtime object reader emits no ZPF artifact, so it is not another
production producer requiring an encoding migration at this checkpoint.

All required scoped build, host, native, IDE and packaging checks pass. The
remaining low diagnostic-observation gap and tight-bank constraint are recorded;
this attestation covers the typed generic-linker checkpoint only, not completion
of runtime compaction, native byte-unit placement or RAM savings.
