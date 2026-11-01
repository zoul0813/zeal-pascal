# Test runtime audit and action plan

October 30, 2026. Product optimization work is on hold while test orchestration
is improved. All previous baseline, candidate and timing processes were stopped.
The interrupted baseline has a real unresolved failure; it is not a passing
reference. No tests or timing trials were executed for this audit/change,
at the user's request. Durations below come from the already captured runs.

## Changes prepared

The supported entry point is `python3 tools/run-tests.py`. Its explicit plan is
`tools/test-plan.json`: **66 default commands, 52 distinct product runners**.
`--list` prints the plan without running tests; `--include-redundant` restores
the three optional duplicate diagnostic invocations, for 69 commands.

Four worker slots are available by default (`--jobs N` changes the budget).
Independent commands overlap; earlier modes of the same runner must finish
first because some host output directories are shared. The first sanitized
host suite finishes before other commands start. It supplies the cancellation
checkpoints. Runtime-layout waits for the parameter suite and consumes its fresh
program, not an old capture. Publication-interruption's existing internal pool
uses at most three slots from the same budget. One orchestrator owns a checkout
at a time; direct individual tests and builds must not run alongside it.

Logs are per command under a unique `analysis/test-runs/` directory. The runner
records statuses and input hashes, reports active commands every minute, and
stops scheduling on the first observed failure. It cancels all other owned
process groups on failure or interruption, including emulator descendants.
`--keep-going` optionally collects independent failures and explicitly skips
their dependents; it never waives a failure. Unstarted and cancelled commands
are recorded. Unexpected leftover processes after a runner exits fail that
command. Signal handlers defer interruption until owned children are registered
and remain active during cleanup, so repeated signals cannot abandon cleanup.
An interrupted, partial or input-mutated run cannot be reported as passed.
There is no cached-pass or resume mechanism.

| Default duplicate removed | Coverage retained by | Saving evidence |
| --- | --- | --- |
| `test-native.py --core-only` | `test-native.py`; its startup identity/mapping authentication and progress observer now also run in full mode | The redundant run had consumed 23 minutes when stopped; its complete duration was not measured |
| `test-chain-call.py` | `test-chain-call.py --ide`, which runs the same compile/execution/cleanup checks before adding IDE checks | One repeated compile/execution/cleanup sequence; time unmeasured |
| `test-compiler-messages.py --publication-error` | `test-compiler-messages.py --native`, which already invokes host, native and publication-error checks | One repeated host + native publication-failure sequence; time unmeasured |

No expected output or product assertion was removed or relaxed. Core-only is
still useful as a focused alternative to the integrated run. Host/native pairs,
RAM/COM execution, Real/trigonometric vectors, bridge corpora, fault injection,
and individual IDE workflows remain distinct coverage. Similar fixture names
alone are not sufficient evidence to delete them.

## Existing measurements

| Completed baseline command | Wall time |
| --- | ---: |
| Sanitized host compiler, key and editor checks | 8.293 s |
| Native compile cancellation | 138.361 s |
| Language corpus | 1,140.813 s (19.0 min) |
| Native editor suite | 689.484 s (11.5 min) |
| Integrated native suite, including failing full-media checks | 2,967.448 s (49.5 min) |

Source: `analysis/performance/baseline/suite.json`. These were not controlled
worker-scaling measurements; concurrent candidate work overlapped some runs.
Do not sum them to claim the time of the complete 66-command plan.

The 69-command reference is that captured baseline driver's manifest, not the
old README command block. It included supplemental tracked runners/modes and
already resolved both parser and sealing cancellation checkpoints from host
output (208/295 at this baseline). The old README listed only `--seal-poll 295`,
although the current cancellation runner requires both options. The supported
orchestrator retains the captured driver's observed-checkpoint behavior instead
of reproducing that incomplete README invocation.

The emulator already runs native headless console/debugger mode without
wall-clock throttling. There is no 10 MHz speed limit to remove. Previous native
timing captures show approximately 6.3 billion cycles before the measured compile
entry and 1.47 billion cycles for a small Memory compile request. Repeated library
loading/authentication and repeated compilations therefore remain expensive even
without GUI pacing. Debugger instruction checks and physical input observation
also cost host time; their individual contribution has not been profiled.

## Further improvements, ordered by likely value

1. **Use bounded parallelism.** Prepared with four default workers and configurable
   `--jobs 6` or higher. Four workers can approach a 4x wall-time reduction only
   for perfectly balanced independent work with sufficient CPU/I/O capacity;
   this is a theoretical ceiling, not a measured result. Startup dependencies,
   shared outputs, internal pools and the longest single suite reduce the gain.
   When test execution is authorized again, compare a representative unchanged
   selection at 1/4/6 workers before increasing the default further. Check
   timeouts under contention as well as total throughput.

