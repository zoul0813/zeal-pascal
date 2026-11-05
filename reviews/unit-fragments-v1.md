# Review: unit-fragments-v1

## Opus turn 20261105T035516-ad1c75ee

Mode: code; read-only

# Opus review: unit-fragments-v1 (first pass)

**Verdict:** I found no blocker or high-severity defect. By code reading, validation, closure, placement, cross-page patching, cancellation, and release retry look sound for this private component. There is one placement defect, and several contract behaviors are not actually exercised by the 681/687 assertions. This assessment doesn't imply user-facing unit support.

I didn't run anything or recompute any hashes. The pass counts come from the supplied report.

## Established defects

### 1. Medium: origin at or above `$C000` allocates an unused page
- **Location:** `src/compiler_fragments.c`, `cc_fragment_link`, `pages=end>0xc000UL?2:1`
- **Trigger:** Any link with `origin>=0xc000`, such as the existing `0xf3fe` tiny case.
- **Impact:** `pages[0]` is allocated and cleared for `$8000..$BFFF`, even though no selected byte lands there. Peak and retained page counts rise by one, and a failed allocation can reject a link that fits.
- **Evidence:** The page count depends only on `end`. This contradicts the header ("allocated only for the retained high-address span") and the matching sentence in the experiment doc. The `0xf3fe` test doesn't assert page use.
- **Fix:** Compute the first and last slots, leaving `pages[0]==255` when the span starts in the upper page:
  ```c
  first=origin>=0xc000u?1:0; last=end>0xc000UL?1:0;
  for(o=first;o<=last;++o) ...
  ```
  `emit_byte` and close already work with this. Assert the page handles and `used` in the tiny case.

### 2. Low: the closure walk ignores sticky errors and has an unpolled relocation loop
- **Location:** `src/compiler_fragments.c`, the `while(n)` walk
- **Trigger:** A source goes stale during the walk, or a fragment has many relocations.
- **Impact:**
  - **Late failure:** If `field()` fails it returns 0 and `poll()` still succeeds, so the link goes on to lay out, allocate, and clear output. `emit()` then fails safely, so nothing is published, but the work is wasted.
  - **Cancellation latency:** The inner `target()` loop doesn't poll, although the equivalent loops in `validate` and `emit` poll per relocation. One fragment can hold about 2,044 relocations.
- **Fix:** Add `if(l.error)goto failed;` after the two `field()` reads, and poll inside the inner loop. Also consider polling the catalog scans in layout and `emit`, which can visit up to 4,096 unselected entries without polling.

## Test gaps

### 3. Medium: no fragment has more than one relocation
- **Location:** `tests/native/fragments.c`, `a_owners`, `b_owners`, and `tiny`
- **Impact:** `(at&&p<previous)` never runs with a real predecessor, so the ordered and disjoint rule is unproven in both directions. Multi-patch emission is also unproven.
- **Add:** One accepted fragment with adjacent patches (`p`, `p+2`), plus rejected overlapping (`p+1`), duplicate, and descending cases.

### 4. Medium: every `target_offset` is zero
- **Location:** All `refs` tables and the Python goldens
- **Impact:** The `+addend` in `value=catalog(l,to,2)+addend` could be removed without failing a test. The accepted last byte (`size-1`) and rejected one-past-end (`size`) boundaries are also untested. The current negative uses 5 on a 4-byte fragment, which is not the boundary.
- **Add:** Positive nonzero and `size-1` offsets with exact patched values, including one across fragments in different objects, and a negative at exactly `size`.

### 5. Medium: catalog and object limits are unexercised
- **Impact:** The assignment calls out these bounds, but the only limit test is an argument rejection with `count=33`. There is no coverage for:
  - 32 real objects, which writes `bases[32]`
  - exactly 4,096 fragments, using the last catalog bytes at 16382..16383
  - 4,097 fragments, which should return `CF_CAPACITY`
  - a `PAGE_SIZE`-byte object, with its last payload byte read through `far_read`
  - a selected fragment at the highest catalog ID
- **Add:** A small generated fixture of near-maximal header-heavy objects. A single page holds at most 2,046 fragments, so reaching 4,096 needs at least three objects.

