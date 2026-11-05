# Review: library-exploration-v1

## Opus turn 20261105T003348-07e5daec

Mode: creative; read-only

# Creative review of `docs/library-exploration.md` (first pass)

**No edits were made.** I reviewed the supplied text and ran nothing. Addresses and technical behavior below need Codex's verification.

**Verdict:** The document is honest and technically careful, but it isn't yet easy for the owner to use. Its best distinctions are scattered through dense design rules, and the headline makes capacity recovery sound more certain than the body does. The largest gaps are the Chain contract, the first-version authoring workflow, and the go/no-go criteria.

## Priority recommendations

### 1. Name the two goals and state the risk (`Recommendation`)
"Decisive second requirement" and "neither goal" refer to goals that are never named, and `Readiness` then says "the goal."

Suggested wording:

> This proposal has two separate outcomes: **reusable libraries** and **more memory for programs**. Units and selective linking can deliver the first. The second also requires a new memory layout, which is unproven. The library feature could succeed while adding no usable capacity.

Also add one sentence saying who benefits: integer-only programs gain most, while programs using Real may gain little.

### 2. Add an explicit compile-time/execution table (after `Recommendation`)
"It does not let a program execute library code that exists only at compile time" is correct, but too abstract to correct the likely misconception. The key promise that static programs need no library files is also buried at the end of the ZVB section.

| Item | Used by compiler | In saved program | In memory while running |
| --- | --- | --- | --- |
| Unit interface | Yes | No | No |
| Exported routine never reached | Available | No | No |
| Reached routine and its dependencies | Yes | Yes | Yes |
| Non-empty unit initialization | Yes | Yes | Yes |
| Real literal conversion | Yes | Resulting value only | Value only |
| `.ZPU` file itself | Yes | No | No |
| Future banked routine | Yes | Stored in the program or a separate asset (undecided) | Trampoline resident; body in other pages |
| Future overlay | Yes | Separate/on disk | Loaded on demand |

### 3. Separate the meanings of "runtime"
A document about compile time versus runtime uses "runtime" for both the library and the execution phase. "Optional runtime components" are actually selected at link time, and the heading `ZVB library and runtime banking` can be read two ways.

- Use **runtime library** or **resident prefix** for the code.
- Use **during execution** for the phase.
- Rename the section `ZVB library and banked execution`.
- Define or reduce overloaded uses of "retained." It currently means kept by the linker, preserved, and still referenced.

### 4. Show the fixed-prefix blocker visually
The explanation is split across three sections, and the terms shift between "hole," "low-page," "page 1," and "below `$8000`." A small diagram would make it clear at once:

```text
Today:              [$4000: runtime + padding][$8000: globals + code...]
Selective link only:[$4000: small runtime|HOLE][$8000: globals + code...]
Goal (unproven):    [$4000: small runtime|usable][$8000: ...]
```

Then say the crucial part directly: **the space we want back is in the window that CRT and file operations temporarily replace.** This appears to be true from the document and evidence, but Codex should confirm it.

Also resolve this apparent contradiction: "replace the fixed `$8000` globals base" versus "do not simply lower `GLOBAL_BASE`." Explain that the base can change only through a checked placement model, and list what may and may not occupy the swappable window.

### 5. Add the Chain contract as a design decision
This is the most material omission. `CHN deliberately borrows its caller's runtime`, while selective linking makes each caller's runtime different. The linker can't see what a later chained program will need, so a parent may omit a service the child requires. Per-image globals may also conflict with Chain's shared-globals behavior.

The document lists Chain as something to audit, but this is an unresolved compatibility decision. Options to compare include:

- A full-runtime mode for Chain programs
- Declared runtime requirements checked when chaining
- Child programs that carry their own dependencies
- Rejecting mismatched combinations

### 6. Add a concrete authoring walkthrough
The example shows a unit and a one-line import, but not how someone works with it. `Proposed native unit model` should open with a short path:

1. Write and save `TILEMATH.PAS`.
2. Compile it in the IDE to `TILEMATH.ZPU`.
3. Add `uses TileMath;` to the program.
4. Run or save the program, which links only what it reaches.
5. Edit the unit and see what happens next.
6. Hit a compile or runtime error inside the unit and see where the IDE takes you.

