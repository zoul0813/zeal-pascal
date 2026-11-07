# Review: runtime-foundation-v1

## Opus turn 20261107T161241-c1caaca3

Mode: code; read-only

# Review: runtime-foundation-v1 (read-only)

**Scope:** `tools/runtime_objects.py`, `tools/test-runtime-objects.py`, the CMake probe targets, `tools/runtime-baseline.py`, the `tools/test-program-pages.py` diff, and the verification JSON.

**Limitations:**
- I ran nothing. Test results come only from the supplied JSON.
- I do not have `link.txt`, the `.rel` or `.map` files, or the ZOS toolchain link flags. I could not confirm whether library members or linker-generated symbols occur in the six targets.
- The expanded program-pages run is pending, so it is not evidence.

## Established defects and weaknesses

### 1. Medium: the linked oracles only check that converter output is a subset of the HEX

**Where:** `tools/test-runtime-objects.py`, `linked_oracle`.

**Trigger:** The converter could drop a T record, a whole area, or a library member's bytes.

**Impact:** The oracle would still pass. It iterates only `image.items()` and checks each address exists in `expected`. Nothing asserts that every byte in the linked HEX was produced from the listed `.rel` inputs.

- Objects are selected by `token.endswith('.rel')`.
- Any SDCC library member pulled in by `-l` or a `.lib` path would therefore be invisible.
- So "6 image oracles pass" proves the reproduced bytes are correct. It does not prove the converter reproduced the image.

**Fix:**
- Assert `set(image) == set(expected)`, restricted to the target's non-ABS link span.
- Otherwise, enumerate any excluded addresses and attribute each one to a named library member or ABS module.
- Report `expected_bytes` next to `initialized_bytes_checked`.

### 2. Medium: linker-defined and library symbols are taken from the oracle itself

**Where:** `linked_oracle`, the line `external = {name:value for name,value in symbols.items() if name not in owned}`.

**Problem:** This supplies every map symbol that no object defines, including `s__AREA`, `l__AREA`, and any library entry points, straight from the map being tested. Any relocation that targets these symbols is verified circularly.

**Production gap:** The converter has no model for linker-generated area symbols. It cannot yet compute them.

Initialized-data copy loops are the likely case: SDCC `_INITIALIZER` / `_INITIALIZED` / `_GSINIT` ordering and fall-through, referenced via `s__INITIALIZER` / `l__INITIALIZER`. The probe has no initialized RAM data, so it never exercises this path.

**Fix:**
- Restrict `external` to an explicit allowlist of platform symbols.
- Fail if any relocation resolves to an `s__` or `l__` name or to a library symbol.
- Alternatively, have the converter derive area symbols from its own placement and compare those against the map.

### 3. Medium: resolution checks overlap only for initialized bytes

**Where:** `tools/runtime_objects.py`, `resolve_objects`.

**Problem:** Overlap is detected only through the `memory` dict of initialized offsets. A BSS or `_DATA` area placed on top of code, another BSS area, or an ABS platform region (gate, `0xfe74`) passes silently.

**Impact:** Stage 3 will use this for dynamic placement, so the gap moves straight into production.

**Fix:**
- Check pairwise `[base, base+size)` intersection for all non-empty areas, including uninitialized ones.
- Optionally allow caller-declared exclusion ranges.

### 4. Low: addends that wrap past 0xFFFF are rejected

**Where:** `_text` and `resolve_objects`.

**Problem:** A 3-byte byte-mode field holding a negative value, or a word reference such as `_sym-1` whose sum exceeds 65535, is rejected. ASxxxx truncates these modulo 2^16.

**Impact:** This fails closed, so it is safe. It is still a false rejection that may surface on legitimate code later.

**Fix:** Either document the restriction or confirm the pinned aslink semantics and mask accordingly. Add a test for whichever is chosen.

### 5. Low: the verification JSON is not bound to the code under review

**Where:** `reviews/runtime-object-verification.json`.

**Problem:**
- `input_sha256` covers build artifacts only, not `tools/runtime_objects.py` or `tools/test-runtime-objects.py`.
- It reports only total `relocations_checked`, so there is no evidence that modes 9, 11, 137 and 139 (low8/high8, area and symbol) were exercised by the paired a/b oracles. The carry-inducing data origins `0x70fd` and `0xb0ff` only prove something if byte relocations to `_DATA` actually exist.

**Fix:** Hash both scripts and report a per-target count for each mode.

### 6. Low: the baseline infers the Real hole from zero bytes, and labels reserved storage as padding

**Where:** `tools/runtime-baseline.py`.

**Problems:**
- **Hole check:** The gap is computed from symbols, then `assert`ed to be zero in the HEX-derived `.zpl`.
  - For `integer_runtime`, `validate_runtime(compact=False)` does not run the all-areas scan, so nothing proves that no uninitialized area overlaps the hole.
  - That conflicts with the docstring's "never infer".
- **`end_padding_bytes`:** `16384 - len(integer)` includes `REAL_DATA` (0x7ea6–0x8000), which is live RAM.
- **`assert` use:** Plain `assert` is skipped under `python -O`.