### 6. Medium: page-count boundaries aren't pinned
- **Impact:** This is the expression changed for the SDCC issue, yet existing cases only reach `end=0x800B`, `0xC009`, and `0xC65E`. A `>=` regression would pass.
- **Add:** With the 11-byte selection, `origin=0xBFF5` gives `end=0xC000` and must use one page. `origin=0xBFF6` gives `end=0xC001` and must use two. Add `origin=0xC000` after fixing finding 1.

### 7. Medium: closure through a cycle isn't proven
- **Evidence:** `link(4)` roots A/1 and A/3 explicitly, and B/1 is also a root. The transitively reached fragments, B/0 and A/2, are acyclic. That case also checks only totals, not patch values.
- **Add:**
  - Root only A/1 and require exactly A/1 + A/3, with both cross-patches correct.
  - Root a non-cycle fragment that reaches a cycle.
  - Check patch words and sampled payload bytes in the all-roots image, including after the page crossing.

### 8. Low to medium: "validated before allocation" isn't asserted
- **Evidence:** `used==2` and `workspace==255` would still pass if validation ran after allocation and cleanup. The negative loop also accepts either `CF_OBJECT` or `CF_TARGET`.
- **Add:** `check(alloc_calls==0)` for every validation failure, and an exact expected status per table row. Add offset 14, because only reserved offset 12 is mutated.

### 9. Low: the 32-bit guards have no wraparound cases
- **Evidence:** The casts look correct, but `65535` at offsets 6, 8, and 22 fails regardless of width.
- **Add native cases that only 32-bit arithmetic rejects:**
  - `n`/`r` values where `n*8` or `r*8` aliases the real size modulo 65536
  - `size` near 65535 with `n>0`
  - `count` near 65535 with `r>0`

### 10. Low: evidence binding has small holes
- **Location:** `tools/test-fragments.py`
- **Issues:**
  - Area checks test each area separately, so `TEXT`, `DATA`, or `BSS` could overlap `_HOME` at `0x7000` unnoticed.
  - `measured_fixture` is a literal dictionary. Assertions back most of the values, but derive the payload totals from `expected_objects()` or rename the block.
  - The SDCC version isn't recorded, even though the source comment and doc rely on SDCC 4.4 behavior.
- **Fix:** Add pairwise area checks and record `sdcc --version` in the report.

## Documentation wording

### 11. Medium: page-crossing images are byte-checked, not executed
`fragment_run` maps only one output page at `$8000`, so the execution cases all use a single output page. The `0xbffe` case is checked by reading bytes. "Generated42 results ... physical-page crossings all pass" invites a stronger reading. Replace that sentence with:

> Generated 42 results pass for single-page images at origins `$8000` and `$8100`. Relocation bytes, exact upper bounds and physical-page crossings are verified by reading output pages through far-memory operations. Two-page images and page-crossing code are not executed by this fixture.

### 12. Low: three other scope corrections

- **Page allocation, if finding 1 is fixed:**
  > Output pages are allocated only for physical pages touched by the selected span.

  If it isn't fixed, use this instead:
  > Spans starting at or above `$C000` still allocate and clear the `$8000` page.

- **Boundary ownership:** Replace "The existing compiler poll, page allocator and far-memory operations own those boundaries." with:
  > The fixture uses production far-memory assembly and OS `palloc`/`pfree`, but test-owned `cc_poll` and `page_*` implementations. Integration with the production poll and page ledger is not established.

  Also soften the header's "cc_poll is the compiler's existing ... boundary" to "the intended boundary".