Steps 2 and 5 aren't currently defined. Because Make is deferred, say whether the first version requires manual compilation in dependency order and whether stale units simply stop Run with a diagnostic. Show the importing program as code too.

### 7. Call out IDE Run as a deliberate departure from Turbo Pascal
The TP table repeats that smart linking was disabled when compiling to memory, while the recommendation applies it to IDE Run. The document never reconciles these. Add this explicitly, with the reason and the cost to Memory-mode compilation and linking.

A small **Inherit / Change / Defer** table would help after the TP findings:

| Treatment | Items |
| --- | --- |
| Inherit | Unit structure; procedure and declaration-section granularity |
| Change | Linking in Memory mode; `.ZPU` format; rejecting unit cycles |
| Defer | `.TPL`-style archives; overlays; banking |
| Not proposed | TPU binary compatibility; general finalization |

### 8. Add an owner decision list and real gates
The heading says `acceptance gates`, but nothing defines pass, fail, or stop. Add a short `Decisions needed` section before the sequence:

- **Minimum value:** What capacity gain justifies changing the layout?
- **Failed layout proof:** Do units still proceed for their reuse value?
- **Layout coexistence:** Is "keep the current layout available" temporary or permanent, and who selects it?
- **Old artifacts:** Which remain supported?
- **Language boundary:** How are `unit`, `interface`, `implementation`, and `uses` introduced without breaking TP3 source that uses those identifiers?
- **Source expectations:** What happens with TP-style `uses Crt, Dos`? The document rejects TPU binary compatibility but is silent on source compatibility.
- **Name rules:** Is qualification optional, how do collisions resolve, and are interface variables or implementation-section `uses` allowed?
- **File mapping:** How does a unit name map to a Zeal filename, including long names?
- **Native boundary:** How does a unit author attach native code on Zeal? The ZVB plan depends on this, but only Inline reachability is discussed.
- **ZVB ownership:** Is ZVB a bundled library, a sample, or a user-maintained unit?

### 9. Fix the sequence logic
Step 2 proves layout and dependencies with "tiny linked images," but linkage arrives in step 3. Later, an "interim selective-link implementation" implies the opposite order.

Say whether step 2 uses hand-built prototypes and what the interim stage delivers. With a fixed padded prefix, it may offer neither extra capacity nor a smaller file, so its value appears to be proof only.

### 10. Add a mechanism-by-budget matrix
The four budgets are good, but they are explained only in prose. Use a table with these rows:

- Fixed-layout selective linking
- Compact layout
- Banked code
- Overlay

Its columns should be logical space, execution pages, compiler peak pages, and file size. Replace "Freeing 10 KiB," which reintroduces the unproven figure, with "Freeing logical space."

## Smaller clarity corrections

11. **Heading accuracy:** Rename `What TP4–TP7 actually did` to `What the TP4–TP7 manuals document`. The current title conflicts with the first sentence below it. Move the OCR provenance and P2/P3 aside to a note or the evidence file.
12. **Inconsistent gate:** "Unused exports/data absent" conflicts with declaration-section retention. Use "unreached procedures and unreferenced declaration sections absent."
13. **Ambiguous "cycles":** Distinguish reachable *routine call cycles*, which work, from *unit dependency cycles*, which are rejected.
14. **Screen decision:** "Can be organized internally" sounds undecided. If this is the recommendation, say: "The first version adds no `Screen` unit."
15. **ZVB in the headline:** Add one sentence to `Recommendation` saying that ZVB would be statically linked first, with banking considered only against a measured limit.
16. **Authorization boundary:** Add "This document does not authorize implementation" to the status. Consider renaming the sequence `Possible implementation sequence`.
17. **Readiness wording:** Replace "establishes that selective static units fit the goal" with "supports selective static units as the design direction." Also clarify "binary-format recovery," which appears to contradict the statement that TPU compatibility isn't proposed. If TPU internals simply weren't examined, say that.
18. **Scannability:** Divide the unit model with subheadings: Language, Compiled format, Linking, Initialization, Lookup, and Failure behavior. This keeps the detail without forcing the owner to read it as one continuous specification.

## Technical claims for Codex to verify