**Fix:**
- Scan the map's area table for intersections with the hole.
- Rename the field, or split it into uninitialized-reserved bytes and genuinely unowned bytes.
- Raise `ValueError` instead of using `assert`.

### 7. Low: weak native-measurement checks in `test-program-pages.py`

1. **"Prefix retained" reads a variable, not ownership.** `RAM prefix retained while module mapped` reads `0xfe76`, which is the engine's saved MMU byte. That shows bookkeeping, not ownership or mapping. RAM mode also captures no `mmu` at the module stop.
   - **Fix:** Rename the check, or capture `mmu` and verify that 8000/C000 are unchanged and 4000 maps to the module page.
2. **No release check after RAM Run.** Nothing verifies that `0xfe74` is cleared and the module page released after RAM Run.
   - In standalone mode, kernel process cleanup would hide a leak.
   - The RAM path is where a leak would accumulate across repeated Runs.
   - **Fix:** Add a check at `_key_read`.
3. **Breakpoint authentication is adequate.** The `0x4000` breakpoint is authenticated by `ZFIL` plus `PC=4000`, and misses fail rather than pass. I found no false-positive path there.

## Questions and forward risks

### 8. Can ZPF1 carry byte relocations?

`docs/runtime-compaction.md` says runtime objects "reuse the unit linker's dependency/relocation model". However, `compiler_fragments.h` defines ZPF1 relocations as word patches only, with no kind field.

The converter deliberately retains low8 and high8 relocations, and SDCC C code emits them for pointer arithmetic. How will Stage 2 represent them?

- Extend the format with a relocation kind, or
- reject such objects at conversion.

Either way, the result must fail closed rather than coerce a byte relocation into a word patch.

### 9. Pending program-pages run

The `BLOCKIO`, `TYPEDIO` and `TEXTIO` results, including the 3-page `expected_pages` assumption for file cases, are unverified until that run is supplied.

## Converter correctness

I found no silent-acceptance path in the parser:
- Unknown records, modes, area flags, duplicate areas, header count mismatches, overlapping or out-of-range relocation sites, and unknown targets all raise.
- The 3-byte collapse for byte-mode fields and the base-plus-addend carry behave as described.

### Codex reconciliation

Accepted findings 1–3: oracles now reconstruct every initialized HEX byte,
select SDK archive dependencies independently, derive generated area symbols,
and reject overlapping live extents including BSS. Eleven unit tests and six
exact linked-image comparisons pass (hashed verification JSON supplied).

Finding 4 is deliberately restricted: signed/wrapping addends fail closed and
have rejection tests. Their support is deferred until production input needs
and pinned toolchain semantics are qualified. Finding 5 accepted: source and
artifact hashes plus mode counts now include external low/high carry probes.

Finding 6 accepted: all map/object areas prove the hole unowned; tail is split
into 138 live BSS bytes and 55 unowned bytes. ValueErrors replace assertions.
Finding 7 accepted: native tests observe actual module MMU mappings, unchanged
upper windows, handle clearing before parent fallback, and release on RAM return.
The final expanded native run passes all checks; full hash-bound baseline is
supplied. Question 8: typed native relocations are the next stage; this tool
retains kinds and emits no production ZPF. The unchanged word-only reader
rejects byte references rather than accepting a coerced representation.
Question 9 resolved by the completed native run. Production output unchanged.

## Opus turn 20261107T164014-1f0c360c

Mode: code; read-only

# Follow-up review: runtime-foundation-v1 (read-only)

**Verdict:** No blockers or high-severity issues remain for this foundation-only checkpoint. Four low-severity residuals follow.

**Basis:** Source as supplied, `runtime-object-verification.json` (11 unit tests, six oracles) and `runtime-baseline-evidence.json` (native run). I executed nothing; passing results are taken from those artifacts.

## Disposition of initial findings

| # | Finding | Status | Evidence |
|---|---|---|---|
| 1 | Oracle only checked a subset of the HEX | **Resolved** | `linked_oracle` asserts `set(image) == set(expected)`. SDK `z80.lib` members are selected by `library_closure` and cross-checked against the map's "Libraries Linked" list. The JSON shows `expected_initialized_bytes == initialized_bytes_checked` for all six targets, and the pulled members (`mul.rel`, `crtenter.rel`, `_strcmp.rel`, …) are now byte-verified. |
| 2 | Area and library symbols came from the map under test | **Resolved** | `area_symbols` derives `s_`/`l_` values from the chosen placement, and only those are passed as externals. Any other unresolved name raises. The probe now has initialized data (`runtime_seed`) plus an `s__INITIALIZER`/`l__INITIALIZER`/`s__INITIALIZED` copy loop, and both a/b placements match exactly. |
| 3 | Overlap check ignored uninitialized areas | **Resolved** | `resolve_objects` rejects any overlap between non-empty extents, including BSS and absolute areas. Two unit tests cover it. |
| 4 | Signed/wrapping addends rejected | **Accepted as deliberate** | Fail-closed, with rejection tests. No action needed now. |
| 5 | Evidence not bound to source; no per-mode counts | **Resolved** | Source hashes are recorded. Per-mode counts show `low8_symbol` and `high8_symbol` exercised in the a/b probes, at carry-inducing origins. |
| 6 | Real hole inferred from zero bytes | **Resolved** | Map areas and object ABS areas are scanned for overlap with the hole. `ValueError` replaces `assert`. The tail is split into 138 live bytes and 55 unowned bytes. |
| 7 | Weak native-measurement checks | **Mostly resolved** | See residual R1. |
| 8 | ZPF1 cannot carry byte relocations | **Acknowledged** | The converter emits no ZPF artifacts, and the doc now states this gap. |
| 9 | Program-pages run pending | **Resolved** | 253 checks pass. Standalone file cases show 3 pages owned at `zos_loader_exit` against module page 39. That proves the program released the module page before kernel cleanup. |

