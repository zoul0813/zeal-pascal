# Program execution pages

The native baseline measures four arithmetic programs using the production IDE
at checkpoint 062515f. The library case authors an Integer unit through the IDE,
then imports its compiled artifact. Every program is compiled and run in Memory
mode and saved for a separate cold standalone launch. Native completion,
physical image handles, actual root mappings, and kernel page ownership are
checked. Emulator evidence is bound in `program-pages-baseline.json`.

| Case | File bytes | Code bytes | Global bytes | IDE execution pages | Standalone owned pages |
| --- | ---: | ---: | ---: | ---: | ---: |
| Minimal | 16906 | 522 | 0 | 3 | 3 |
| Integer | 16988 | 602 | 2 | 3 | 3 |
| Real | 17189 | 799 | 6 | 3 | 3 |
| Library | 17159 | 775 | 0 | 3 | 3 |

Every page is 16384 bytes. Each Memory compilation initially retains two image
pages. Run allocates the third window for the stack and execution controls,
retaining all three image pages for repeated Runs. Ten compiler code pages
remain resident while the child runs. Those pages and the IDE's other storage
are additional to the measured child image.

The unmodified OS loader initially gives a standalone process three pages.
Standalone ownership is measured before process cleanup. These arithmetic
cases use no explicit file, module, or dynamic heap operations. Their counts
are entry/exit measurements, not a claim about those other operations' peaks.

The allocation change must preserve runtime/source locations, repeated Run
state, stack/heap bounds, unsupported/native feature behavior, allocation
failure cleanup, and old images. Its measurements will use the same driver.

## Requirement-based allocation

The production compiler selects two physical execution pages for small programs
with bounded memory requirements. The runtime prefix keeps its own page. The
program's second page backs both `$8000–$BFFF` and `$C000–$FFFF`. Static storage
and code use their existing low addresses. The stack and execution controls use
the high alias. The heap floor is translated to the high alias after startup,
so existing stack/heap guards compare addresses within the same physical page.

The selector requires the statics, emitted startup code and a 2048-byte stack
reserve to fit within that page. It uses a checked 128-byte upper bound for its
startup routine. A default End of `$FBFF` gives the fixed startup copy a known
stack location. Explicit alternative End options retain independent pages.
Memory Run owns only two image pages and records the shared high-window mapping
without storing a duplicate page owner in Build. Repeated Runs retain the same
image handles and typed constants. Normal cleanup releases each actual owner
once. The existing three-page layout remains available for larger requirements.

The normal OS loader initially allocates three pages. The emitted startup copies
its live stack and private controls into the spare top of the second page,
changes the third-window mapping, and frees the old third page through the
supported native syscall. It preserves interrupt state and exits with an error
if release fails. Memory Run installs the alias before entry and skips this cold
release. Standalone working ownership can therefore fall to two pages after
loading, while the OS loader's transient allocation remains three pages.

Heap operations and capacity queries, raw/native address access, unbounded local
routine call depth, recursion workspaces and larger temporaries retain three
independent pages. File, console, transfer, keyboard, CRT, interrupt polling, Random and numeric
text formatting/input dependencies also keep the larger layout. Their helper
stack demand is outside this first bounded arithmetic contract. Known leaf-unit calls can use the smaller layout. Per-side
reservations above 512 bytes conservatively request the larger model, leaving
headroom within the 2048-byte shared-page startup reserve.

The unreleased ZPU format uses ABI1 under the `ZPU1` magic. Its per-fragment
requirement byte holds bit0 for Real and bit1 for independent memory windows.
Both bits propagate only from retained fragments. There are no older-ABI readers
or migration paths: this project is unreleased, and the format can evolve in
place until the first release. A runtime fingerprint still prevents mixing a
unit with incompatible private runtime code; malformed files, unknown versions,
unknown flag bits and invalid bounds are rejected. Rebuild experimental unit
artifacts when changing the compiler/runtime.

