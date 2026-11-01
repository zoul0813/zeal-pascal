# Compiler simplification integration into main

Prepared October 31, 2026 against main `d3a17d8` (PERF22). The source is the previously tested `compiler-job-investigation` worktree; unrelated dirty test-chunk infrastructure is preserved and excluded from the integration commit.

The integration removes compiler request/reply serialization, wire-table views and protocol-only hash helpers. It retains synchronous compile operations, source identities, authoritative page ownership/lifetime/version checks, final authenticated `build_check`, publication recovery, cancellation, and original source/error-location behavior. Captured-root digest reuse and existing-record source adoption remove repeated hashing and descriptor work. No page consolidation or compatibility-contract change is included.

PERF16 and PERF20 are retired as superseded: the proposed duplicate parent hash traversal and private wire helpers no longer exist. No estimated savings from those tasks are added to measured compiler simplification gains. PERF21 remains deferred: the integration does not solve its eager-error semantics or storage constraints.

PERF22 is preserved by allowing read/fill interrupts without routing them through the slower shared move entry. The separate move entry, inline version updates and overlap comparison remain. The IRQ test finds the retained helper region using the earliest entry address, so all four live probes work with either layout. Successful qualification is recorded below.

## Provenance and verification

`worktree/compiler-job-investigation/analysis/main-integration/` contains the stable main snapshot and dirty status, exact integration diff, build/host/native logs, frozen timing binaries and measurements. The prior full sync passed seven target builds, sanitized host checks, publication/recovery checks and native compiler/editor checks; details remain in the investigation tree's `docs/latest-tree-sync-results.md`. This integration additionally rebuilds the IDE/compiler-job targets and reruns the affected host, IRQ/compiler and timing checks.

## Review limitation

The project-required Opus review was attempted with the `opus-code-review` skill under `main-integration`. It returned “You've hit your 5-hour limit · resets 11:40pm (America/New_York)” and produced no verdict. The prior source-adoption follow-up remains similarly incomplete. The user explicitly requested integration after this review limitation was disclosed; no independent-review completion or workflow finalization is claimed. Recovery must use the same review ID with the runner's documented failed-session procedure. No full release acceptance replay is claimed.

## Final verification

ZDE built `zpascal_bin` and `zjobtest_bin`. The sanitized host suite passed 84,489 front-end/identity/allocation checks, 3,723 editor assertions, 45,809 source-validity checks, 202 key checks, 23 runner tests and six chunk tests. Native compiler acceptance passed **960 assertions, zero failures**, with all four live read/fill/backward/forward IRQ guards and unchanged qualification inputs. Cancellation/typeahead/recovery passed **17/17 checks** at parser/seal checkpoints 144/231.

Two trials per mode for the 96-variable fixture repeated exactly: Memory **2,063,212,783 → 2,063,212,267 cycles**; COM **2,063,289,928 → 2,063,289,412 cycles**. Each saves **516 cycles** versus the frozen pre-integration investigation binary. Image identity/lengths and generated COM/ZMP hashes are unchanged; none of these four measurements slowed down. This is a focused integration comparison, not a new whole-application or startup speed claim, nor a comparison against the older serialized compiler. Raw evidence is in `analysis/main-integration/verification.json` under the investigation worktree.

The commit excludes the pre-existing chunk-runner infrastructure edits, including their CMake and host-runner hunks. Main's integrated production sources match the tested investigation sources. No push was requested or performed. The independent-review limitation above remains.