## Remaining findings (all low)

### R1. The RAM "clears module handle before parent cleanup" check can pass by reading the wrong page

**Where:** `tools/test-program-pages.py`, RAM branch for `FILE_CASES`.

**What happens:** At the `parent_return` stop (address read from `0xfc12`), the test reads `0xfe74` without capturing `mmu`.

**Risk:** If C000 is already mapped back to the IDE's own data page at that point, the zero byte belongs to the IDE, not the program. The check would then pass trivially.

**Mitigation already present:** The companion check `result[81]==0 && result[82:114]==0` likely provides independent IDE-side evidence. However, the meaning of those `_ram_result` offsets is not documented in the supplied context.

**Fix (pick one):**
- Capture `mmu` at `parent_return` and assert `c000 -> pages[2]*16384`.
- Document what `_ram_result[81..113]` represents.

### R2. The hole ownership proof covers linker areas, not absolute equates

**Where:** `tools/runtime-baseline.py`.

**Gap:** The scan covers relocatable and ABS linker areas only. An absolute `.equ` used as scratch inside the hole would not appear.

**Fix:** Also check that no defined absolute symbol, from objects or the map, falls in the hole range [0x4f9e, 0x77cd). This is cheap and completes the "never infer" claim.

### R3. The design doc contradicts itself on the relocation model

**Where:** `docs/runtime-compaction.md`.

**Problem:** "Current work" still says runtime objects "reuse the unit linker's dependency/relocation model". The final paragraph correctly says ZPF1 accepts only word references.

**Fix:** Reword the first statement as a goal that depends on typed byte relocations.

### R4. Evidence reproducibility

**Where:** `docs/runtime-baseline-evidence.json`.

**Problems:**
- `input_sha256` includes `/private/tmp/runtime-baseline-final.log`, which lives outside the repository and will not survive.
- The nested pages report hashes `tools/test-program-pages.py` as `5ad53cfa…`. I cannot confirm that matches the reviewed file.

**Fix:** Keep the log under `analysis/`, or drop it from the hashed inputs. Confirm the pages-script hash against HEAD.

## Limits

- **Converter output is replay only.** Correctness is proven against map-derived area bases. Independent placement and ordering is untested until Stage 3; that is acceptable for this checkpoint.
- **Library selection differs from aslink.** `library_closure` pulls members only for relocation-referenced symbols, whereas aslink also resolves unreferenced `Ref` declarations. It also fails on any symbol that more than one member provides. For these six targets the member list matches the map. Other inputs may diverge, and the closure fails closed when they do.
- **File-module coverage is first-call only.** The native file-page observations cover the first module call, not dynamic peaks, as the report's `limits` field states.

No production compiler or runtime output changed.

### Codex reconciliation

Follow-up has no blockers. R1 addressed through the review's documentation
option: worker.c publish() writes the authoritative PAGE_WORKER count at child
FC51 and the exact owned handle list at FC52..FC71, zeroing unused slots.
native.asm copies FC00..FC7F to ram_result before parent cleanup; offsets
81 and 82..113 therefore prove the child released all worker allocations before
WORKER_FINISH can perform fallback cleanup. The separate FE74 observation is
supplementary, with its unauthenticated top-window limitation acknowledged.

R2: independently scanned all defined absolute symbols from every linked input
object; none falls in [4F9E,77CD). The automated analyzer currently proves linker
area ownership only; extending its absolute-equate guard is deferred to the
next stage. No actual hidden scratch storage was found.

R3 accepted as a wording correction for the next stage: reuse is an intended
extension, not existing typed-byte support. This checkpoint produces no ZPF.
R4: compared the actual pages script SHA256 to nested evidence: exact match.
Preserved the native log under analysis/native-slice/20261107T162622.246515Z-56127/
runtime-baseline-pages.json as well as its original temporary path. The embedded
JSON remains in committed evidence; raw debugger captures remain local as usual.

Required checks: 11 converter unit tests, six complete-image comparisons, all
253 native checks and git diff --check pass. Production defaults remain
unchanged. This approval is limited to foundation tooling and measurements;
typed native references, independent production placement, runtime compaction,
file-overlay replacement and one-page support are subsequent work.