Public gates, globals addresses and the generated 16 KiB prefix are preserved.
The normal executable header uses flag bit1 to declare the shared stack layout.
Readers validate its size, End and stack bounds before allowing that layout.

The startup routine adds 70 bytes to the four small baseline executables. The
existing private runtime entry dispatch points to the new startup only for
shared images. Program code, typed constants, and explicit machine-code addresses
keep their original placement. Independent-layout files remain byte-identical
to the full-runtime control when they select the full Real engine. This reduces
RAM ownership without compacting the zero-filled gaps in the runtime file image.

## Verified allocation measurements

The four baseline programs now save one physical 16 KiB execution page each:
32768 bytes instead of 49152, a one-third reduction. These counts exclude the
IDE/compiler and any separately allocated module peaks.

| Case | Saved file bytes | IDE execution RAM | Standalone working RAM |
| --- | ---: | ---: | ---: |
| Minimal | 16976 | 2 pages / 32768 bytes | 2 pages / 32768 bytes |
| Integer | 17058 | 2 pages / 32768 bytes | 2 pages / 32768 bytes |
| Real | 17259 | 2 pages / 32768 bytes | 2 pages / 32768 bytes |
| Library | 17229 | 2 pages / 32768 bytes | 2 pages / 32768 bytes |

The 13-case driver verifies two pages for the four primary cases, a pure routine
selected from a unit containing an unused heap-query routine, and persistent
Counter state. Local calls, recursion workspace, selected unit heap queries,
dynamic heap operations, HeapPtr/RecurPtr, raw access and large statics retain
three. RAM mappings, translated guards, real kernel owners, repeated Runs,
post-exit cleanup, repeated cold launches, IFF preservation and failed pfree
startup are checked. A forced Real frontier exhaustion reports FF at the emitted
guard PC, preserves statics and releases the child pages.

The native helper harness passes 777 assertions, including explicit/default Start
cutoffs, decoder acceptance, zero-free-page pressure, safe third-page failure and
retry, save after Memory Run, preserving exec, saved Pascal Execute into a shared
child, and Memory Execute rejection before retiring Build. ReadLn execution and
the complete 17-case optional Real/full-runtime comparison also pass.

The final four-check registered run freezes inputs and passes host sanitizers,
unit-real, the Real matrix and program-pages. Six earlier independent checks
passed: general units (404 assertions), the language corpus, IDE unit authoring,
IDE unit failure/media/cancellation, source-free navigation and standalone CHN.
They predate the final CRT requirement tag; the final helper harness and Real
matrix revalidate CRT behavior. This is not a claim that the entire 76-command
plan ran on the final snapshot. Reports, hashes and capture bindings are recorded
in `program-pages-evidence.json`; the milestone review is
`../reviews/program-pages-v1.md`.

Compiler caching remains 10 code pages cold and 15 warm; it is additional to the
child's execution RAM. The production bank container is 239941 bytes and all
banks fit their 16 KiB windows. No smaller COM or compiler-startup page count is
claimed by this allocation change.

## Helper eligibility

| Capability | Execution pages | Reason |
| --- | ---: | --- |
| Silent Integer / Real arithmetic | 2 when statics fit | Bounded generated temporaries and runtime guard |
| Selected leaf unit | 2 when statics fit | Retained fragment declares its requirements |
| WriteLn / ReadLn | 3 | Console/input helper stack |
| Str / Val | 3 | Numeric formatting/parsing helper stack |
| Random / KeyPressed / CRT / U+ | 3 | Additional runtime helpers or interrupt polling |
| Inline / native addresses / pointers | 3 | Unspecified native memory demand |
| Heap / HeapPtr / RecurPtr / MaxAvail | 3 | Preserve independent heap/stack capacity and pointer meaning |
| Local calls / recursive workspace / large temporaries | 3 | Call-depth or larger workspace demand |

This is a conservative allocation contract, not exact whole-program stack-depth
analysis. It preserves the existing 2048-byte minimum reserve and bounds both
sides of supported leaf-unit calls. Broader helper eligibility can be added only
with corresponding stack-demand evidence.