- The prefix occupies `$4000–$7FFF`, and that is the same page 1 window replaced by CRT and file operations.
- Selective linking under the fixed layout doesn't reduce saved-file size.
- CHN programs cannot safely assume a selectively linked caller provides their services.
- The new keywords conflict with valid TP3 identifiers unless they are handled contextually.
- Zeal filename limits affect unit lookup.
- The future banked and overlay rows in Recommendation 2 are accurate.
- The compatibility case for keeping fixed public service gates is clear.

## Unresolved issues

- **Creative suitability:** Strong evidence, but the document needs better orientation and more explicit owner decisions.
- **Technical readiness:** Not established. The Chain contract, compact-layout feasibility, first-version workflow, and native-code authoring path are all still open.
- **For the follow-up:** Please give dispositions on Recommendations 5, 6, and 8 in particular. If Chain is already resolved elsewhere, point to that source and I'll withdraw or narrow Recommendation 5.

### Codex reconciliation

Accepted 1–2: named reuse and capacity as independent outcomes, added a concise
phase table and moved the no-runtime-unit-file promise up front. Adapted table
scope to current static units: future banking/overlay details stay in their own
section so provisional designs do not resemble supported behavior.

Accepted 3–4: renamed banked execution and manual findings headings; added the
address diagram and explicitly tied the desired hole to page 1. Verified against
compiler prefix emission, file_gate.asm and compiler_crt.c. The phrase about
lowering GLOBAL_BASE now follows a required symbolic placement model rather than
suggesting a constant-only edit. Fixed-layout prefix deletion alone leaves
compiler-computed length unchanged. Broader linker improvements could still
shrink code above the prefix; no blanket file-size claim is made.

Accepted 5: added a Chain release gate. Existing CHN borrows a complete runtime
and fixed shared-globals addresses; initial recommendation preserves those for
Chain-capable builds and rejects compact handoffs. Capability enforcement is
explicitly unproven; compact remains experimental until this is solved.

Accepted 6–7: showed the importing program, manual dependency-order unit compilation,
stale/absent-source behavior, source error handling and deliberate selective
linking in IDE Run. Added its compile-time/workspace cost and inherited/deferred
scope in prose rather than another summary table.

Adapted 8: gave recommendations instead of blocking this exploration on a new
owner questionnaire. Proposed contextual keywords (verified absent from current
reserved list), unambiguous imports/qualification, interface exports and private
uses, independent unit identity/basename, no silent DOS truncation, bundled ZVB
source and existing Inline/Port/Mem native boundary. Filename behavior requires
platform proof rather than an invented exact filesystem limit. Old version-2
readers remain; compact gets a new ABI. Minimum numeric capacity target and
permanent coexistence remain product decisions after measurement. Added explicit
positive-net-capacity and mapping-safety gates; unit reuse may proceed separately.

Accepted 9: the pre-linker proof now explicitly uses hand-built target fixtures;
fixed-prefix selective linking is a dependency proof, not a user-capacity promise.
Adapted 10: retained the four-budget prose instead of an additional speculative
matrix; replaced the hypothetical 10-KiB freeing statement with logical space.
Evidence does not determine future page counts or compiler peaks.

Accepted 11–18: precise manual heading, corrected TP7 units citation to chapter 10,
declaration-section gate wording, distinct routine versus unit cycles, explicit
no-new-Screen-unit policy, static ZVB first, exploration-only authorization,
possible-sequence heading, accurate readiness/TPU-internals boundary, and smaller
unit-model subsections. OCR provenance remains near citations for traceability;
its short example caveat prevents repeating a known source error.

Verification: five downloaded OCR hashes and three copied baseline artifact hashes
match the evidence record. Runtime payload length and map sections independently
match 16191/10819/279/345 bytes. Source inspection supports fixed prefix/globals,
page-1 replacement, legacy CHN borrowing and contextual-keyword necessity. No
compiler code changed; no target build/test or savings claim. Opus changes.diff
is empty, as expected for consultation.

Unresolved implementation decisions are intentional exploration outputs, not
claims of technical readiness: exact unit format/ABI, path mapping proof, compact
placement feasibility, chain capability enforcement, complete native adapter and
unit error-map integration, capacity threshold and long-term defaults.

## Opus turn 20261105T003756-06aa253a

Mode: creative; read-only

# Final creative review of `docs/library-exploration.md`

**No edits were made, and I ran nothing.** Hash results and source checks are as reported by Codex.

