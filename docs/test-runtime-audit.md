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