2. **Replace fixed keyboard waits with authenticated readiness checkpoints.**
   Some IDE runners request **100,000,000 cycles per character**;
   `type_text` defaults to 250,000. That is a 400x difference in the requested
   per-character budget, not a predicted suite speedup. Existing `DebugConsole`
   readers and physical-line authentication provide patterns to extend. Stop
   only at the correct resident image/reader with an empty keyboard queue and
   valid input mode. Preserve human-input semantics and retries; simply reducing
   cycle constants can drop keys and create misleading passes/failures.

3. **Batch compatible cases inside one native test launch.** Reuse a verified
   compiler session for ordinary independent language/ABI checks; keep fresh
   boot, missing/corrupt library, allocation, startup and ownership tests in
   isolated launches. Each avoided ordinary startup can remove roughly the
   observed 6.3-billion-cycle pre-compile cost, but many current runners already
   batch cases. Count remaining eligible launches before claiming aggregate
   savings. Reset and assert mutable state between cases.

4. **Separate quick developer feedback from full acceptance.** Sanitized host
   checks previously took about eight seconds. Use them and directly relevant
   native runners during development; retain the complete distinct native plan
   for acceptance. This shortens feedback, not the complete acceptance workload.
   It does not waive the full TP3 validation gate for a product optimization.

5. **Cache host build work only, with complete input keys.** Avoid recompiling
   identical host harnesses/assets. Include compiler/flags, source, headers and
   generated runtime assets in the key; continue executing assertions. Likely
   smaller than native savings because the main host suite was already fast.
   Do not cache a test's pass as a substitute for execution after source changes.

6. **Investigate supported emulator build options.** Confirm the actual installed
   binary's optimization flags and debugger costs before proposing changes.
   No evidence currently establishes an unoptimized binary. No emulator source
   patch, ROM modification, authentication bypass or input-memory shortcut is
   included in this work.

## Validation and outstanding work

Static inspection confirms that the old 69-command matrix equals the new
66 default + 3 optional matrix and all 52 tracked product runners remain
represented. Python syntax and preservation of native check expressions and
publication-interruption assertions were inspected without importing/running
any test module. Independent Opus review is required by the repository workflow.
Runtime correctness, parallel isolation, cancellation and actual speed gains
remain unverified until test execution is authorized again.

The existing baseline full-media failure remains open: Compile/Run did not reach
the expected diagnostic checkpoints; Save returned error 22 and the media was
unchanged. An isolated unchanged rerun reproduced the two failed check results.
No failure is waived by deduplication or parallel scheduling. Establish a clean
baseline with the revised plan before resuming PERF01.


## Follow-up: remaining long suites after clean baseline

The complete revised plan now passes 109/109 jobs in 54m44s at six workers; the historical failed/interrupted baseline above is superseded. A read-only investigation identified safe boundaries for math, files, compiler options, publication interruption and text: **42 proposed chunks replacing five commands** (146 scheduled jobs in total, retaining the 66 canonical commands). This is a plan, not implemented support or measured improvement.

Use **eight workers for the next tests**, per the user's instruction. Six-worker slot utilization was already 94.6%; unchanged-duration perfect packing would save at most about 2m58s before added startup. Chunking primarily improves focused reruns and balancing; more worker capacity is needed for a large whole-suite reduction. Preserve stage-dependent RAM/COM coverage, OMAX's layout-size dependency and Text rename-error's pre-existing destination. No tests were run for this investigation.

See [concrete chunk plan and verification requirements](../analysis/performance/full-baseline-monitored/next-chunking-plan.md) and [machine-readable case list](../analysis/performance/full-baseline-monitored/next-chunking-plan.json).


## Implemented remaining long-suite chunks — October 31, 2026

Prior infrastructure was committed as `4938065`. The next plan is now implemented: math 10, files 9, compiler-options 6, publication 12 and text 5 chunks replace five commands. The default full plan has **146 jobs with the same 66 canonical commands**. All 146 passed with **eight workers in 44m09s**, versus 54m44s with six workers: **10m36s saved,19.4%less total wall time**. Worker count and chunking changed together; their separate gains are unmeasured.