**Verdict:** The revised document is suitable as an exploration once the five wording fixes below are made. The structure now works, and the two goals, compile/execution split, blocker diagram, and evidence boundary are clear. The remaining problems are places where sections disagree, not reasons to restructure or add scope. I'm not treating intentionally open details as defects unless the text implies they're settled.

**Technical readiness:** Not established, as the document correctly says.

## Initial recommendations 5, 6, and 8

| Recommendation | Disposition | Remaining issue |
| --- | --- | --- |
| **5. Chain** | Resolved in substance | The stop condition omits Chain, and "before handoff" is ambiguous (items 2 and 6) |
| **6. Authoring** | Resolved as a workflow | Make scope and the recompile rule are inconsistent (items 3 and 4) |
| **8. Decisions** | I accept the adaptation | Recommendations work better than a questionnaire, and deferring the numeric threshold is honest, but the "Proceed" and exit wording needs tightening (items 2 and 5) |

I also accept the adaptations to Recommendations 2, 7, and 10. Keeping future banking out of the current-phase table is the better choice. Recommendation 13 is only partly applied (item 8).

## Fixes needed before finalizing

### 1. State what the default build delivers (`Recommendation`)
This is the main remaining misleading impression. The opening describes optional runtime components and integer-only beneficiaries, but the default later turns out to be legacy layout. A reader could assume integer programs gain memory by default.

Suggested wording after "could succeed while adding no usable capacity":

> With the proposed legacy default, units provide reuse and omit unreached unit code, but ordinary programs gain no reclaimed prefix capacity. Any capacity gain would require the separate, experimental compact layout. If that succeeds, integer-only programs are the strongest candidate beneficiaries.

Also limit the table's "Unreached procedure | Omitted" row to unit code, or add a note. Chain-capable builds keep the complete runtime, so "omitted" isn't universal. The diagram's "Linking alone" row should be labeled a dependency proof, not a user-capacity state.

### 2. Align the compact exit criteria with the Chain gate (`Recommended decisions and stop conditions`)
The paragraph lists only net capacity and page-1 safety, while the Chain section calls itself a release gate. "Keep experimental until..." also implies that a one-byte gain could end the experimental status.

Suggested wording:

> Compact layout remains experimental unless an integer program shows a positive net increase in allocatable ordinary memory, all page-1 safety cases pass, and Chain capability enforcement is proven. These conditions are necessary but not sufficient. Any wider availability still needs a measured owner decision.

Then change step 2's "before replacing defaults" to "before any wider compact availability." Step 2 alone can't justify a default change.

### 3. Resolve the Make and fingerprint contradiction
The workflow says source and option fingerprints stop Run in the first version. `Lookup and failures` says source lookup and fingerprinting must be specified "before exposing an IDE Make command."

Those rules are needed earlier than Make. Suggested wording:

> Source lookup and option fingerprinting must be specified for the first manual workflow. Automatic rebuild policy must be specified before exposing an IDE Make command.

### 4. Correct the manual recompile rule
"After editing a unit, recompile it and relink its users" conflicts with the later rule that interface changes require dependent recompilation.

Suggested wording:

> After editing a unit, recompile it. If its public interface changed, also recompile dependent units in dependency order, then compile or run the program to relink it.

### 5. Keep the proposal and the sequence conditional
"Proceed with reusable units" sits uneasily with "does not authorize implementation." The numbered sequence also makes step 2 look like a gate for step 3, even though reuse is meant to be independent.

- Change the opening to "If implementation is authorized, pursue reusable units independently of compact-layout success."
- Add to step 2: "A failed compact proof stops capacity work, not unit reuse."
- Make step 4 conditional. Without a capacity path, extracting Real may have little value.
- Add a reuse-side stop condition, because the heading promises stop conditions but only capacity has one. For example: reassess if unit compilation or linking can't fit within Zeal's compiler and Run limits, or if TP3 regression fixtures fail.

## Smaller clarity corrections