- **Peak pages and close contract:** Rename the table row to:
  > Linker ledger peak pages (2 source, 1 workspace, output; excludes the program's own process pages)

  Add this to the header:
  > After any link failure other than `CF_ARGUMENT`, the caller must call close, which may need retrying. `CF_ARGUMENT` can leave an existing valid image untouched.

## Questions and missing context

1. **Production `cc_poll`:** Does it match `uint8_t cc_poll(void)`, return nonzero to continue, and return normally on cancellation? `compiler.h` and its implementation weren't supplied. If it is banked or uses another declaration, the local prototype would be wrong at integration.
2. **Production `page_drop`:** Does a 0 return guarantee the page is still owned and safe to retry? The retry rule depends on this, and only the test ledger was visible.
3. **`tools/test-native.py`:** Does `n.execute` always start with an empty `hostfs`? Fresh output there matters for the disk-golden check. I'd also like to confirm how `symbols` and `memory` parse output.
4. **Default code segment:** Which segment do `compiler_fragments.c` and the SDK use? The runner doesn't check any area beyond those it lists, and `zos_init.cmake` wasn't supplied.

## Assessment of the two target fixes

- **SDCC page count:** The fix is sound. `limit<=0xf400` and `origin>=0x8000` are enforced before use, so two slots are enough, and no similar mixed-constant expression remains in this file. It still needs the boundary tests in finding 6. No reduced reproduction was supplied, so the folding claim rests on the source comment and the assignment.
- **SP observer:** Identical wrapper frames do remove the C-spill artifact, and reaching `resume` shows the fixture's stack is balanced. However, the wrapper restores page 2 itself before taking the "after" snapshot, so the comparison can't detect generated code changing page 2. That's acceptable for this fixture, but it proves wrapper restoration rather than generated-code MMU discipline.

## Review limitations

- I didn't have the logs, built artifacts, or `tools/test-native.py`, or the production `compiler.h`, `platform.c`, and SDK sources.
- Any fix or added test will invalidate the recorded hashes and hard-coded 681/687 counts, so the gate, evidence JSON, and doc path must be regenerated before the follow-up.

### Codex reconciliation

1. Accepted: allocation now iterates only first..last touched slots. Native tiny F3FE, C000, BFF5(end C000) and BFF6(end C001) cases pin handles, used count, bytes and patched words. Upper-only output leaves pages[0]=255.

2. Accepted: sticky errors stop poll/traversal promptly; closure polls each relocation and checks field errors. Layout polls every catalog scan, checks field errors before placement; emit polls catalog scans as well as chunks/patches and stops after stale field reads. Exhaustive small-link cancellation was regenerated for the new poll sequence.

3. Accepted: new accepted adjacent-patch fixture and rejected overlapping, duplicate and descending cases. Exact patched values are checked, not just totals.

4. Accepted: nonzero interior offset1 into a3-byte fragment, cross-object last-byte offset3 into a4-byte fragment, and exact one-past-end failures at3/4. Cross-object and local output words are pinned.

5. Accepted/adapted to physical resource limits: native3-object fixture reaches exactly4096 fragments and selects highest ID4095 (last catalog address word), then rejects4097 before any allocation. A full16384-byte object checks the last payload byte through real far_read. The32-object case is host-only:32 real source pages validate through bases[32], then correctly fail workspace allocation because all modeled physical RAM slots are used. Actual hardware cannot hold32 source pages plus its own process/workspace, so no32-object successful or native link is claimed; adding a packed-source format merely to make that test fit would expand this milestone's design. Documentation states that limitation.

6. Accepted: native boundary cases pin exact end C000, next byte C001, origin C000, and existing cross-page word patch and F400 upper bound. The high-only tiny case pins a single allocated upper page.

7. Accepted: rooting only A/1 reaches A/3 and checks both cycle patches; a separate noncycle root reaches that cycle with the acyclic B/0/A/2 path. The all-roots image checks every reference word and sampled bytes on both sides of C000.

8. Accepted: malformed-record rows use exact status per case and assert alloc_calls==0, including reserved word14. Other source-header/ownership failures also assert no allocation. Root-selection failures are separate and may allocate/close the workspace.

9. Accepted: native header n=8196 and r=8195 alias old n/r byte products modulo65536, while descriptor size65535 with prior payload offset5 and count65535 with prior relocation index1 exercise wrap-sensitive bounds. Exact format failure and zero allocations are required. These validate rejection, not a claim that each cast is the sole redundant guard that could reject each malformed object.

10. Accepted: the gate checks pairwise overlap across its linked code/data areas and rejects unrun C initialization sections; the version is obtained by a ZDE CMake target running the installed compiler's --version. Report records SDCC4.4.0 #14620. The assertion-backed fixture block is named asserted_fixture and total available payload is derived from the independently authored golden objects.

11. Accepted: docs explicitly say42 execution is single-page at8000/8100. Physical-page crossings and two-page images are byte-checked through far memory, not executed.

12. Accepted: docs and header distinguish test-owned poll/ledger implementations from production integration, clarify failure/close/CF_ARGUMENT handling, and label peak counts as the linker's ledger excluding its own process pages. The observer verifies the call wrapper's restored mapping, not arbitrary generated-code MMU discipline.

Questions: supplied read-only fragment-owner-context.txt excerpts and source hashes show production cc_poll is plain uint8_t cc_poll(void), nonzero-to-continue and returns normally on stop. Production page_drop retains its ledger record on pfree error; the inspected pinned kernel source rejects invalid owners before freeing and returns success after MMU_FREE_PAGE. These are source observations, not extra production-integration proof. tools/test-native.py creates each run directory with exist_ok=False, so library goldens cannot come from a previous capture. Supplied compiler flags show the default code segment is TEXT; all actual nonzero SDK/test areas are accounted for by the map checks.

Final verified gate: analysis/fragments/20261105T040838.905303Z-71263/report.json. 831 sanitized host assertions,770 native assertions, zero failures, exact disk goldens and unchanged source/build/emulator/ROM hashes. The full report is retained in docs/unit-fragment-evidence.json. Older first-pass report stays supplied as historical review input; it does not describe the changed code. No source edits while the required final follow-up runs. No production ABI, parser, allocator or full-program RAM saving is claimed.

## Opus turn 20261105T041315-3c7bb16e

Mode: code; read-only

# Opus final follow-up: unit-fragments-v1

**Verdict:** I found no blockers and no remaining high- or medium-severity findings. Findings 1–12 are resolved, and I found no regression from the fixes. By code reading, this private linker component is sound within its stated scope. It is not finished or user-facing unit support, and production integration is still unproven.

I didn't run anything or recompute any hashes. The 831 host and 770 native passes come from the supplied gate report, whose content matches `docs/unit-fragment-evidence.json`.

## Finding dispositions

| # | Status | Assessment |
|---|---|---|
| 1 | Resolved | `first..last` allocates only the touched slots. An upper-only span can't produce `first>last`, because every root fragment has at least one byte. Close and `emit_byte` both handle `pages[0]==255`. |
| 2 | Resolved | `poll()` now stops on sticky errors, and the closure, layout and emit scans all poll. Errors from `field()` stop the link before layout or copying continues. See residual item 1. |
| 3 | Resolved | Adjacent patches pass, while overlapping, duplicate and descending patches return `CF_OBJECT` with no allocation. |
| 4 | Resolved | Interior and last-byte offsets are checked by exact word values, and one-past-end is rejected at sizes 3 and 4. |
| 5 | Resolved, adapted | 1818+1818+460 gives exactly 4,096 fragments, and root 12/459 selects ID 4095, the last catalog entry at bytes 16380..16383. 4,097 fails before allocation, and the full-page object reads its final byte through `far_read`. The host-only 32-object case is a reasonable adaptation and is documented. See residual item 2. |
| 6 | Resolved | Ends `$C000` and `$C001`, plus origin `$C000`, are pinned with page handles and `used` counts. |
| 7 | Resolved | A/1 alone retains A/3, a separate non-cycle root reaches the cycle, and the all-roots image now checks reference words and sampled payload bytes. |
| 8 | Resolved | Each table row has an exact status and asserts `alloc_calls==0`, including offset 14. Root failures happen after workspace allocation, and the doc's claim covers object validation, so this is consistent. |
| 9 | Resolved | The cases exercise the guards as requested. Only `n=8196` isolates its arithmetic: a 16-bit alias would reach `CF_CAPACITY` instead of the expected `CF_OBJECT`. The other three would likely still fail through later partition checks, which matches Codex's caveat. |
| 10 | Resolved | Sorted adjacent checks do detect any overlap among listed areas. The SDCC version is recorded, the fixture block is honestly named `asserted_fixture`, and available payload is derived from the goldens. See residual item 3. |
| 11 | Resolved | The docs now limit execution to single-page images and say crossings are byte-checked only. |
| 12 | Resolved | The docs and header now cover allocation scope, test-owned poll and ledger code, close obligations, and peak-page labeling. |

## Checks against the current fixtures

- **Expected values:** I recomputed the layouts and patched words by hand for the all-roots, `patches_and_cycles` and `capacity` cases, and they are correct.
- **Assertion counts:** The difference reconciles. The host-only `maximum_objects` adds 67 checks and three native executions add 6, so 831 − 67 + 6 = 770.
- **Table integrity:** The `locations`, `values` and `errors` tables each have 20 aligned rows.
- **Mutation timing:** By my count, a one-root link makes about 164 polls, so `mutate_at=total/2` lands in the closure walk. The `total` case lands at the final scan, so the post-emit `live()` check is what catches it.

## Earlier questions

1. **`cc_poll`:** The excerpt matches the local prototype: nonzero continues, and it returns normally when stopping. Integration must account for two behaviors:
   - It always continues when `cc.polling` is off.
   - A pending compiler error surfaces from the linker as `CF_CANCELLED`.
2. **`page_drop`:** The ledger entry is kept on failure, and the kernel excerpt rejects an invalid owner before freeing, so retrying is safe. A wrong-owner failure will simply keep failing. The worker-mode check in `page_drop` also makes page ownership part of the future integration.
3. **Fresh `hostfs`:** Each run directory is created with `exist_ok=False`, so old disk objects can't satisfy the goldens.
4. **Default code segment:** The flags show `TEXT`, which the area checks cover.

These answers rest on supplied excerpts, whose hashes I didn't verify.

## Residual low-severity items

None of these needs a change before closing this milestone. With the code frozen, record them for the next integration work.

1. **Early-stop timing isn't pinned:** The `total/2` mutation asserts `CF_OWNERSHIP`, but not that output allocation was skipped. Adding `alloc_calls==1` for that case would catch a return to late failure.
2. **Many-object success is unexercised:** The largest successful link uses three objects. The 32-object case fails at allocation, so it writes `bases[32]` but never reads it in the walk, and zero-fragment objects never reach a successful link. A host-only link with about 30 objects would cover both.
3. **The area list is fixed:** The runner checks a named list, so a new nonzero area wouldn't be caught. Enumerating all `l__*` symbols would close that. I also can't confirm Codex's statement that all nonzero areas are covered, because the map wasn't supplied.
4. **A clean rebuild won't run as documented:** `n.execute` copies `build/zpascal.*` and `build/zptest.*`, but the gate builds only `zfragmenttest_bin`. Those copied files aren't hashed or executed, so the evidence still holds. It does mean the reproduce command depends on an existing build tree, which fits the doc's "incremental" wording.

## Material limits

- **Evidence:** This is emulator acceptance on pinned binaries, from an incremental build. It isn't hardware verification or clean-toolchain reproducibility.
- **Execution:** Only single-page images run. Two-page images and page-crossing code are byte-checked only, and the snapshot proves wrapper restoration, not MMU discipline in generated code.
- **Production integration:** The fixture uses test-owned `cc_poll` and `page_*` code, and neither compiler calls this linker. Worker and IDE ownership are not established.
- **Object limit:** 32 is a nominal bound. No successful 32-object link is claimed, and real capacity depends on free RAM pages.
- **Explicit exclusions:** These remain with their owning integrations and aren't defects here:
  - `.ZPU` format and unit name, type and ABI exports
  - parser and import work
  - source-unit cycle policy and initialization order
  - source maps and symbolic Inline operands
  - publication transactions and allocator placement
- **Savings:** The page and byte counts apply only to this fixture and aren't full-program savings.
- **This review:** I didn't have the logs, built artifacts or link map, and the SDCC folding claim still has no reduced reproduction.

### Codex reconciliation

Accepted the final assessment: no remaining high/medium finding or code blocker for the private component. All substantive first-pass fixes and documentation corrections preceded this follow-up. Code, tests, supplied docs, evidence and context stay unchanged after it.

Recorded residual low-severity items for the owning integration rather than changing the reviewed snapshot: (1) pin workspace-only allocation count for closure-time stale-source early failure; (2) add a successful many-object/empty-object host case (the current32-object case proves validation then resource failure); (3) derive the complete area list from l__* symbols instead of the current named list; (4) remove the shared native runner's dependency on existing unused zpascal/zptest corpus files, or explicitly build that corpus when enabling clean-tree reproduction. The present worktree contains those files, and this is explicitly an incremental-existing-tree emulator proof. They are not executed or claimed as fresh inputs to the fragment proof.

The material limits remain: single-page generated-function execution only; two-page images checked as bytes; wrapper mapping restoration only; test-owned polling/page ledger; nominal32-object bound constrained by physical RAM; no Pascal parser/unit import, public ZPU/type/ABI contract, source maps/initialization/Inline integration, hardware proof, or whole-program page saving. These limits are stated in the component documentation and this record.

Codex verified the gate's source and build hashes, native completion/assertion counts, exact host/native disk objects and emulator/ROM stability. Final accepted gate analysis/fragments/20261105T040838.905303Z-71263/report.json has831 ASan/UBSan host checks and770 native checks, zero failures. No extra review round or source edit followed the final review. Remaining low items are deferred explicitly; this milestone can close without claiming production integration.