Longest individual chunks: math2m58s (previous command11m51s), files3m16s (11m09s), options2m29s (8m35s), publication2m37s (7m36s with 3 internal workers), text2m11s (7m28s). These are focused-command durations, not suite wall times or additive savings. The27 C groups passed 1,677 assertions, normalizing to1456original assertions after removing repeated setup/selectors and Text cwd checks. Original call/stage order, output/disk bytes, RAM/COM, ownership and error locations are retained.29 host tests and the required Opus review passed. No product binaries changed, no inputs changed and no test processes remained.

The legacy66-command monolithic mode has static/host equivalence proof but was not rerun. Historical Git provenance verification explicitly skips if4938065 is unavailable; it ran without skip here. No native assertion or expected disk/output byte was relaxed. Product optimization tasks remain open.

See [full measured report](../analysis/performance/long-chunks-8workers/report.md), [all command timings](../analysis/performance/long-chunks-8workers/command-times.csv), and [authoritative suite](../analysis/test-runs/20261031T033836.687737Z/suite.json). Select cases with `python3 tools/run-tests.py --only math:sqrt-programs --jobs 8`; run the full plan with `python3 tools/run-tests.py --jobs 8`.


## TP3 single-Work correction — November 1, 2026

The IDE's persistent editor source catalog and parked dirty buffers are removed.
The Borland manual (printed page16) and preserved CP/M-80 TP3 executable
`89e65bd28d176dba815dbcf6c49f356e3cba5a9db9454264ab3f87f3dd75c0b7`
establish the actual-file save/reload workflow: save dirty Work before compiling
a different Main, or before an include error replaces it. Repeated automatic
saves replace BAK with the previous source revision. This is not a temporary-file
scheme. Self-contained Work still compiles/runs without saving. Compiler/build
source tables remain; they are not additional editor buffers.

Kernel ownership confirms17 additional startup pages (previously18), or20 total
application pages including the three OS execution pages:16 KiB saved. The IDE
bank shrinks16070 to15099 bytes (-971); file services grow14689 to14716 (+27),
saved-error services13613 to13663 (+50), and resident code13402 to13489 (+87).
Other banks are unchanged. Net linked payload shrinks807 bytes;16 code banks
remain allocated.

The full ZDE build/packages pass. Targeted checks pass: sanitized host compiler,
editor, saved-map and publication/rollback tests; native single-Work transitions
with numeric/explanatory diagnostics and Inline repair; save/load failure safety;
saved-PC lookup; editor replacement pages and exact recovered bytes; U/C
interruption recovery; Execute; directory operations; and compilation cancellation
through sealing. Original executable replay verifies both automatic saves and
resulting file contents. The original artifacts remain hash-identical.

Obsolete editor SHA-field inspection was replaced with revision and actual saved
byte/backup checks. The cancellation observer now qualifies the actual sealing
bank and queues physical input before sealing's final poll, allowing keyboard IRQ
delivery; compile-interruption text, unpublished output, and ownership assertions
remain required. No product instrumentation was added.

This is targeted acceptance, not a rerun of the146-job matrix or physical hardware
qualification. Opus was explicitly excluded by the user. Local hash-bound evidence:
`docs/native-single-work-evidence.json`; original replay:
`analysis/tp3-single-work/qualified-original/report.json`.


TP3 scope follow-up (2026-11-01): the synchronous compiler no longer re-parses
its own complete image/map at handoff or snapshots all parent-page versions.
Bounded allocation ownership, later build freshness and external-reader checks
remain. Saved execution permanently releases its transient Build instead of
reloading a RAM cache. Native fixture ownership expectations now reflect that
policy; generated-map correctness remains independently checked by host/native
readers. A source-readiness bound is captured before iteration to avoid the
observed SDCC advancing-address failure. Scoped host, publication-fault, native
compiler/IRQ,256 option/layout, IDE options and repeated HostFS/TF publication
checks pass. Evidence and failed-run distinctions are recorded in
docs/tp3-scope-followup-2026-11-01.json. True disk-backed generation remains open.


TP3-08/14 completion (2026-11-01): retired multi-buffer compiler parameters,
input-catalog allocation and unsaved include overrides. Fixtures now exercise
the single-root/disk-include contract; obsolete override cases are removed.
Compiler-only math excludes formatting/Val/transcendentals and unused runtime
operations while using the existing conversion primitives. Full generated
runtime bytes remain identical. Scoped host checks and native math471/0,
compiler/IRQ944/0, includes65/0 and IDE error-repair/disk-Main workflows pass.
The source-count and saved-cache observers were updated to the intentionally
reduced fixture scope and prior cache-removal behavior, retaining failed
original captures. See docs/tp3-scope-8-14-evidence.json.