6. **Chain timing:** "Reject... before handoff" could mean at compile time or during execution. Say which, or state that the mechanism is undecided. The Chain handoff happens during execution, so this matters to the document's central distinction.
7. **ZVB sequence:** "Recommend shipping ZVB as a bundled... unit" is followed by "Start with a user-authored Pascal unit." Change the second sentence to "Author it as an ordinary Pascal unit, with a small documented native boundary."
8. **Cycles:** Use "reachable recursive routine cycles work" and "reject unit dependency cycles, including through implementation-only `uses`." This matters more now that implementation-only `uses` is included.
9. **Evidence labels:** The `uses`/linker paragraph sits under documented manual findings, but "routines reached through retained address references" may be inferred. Label anything the manuals don't state, as the project's evidence rules require.
10. **Departure rationale:** The Memory-mode paragraph states the change and its cost, but not why. Add a short reason, such as using the same linked behavior for Run and saved output.
11. **Stale or duplicate text:**
    - Remove the second "Unit qualification disambiguates exports."
    - Step 1 says "Agree which old artifacts remain supported," though version-2 readers now have a recommendation.
    - Align "unit/offset" and "unit/routine/offset."
12. **Promise strength and terminology:**
    - In "Screen/CRT and Real will be organized internally," change "will" to "would" so it matches the document's proposal status.
    - In the table, replace "Not needed as runtime library files" with "Not needed during execution."
    - In the ZVB closing sentence, change "at runtime" to "during execution."
13. **Readiness:** Add Chain capability enforcement to the unproven list.

## Technical items for Codex

- **Legacy-default runtime:** Confirm the proposed default keeps the complete runtime. If it doesn't, adjust the wording in item 1.
- **Units in Chain-capable builds:** The Chain section covers compact layout and the runtime contract, but not imported units under the legacy layout. Check whether selected unit data can shift shared globals, and how "initialized once" applies when a parent and CHN import the same unit. A single sentence would suffice, either allowing this with a stated rule or marking it open.
- **Fingerprint behavior:** Confirm whether checking available source on each Run is practical on Zeal. This is an implementation cost to measure, and it doesn't change the recommended wording.

## Final status

- **Creative suitability:** Ready after fixes 1–5. Items 6–13 are quick polish and don't need a structural change.
- **Unresolved after this review:** The item-1 default-build statement needs Codex's confirmation, and Chain behavior for imported units is still unspecified. The other open decisions are correctly presented as outputs of the investigation.
- **Review status:** This completes the second consultation turn. Under the project workflow, any further round would need the user's request.

### Codex reconciliation

Accepted final recommendations 1–5. Confirmed the proposed legacy default
keeps the complete current runtime; qualified omission to unit procedures and
renamed the diagram's intermediate state a dependency proof. Compact exit
criteria now include proven Chain enforcement and explicitly are necessary but
not sufficient. Wider availability/defaults require a measured owner decision.
Aligned manual-source/option fingerprint requirements with the first workflow,
corrected dependent-unit recompilation after interface changes, made implementation
sequence conditional, and added a reuse-side resource/regression stop condition.

Accepted 6–13. Explicitly left compile-time versus runtime rejection enforcement
open, replaced duplicated ZVB authoring prose, distinguished routine call cycles
from unit dependency cycles including private imports, labeled address-taken
reachability as design inference, gave the Run parity rationale, removed repeated
qualification wording, aligned artifact/source-location and execution terminology,
and added Chain enforcement to readiness limits.

Technical reconciliation: legacy fixed-layout behavior is directly visible in
compiler_compile.c, compiler.c and native_image.c. Imported-unit storage and
initialization across parent/CHN images remain explicitly unspecified, including
shared-global offsets and per-activation versus cross-image initialization. This
is a compatibility design gate, not an assertion that the mechanisms compose.
Available-source fingerprint checks on each Run are a proposed cost to measure.
No target execution or implementation proof is claimed.

Both consultation changes.diff files are empty. The final editorial recommendations
were applied by Codex after the second read-only turn, then links, hashes and
whitespace were checked. No third review was requested. Because those wording
changes alter supplied inputs after the last review, the runner's unchanged-input
finalization condition is not satisfied: do not claim a finalized milestone or
an Opus review of the exact final text. The two reports and reconciliations remain
available; the exploration is delivered with implementation questions open.

Remaining technical work: unit format/ABI and imports, compact placement, chain
capability enforcement and unit/CHN composition, native authoring safety, verified
error maps, source lookup costs, exact capacity gain and future default policy.
These are explicitly unimplemented exploration outputs.
