# Zeal Pascal

A native Pascal IDE, compiler and runtime for the MMU Zeal 8-bit Computer.
This is an incremental implementation of the recovered Turbo Pascal 3 dialect;
full language, editor and runtime conformance is still in progress.

The CP/M-80 Turbo Pascal 3 language and IDE workflows now have bounded native
acceptance. Next is performance work, then the remaining Turbo Pascal
runtime/library services. Overlay support remains deferred.

Explore [six self-contained applications](examples/README.md): a text adventure,
persistent stock database, loan calculator, concordance, Life simulation and
heap-backed task manager. Each includes a standalone `.bin`, source, run
instructions and a short demo.

Build using the existing Zeal development environment:

```sh
zde cmake
python3 tools/test-native-host.py --sanitize
python3 tools/test-language-conformance.py
python3 tools/test-editor-native.py
python3 tools/test-native.py
python3 tools/test-native.py --core-only
python3 tools/test-input.py
python3 tools/test-io.py
python3 tools/test-files.py
python3 tools/test-text.py
python3 tools/test-standard.py
python3 tools/test-keyboard.py
python3 tools/test-logical.py
python3 tools/test-console.py
python3 tools/test-control.py
python3 tools/test-devices.py
python3 tools/test-hooks.py
python3 tools/test-language.py
python3 tools/test-sources.py
python3 tools/test-saved-map.py
python3 tools/test-saved-map.py --native
python3 tools/test-ide-sources.py
python3 tools/test-compiler-job.py
python3 tools/test-compiler-messages.py --native
python3 tools/test-ide-execute.py
python3 tools/test-ide-directory.py
python3 tools/test-inline.py
python3 tools/test-ide-sources.py --inline
python3 tools/test-ide-interrupt.py
python3 tools/test-compile-interrupt.py --seal-poll 295
python3 tools/test-options.py
python3 tools/test-options.py --ide
python3 tools/test-chain.py
python3 tools/test-chain-call.py --ide
python3 tools/test-chain-call.py --execute
python3 tools/test-chain-loader.py
python3 tools/test-chain-loader.py --native
python3 tools/test-execute-override.py
python3 tools/test-execute-handoff.py
python3 tools/test-compiled-startup.py
python3 tools/test-transfer.py
python3 tools/test-transfer.py --native
python3 tools/test-runtime-profile.py
python3 tools/test-chain-files.py
python3 tools/test-chain-files.py --native
python3 tools/test-bridges.py
python3 tools/test-bridges.py --corpus routines
python3 tools/test-bridges.py --corpus real
python3 tools/test-file-runtime.py --host --native
python3 tools/test-file-gate.py
python3 tools/test-workers.py
python3 tools/test-params.py
python3 tools/test-math.py
python3 tools/test-output.py
python3 tools/test-output-host.py
python3 tools/test-members.py
python3 tools/test-output-interrupt.py
python3 tools/test-startup-path.py
python3 tools/test-real.py --host --sanitize --native
python3 tools/test-real.py --trig --sanitize --native
```

The language conformance runner requires the preserved local reference corpus
and its recorded original captures. Research stays outside Git; missing inputs
fail validation. It compares native RAM and saved-program output, compile-error
numbers, source positions and filenames, with explicit native adaptations.
`--prepare-only` lists the corpus without running the emulator. The core-only
regression command runs the existing compiler/runtime corpus without repeating
the separate IDE/platform workflows.

Deploy the matching `build/zpascal.bin` and `build/zpascal.zpl` from one build.
Run the BIN from ZShell on the supported video/PS/2 target; its default library
path is `T:/lib/zpascal.zpl`. Create that installation directory and copy the
ZPL there with the normal shell utilities. To select another library path:

```text
H:/zpascal.bin H:/zpascal.zpl
```

For a separate project TF image with the matching executable and library already
installed, run this after building:

```sh
python3 tools/package-tf.py
```

Select `build/tf.img` as the emulator's TF card, then run
`T:/bin/zpascal.bin`. The image contains `T:/lib/zpascal.zpl` and is checked by
starting the IDE without a library-path argument. Initial creation uses the
development environment's base image; later refreshes retain the card's other
files and verify their contents. Run the packaging command again
after rebuilding; it does not modify your existing system TF image.
An existing project `build/tf.img` is preserved under `build/recovery/` before
replacement, including any source files saved on that card.
`zde cmake` builds the loose files; refreshing this image is a separate step.
Close the emulator using the project card before refreshing it.

The launcher verifies the complete library before entering any application
bank. A missing, damaged or mismatched library returns to the shell. The editor,
compiler, file operations and compiler front end use separate owned RAM banks.
Routine, expression and type compilation use additional code banks.
Compiler phase banks are released while a program runs. The IDE retains the
original library handle and authenticates reloaded phase bytes before compiling
again, so the library must remain available while using the IDE. A reload error
retains the source and reports that the compiler library is unavailable.
The runtime template occupies a verified library bank; the RAM trampoline
remains in the launcher. Internal `pascal_runtime.zpl` and `ram_trampoline.zpl`
build products require no separate deployment. Generated Pascal COM
programs contain their own runtime and need no IDE library.

Startup is currently slow: allow roughly 2½ minutes at the target's normal
speed for library loading and verification before the IDE prompt appears.

At the startup message prompt, Y enables compiler explanations and N keeps
numeric diagnostics. Missing explanations still leave the error number usable.
The menu retains the last successful compilation's Main line count and code/data
sizes. It reports owned RAM pages; free memory and disk capacity are explicitly
unavailable because the OS provides no capacity query.

| Key | Operation |
| --- | --- |
| L | Select an available native drive root; empty keeps the current directory |
| D | List a native directory or filename mask; empty lists the current directory |
| W | Open/select a work file by name; default extension PAS |
| M | Set Main, the compilation root; an empty field uses Work |
| E | Edit; Ctrl-K,D returns to the menu with text retained in RAM |
| S | Save source through verified temporary/recovery copies |
| A | Save current source under another filename; Escape cancels |
| C | Compile in the selected output mode |
| R | Run the current build, compiling first when needed |
| X | Execute an external native program and return to the IDE |
| O | Select M Memory, C native Com-file or H cHn-file; saved modes expose S Start/E End; F finds a saved runtime error; Q returns |
| Q | Quit, resolving unsaved source with save/discard/cancel |

Startup opens `T:/src/pascal/UNTITLED.pas` when available. Missing source or
parent directories start an empty RAM buffer silently. W accepts native absolute
or relative paths, including a new filename under missing parents. Loading
creates no directories. S and native COM/CHN publication create missing parents
recursively when saving. A parent that is an existing file is an error; failed
loads/saves retain the previous RAM buffer.

To change the filename after editing, press Ctrl-K,D to return to the menu,
then A, enter a path such as `T:/untitled.pas`, and press Enter. Save as keeps
the current text and cursor. A verified successful save adopts the new path;
failure or cancellation retains the old path and RAM contents. W opens a file
and should not be used to rename unsaved source. Changing the source path
invalidates an existing build so its source map cannot retain the old filename.

Source includes accept `{$I parts/name.inc}` (also `(*$I ...*)`), on a
line of their own. Relative paths use the containing source's directory; a
missing extension defaults to PAS, and native filename case is preserved.
One include level is supported, as in the recovered dialect; nested and
self-inclusion are rejected. Repeated includes share one source identity.

Main stays the compilation root when an error selects an included Work file.
Compile and direct-run errors verify source identity before opening the actual
file and positioning its cursor. Automatic navigation retains unsaved buffers;
repairing an include in RAM and pressing R recompiles Main with those edits.
W can restore a parked dirty buffer. Its dirty-current prompt still offers
save/discard/cancel; N explicitly discards that current buffer on successful
replacement. Quit resolves every retained dirty buffer. Compilation never
silently saves these sources. Saved COM/CHN and ZMP output names follow Main when set.

Saved programs report runtime and I/O errors with a hexadecimal PC. Select the
program's Main or Work source, choose O then F, and enter that PC (one to four
hexadecimal digits). The IDE verifies the saved COM/ZMP or CHN/ZMP pair for the
selected output mode and all source identities, then Escape opens the actual
source or include at the error position.
This works in a fresh session without compiling. Matching unsaved buffers take
precedence over disk; changed, missing or invalid artifacts/sources reject lookup
and retain edits. Overlay lookup remains unsupported. Recompile older programs
with includes to replace their incorrectly encoded source-map headers.

In Com-file mode, compiling the default source writes
`T:/src/pascal/UNTITLED.COM` and `T:/src/pascal/UNTITLED.ZMP`.
Run `T:/src/pascal/UNTITLED.COM` from ZShell. These are native Zeal Z80
executables. Source must be saved separately with S.
Saved runs release their cached image/map pages while the child runs, then reload
and authenticate that cache. Source buffers stay in RAM. A failed or
unauthenticated cache reload invalidates the cache; the next R recompiles.

H cHn-file mode writes a compact `.CHN` code payload and its `.ZMP` source map.
It omits the ordinary executable's runtime, globals and placement gaps. It uses
the same verified save/recovery protocol. CHN files require a compatible caller;
they cannot run directly from ZShell or R. A saved Pascal program can activate
one with `Assign(next,'T:/NEXT.CHN'); Chain(next)`, where `next` is a File variable.
Compile the caller in Com-file mode and the replacement in cHn-file mode with
the same explicit Start and End addresses. Matching initial global declarations
share their storage and open-file position. Successful Chain never returns to
the caller. Failed loads support `$I-`/IOResult; Memory mode reports I/O21.
R explains the required caller while retaining the compiled CHN file.
CHN source lookup through O,F is available. Switching output kinds
invalidates the cached build; it does not save source.

Saved programs can replace themselves with another compiled Pascal program:
`Assign(next,'T:/NEXT.COM'); Execute(next)`. Success never returns to the caller.
The replacement uses its own runtime, globals and Start/End settings. Memory
mode reports I/O21. `$I-`/IOResult handles ordinary load failures. Incompatible
images are rejected before closing files; an attempted OS replacement closes
ordinary files but retains their filename associations if it fails, so Reset
can reopen them. Replacement targets require the current sealed startup; rebuild
older experimental COM files. Executing a saved caller through R returns to the
IDE after the replacement exits, preserving source and the authenticated cache.

The staged loader also requires an exact private-runtime fingerprint. CHN entry
code occupies96 bytes, followed by32 fingerprint bytes that execution jumps
over. The fingerprint binds the runtime prefix, private helper addresses, file
module/gate and transfer module/gate; public ABI1 alone cannot establish their compatibility.
Older experimental CHN files must be recompiled. Rejection leaves caller code,
globals and heap unchanged.

The compiler embeds one packed transfer coordinator, gate and acquisition helper
when a program uses Chain or Execute. The coordinator expands into its owned page
when first needed and is shared across Chain replacements. Saved programs do
not need separate installed runtime libraries. Existing file support and the
transfer assets leave limited room for caller code within the generated-program
size limit. The runtime terminal path can retain/retry coordinator, staged-page
and file-module owners. Native acceptance includes an unmodified saved Pascal
caller; the older test-bootstrap fixture remains separate component evidence.

File-module acquisition and Chain/Execute share one private decoder per generated
program. Both modules are stored as checked packed constants, expanded only into
their owned pages. Packaging verifies exact linked module/template bytes before
publishing the IDE, and their identities are part of the private CHN fingerprint.
The overlay compiler phase occupies an additional bank; it is parked during
Run/Execute to retain the previous thirteen-bank execution memory budget.
This does not enable Pascal overlay compilation yet.

Ctrl-U cancels compilation, including image preparation, scanning, code
placement and final hashing. It also cancels source, Save As, block and executable/
map publication between transfer chunks. Escape acknowledges the interruption;
source edits and queued typing remain available. Filling the 32-key IDE typeahead
queue also stops the operation instead of dropping ordinary keys. Incomplete
builds are released, and the next R compiles again.

If final publication has begun, the IDE attempts to restore the previous files
from its verified recovery copies. Restoration runs to completion without further
keyboard cancellation. A failed or canceled save retains its transaction directory
and reports the recovery path; a retry uses a new directory. Source dirty state
and Save As identity change only after a verified successful save. Filesystem
calls remain synchronous, and this copy-based protocol cannot promise atomic
replacement or protection against power loss.
Record a reported recovery path before starting another save or compile, which
reuses the path display. The old recovery files remain on disk. If cleanup of a
pending compiler reservation fails, that compile reports the failure; retrying
can proceed after the reservation is released.

In Com-file and cHn-file options, S selects the generated code's Start address
and E selects the program's End address. Enter one to four hexadecimal digits; Start 0 selects
the automatic minimum. Empty input, Escape or Ctrl-U retains the current value.
Invalid input retains the old value and asks again. End defaults to FBFF and must
be in CFFF..FBFF with enough room for the program and at least 2 KiB of stack.
The menu shows bounds for the last compiled program; before a successful compile
it shows platform bounds and checks the complete program when compiling.
Changing an option invalidates the cached build. Memory mode uses automatic Start
and FBFF independently of saved-mode settings. Gaps before code are zero-filled.
Ordinary executable startup sets StackPtr to End+1 and validates the actual
static/stack extents.

X accepts an absolute or relative native executable path without adding an
extension. Ordinary Zeal applications need no Pascal header. On return it
restores the IDE text screen and keyboard mode, reports the native exit status,
and retains Work, Main, dirty buffers, editor state and cached compilation.
Loader failures retain the session and show the native error. An empty name,
Escape or Ctrl-U cancels. X supplies no command-tail arguments.

The menu shows the current native directory. L accepts a drive letter such as
`T`, `T:` or `T:/`; selected Work/Main files retain their absolute paths when
the logged drive changes. Failed selections retain the previous directory.

D accepts an existing directory or a final filename mask, for example
`T:/src/pascal/` or `T:/src/pascal/*.pas`. Masks retain native case: `*` matches
any sequence of bytes and `?` matches one byte. Directories end in `/` in the
listing. Space or Enter continues a long listing; Escape or Ctrl-U stops it.
Directory operations create no files or paths. The footer says `Free space
unavailable` because the supported SDK provides no free-capacity query.

Source saves retain the previous source as BAK. Replacing an existing BAK
requires confirmation. Source and executable/map publication stage and verify
new content and old recovery copies before replacing any destination. After all
final files are closed and verified, the operation removes its own transaction
files. Failed saves keep recovery copies and identify their directory. Existing
unrelated recovery folders are never removed automatically. The OS has no atomic
rename/fsync guarantee. No save failure discards the RAM source or reports success.
Verified replacements remove and recreate the destination using normal file
operations. This avoids the pinned HostFS truncate/write return-value problem
without changing the OS or emulator; recovery copies remain until verification.

On the pinned machine, B: is a 64 KiB EEPROM volume; T: is the TF card. COM
output currently occupies about 50 KiB in total while replacing a small existing
program, before source/map overhead: the 16 KiB runtime prefix is present in
the old, staged and recovery images. Native error 22 during publication indicates
no free space/resources. Old builds left transaction folders after successful
saves; those existing folders may still occupy space. Memory mode needs no disk
space: O, M, Q returns to the menu, then R compiles/runs from RAM.

A separate observed Zeal OS defect can corrupt control flow if the disk fills
while a file is growing. The OS remains unchanged at the user's instruction. Tests distinguish
an already-full volume from this failing mid-write exhaustion case.

The previous native emulator rejected TF writes with error 1, also reproduced
by standard `mkdir`/`cp`. The user installed an emulator with corrected SPI
timing; both `/opt/homebrew/bin/zeal-native` and the current development build
now pass the TF write checks, including `T:/untitled.pas`. Historical failure
captures are preserved. This project has not patched ZOS or the emulator.

Current language support includes Integer, Byte, Char, Boolean, Real and `string[1..255]`,
compound statements, assignments, Integer arithmetic/bitwise/shift expressions,
eager Boolean logic, ordinal/string comparisons, If/Then/Else, While/Do,
Repeat/Until, For/To/Downto, Case/Of/Else and same-block Goto labels. Case supports ordinal
selectors, comma lists, inclusive ranges and nested alternatives.
For controls support the implemented ordinal families;
the final bound is evaluated once, zero trips retain the old control value and
normal completion leaves the final bound. Write/Writeln accepts strings,
Integers, Characters, Booleans and Reals, including formatted fields. Read/ReadLn
accept Integer, Byte, Integer-subrange, Real, Char and String targets, including
selected fields, array elements, pointers and Var parameters. Short-string assignment
truncates to the destination capacity. Memory execution preserves IDE pages and
source; checked errors return to their source statement after Escape.

Typed files (`file of T`) support Assign, Reset, Rewrite, Close, Read, Write,
Seek, FilePos, FileSize, Eof, Flush and Erase. Untyped files support 128-byte
BlockRead/BlockWrite transfers, including the optional completed-block count.
Assign binds a disk filename without creating directories. Typed files retain the four-byte component-count/
component-size header. Zero-size components report I/O error 90 before creating
or truncating a file. `$I-` and IOResult support recovery from ordinary I/O errors.

File bindings close automatically when recursive routines return, when their
heap objects are disposed or released, and when a program ends, halts or fails.
Checked cleanup errors return to the ending scope or cleanup statement; a later
cleanup failure preserves an earlier runtime error. Saved file programs include
their disk runtime and need no separately installed file-runtime libraries.
Disk Text files support formatted Write/Writeln, Char/String/Integer/Byte/Real
Read/ReadLn, Eof, Eoln, SeekEof and SeekEoln. Text output uses CR/LF and a final
Ctrl-Z; input accepts physical EOF as well. Eof/Eoln inspect the next byte;
the seek predicates consume their specified whitespace. Numeric reads use the
same conversion rules as console input and Val. Nested file operations in
argument expressions retain the enclosing operation's file and value snapshots.
Assign accepts native relative names, current-disk absolute names and drive-qualified
absolute names. It resolves the name once against the current native directory,
normalizes repeated separators and `.`/`..`, and preserves filename case.
Changing the current directory later does not retarget the association. Data
filenames receive no implicit extension. Invalid names retain the previous
association; drive-relative names such as `T:FILE` are rejected. Names are
limited to127 bytes, with at most15 bytes in each component.
Rename accepts a closed assigned file and rejects an existing destination.
It copies the exact physical bytes, verifies the complete copy and successful
closes, then removes the old name and updates the association. A failure retains
the old association and may leave a partial or complete destination copy for
inspection; remove that copy before retrying the same name. This uses
copy/verify/remove through Zeal OS's supported APIs and is not atomic.
Input and Output are preassigned Text variables for explicit Read/ReadLn and
Write/WriteLn, including through Var Text parameters. They share the existing
console input buffer, conversion and formatting rules. Explicit Input retains
unread data across calls; ReadLn(Input) consumes the current line through LF.
Input and Output share one current-byte cache. Ordinary buffered lines retain
CR/LF: Char reads can consume both bytes, String reads stop before CR, and
numeric reads skip the line endings to continue on the following physical line.
The B directive continues to control omitted-Input Read calls. Assign, Reset,
Rewrite and Close on these preassigned variables report I/O22. Copied tokens gain no standard-stream
authority, and program cleanup leaves inherited console descriptors open.
Console Eof/Eoln/SeekEof/SeekEoln support omitted Input, explicit Input and
Var Text aliases. They read a line when needed and inspect its unread tail.
SeekEoln skips spaces/tabs within a line; SeekEof also skips line endings.
Predicate-observed ordinary EOL yields CR to the next character read. The B+
directive still starts a fresh line for omitted Read/ReadLn; B- uses the primed
line. The logical devices are described below; replacement input drivers and
the complete file-error/capacity matrix remain in progress. File support does not
establish full Turbo Pascal conformance.

Generated code and static storage can occupy up to29,696 bytes above the fixed
16 KiB runtime prefix, leaving at least2 KiB of initial stack space. The compiler
uses an additional temporary page only when code staging exceeds16 KiB and
releases it before execution. Source remains limited to16,383 bytes and ordinary
globals to less than16 KiB; stack/heap checks still apply to actual program use.

Compiler source catalogs share unused root-snapshot space when they fit,
releasing one physical page during compilation and before Run. Canonical source
bytes, hashes and error mappings are retained. Larger sources use a separate
catalog page. Available physical RAM still limits execution and file-registry
capacity.

Declarations may repeat in any order. Ordinary constants alias literal/constant
values; scalar and string typed constants are initialized writable storage.
Named types, aliases, enumerations and ordinal subranges are supported. Byte
and Integer share arithmetic compatibility; distinct enums retain their type
identity. `$R+` enables ordinal bounds checks and `$R-` disables them.
Char/string conversions and comparisons are supported; converting a string
whose length is not one to Char reports runtime error 10.

Implemented functions include Ord, Chr, Succ, Pred, SizeOf, Addr, UpCase, Length,
Odd, Hi, Lo, Swap, Abs, Sqr, Int, Frac, Trunc, Round, ParamCount and ParamStr.
Saved programs can read shell arguments with `ParamCount` and `ParamStr(N)`.
Spaces and tabs separate parameters; case and literal quote bytes are retained.
Index zero, negative indexes and indexes beyond the parameter count return an
empty string. RAM runs receive no arguments. Command tails allow up to 127 bytes;
longer tails are rejected before the Pascal program starts. Ord, Succ and Pred accept
String/Char-array values of exactly one character; other lengths report runtime
error 10. The compiler currently has
512 symbol entries (seventeen predeclared for HeapPtr/RecurPtr/BufLen/CBreak,
Input/Output/Kbd/Con/Trm/Lst/Aux/Usr and the five output/status pointers) and
64 type descriptors (including six built-in types).
Exhaustion reports resource error 98. Arrays support ordinal bounds, nested or
comma-separated dimensions, packed layout, whole-array assignment, typed
initializers and value/Var parameters. R+ checks each index before access.
A value parameter passes a source address; the callee makes an independent
copy before its body runs. Array- and record-valued user functions remain invalid.

`$X+` (default) selects fast array-index scaling; `$X-` selects the smaller
encoding. Both preserve bounds checks, signed lower bounds and expression
evaluation order. The switch accepts either directive-comment spelling and
comma-separated options.

Records support tightly stored fields, nested arrays/records, whole-record
copies, value/Var parameters and writable typed initializers. Variant alternatives
share storage sized for the largest alternative; tags remain programmer-managed.
Initializer fields follow a declaration-order prefix, with omitted trailing bytes
zero-filled. Empty records occupy zero bytes; zero-width copies leave neighboring
storage unchanged.

With evaluates record bases once, gives the last record lookup precedence and
restores its context after normal completion or Exit. W0..W9 selects the per-block
depth allowance before the statement part (default four). Fields also work with
Addr, SizeOf, selected Var arguments and string ReadLn.

Sets support ordinal bases within 0..255, typed initializers, constructors with
ranges, union/intersection/difference, equality/inequality, subset/superset and
membership. Variables store only their declared byte range; expressions and
value parameters use full 32-byte snapshots. Var set parameters require the same
declared type. Runtime constructor bounds use their low eight bits, while
membership outside 0..255 is false. Typed initializer elements must fit their
base. Assignment retains whole edge bytes, matching the original behavior.

Typed pointers support forward targets within a declaration group, `nil`,
assignment, equality/inequality and repeated dereferencing. Selected record,
array, string and set storage works through pointers, including With, Var,
Addr and SizeOf. Pointers to the same target type are compatible, including
Var parameters. Ptr converts an Integer address; Ord returns its address bits.
Pointer-valued user functions use the scalar result convention; assign their
result to a pointer variable before dereferencing. Pointer constants and pointer
type casts remain rejected as in the reference. New/GetMem allocate owned program memory; Mark/Release reclaims a saved frontier,
and Dispose/FreeMem returns individual blocks. Use one reclamation regime per
heap. FreeMem requires the original requested byte count; disposed pointers keep
their dangling numeric value. MemAvail reports total usable free payload and
MaxAvail the largest payload, excluding block headers. Native counts depend on
the current stack and metadata; odd requests round up and zero-width allocations
still consume a header. HeapPtr and RecurPtr expose the frontier and conservative
stack boundary. StackPtr reads the CPU stack; assigning a saved value restores it
within the owned range. Invalid allocation/stack bounds report FF and return to
source. Direct memory-state manipulation with live frames/allocations is unsafe.

Absolute variables provide a typed view of an Integer logical address or another
variable/parameter's first byte. They allocate no storage, including in recursive
frames, and can refer to writable typed constants or other aliases. Numeric
addresses are native Zeal addresses. A declaration cannot alias its unfinished
self without an older lexical target.

Move copies raw selected storage safely in either overlap direction. FillChar
sets raw bytes from an ordinal's low byte, independently of R, or a String value
of exactly one character. Counts are Integer byte counts; zero copies perform no
payload access after evaluating their arguments. Mem[index] is addressable Byte
storage, usable with Var, Addr, SizeOf and the block operations. Mem assignments
apply Byte/R+ checks. Port[index] is an I/O operation rather than addressable
storage: its Integer index supplies the full16-bit Z80 I/O address and writes
use the Integer value's low byte. Upper address bits matter for Zeal MMU reads;
for example, $80F2 selects the page2 mapping. Port data is8-bit. These raw APIs
do not translate CP/M addresses or provide protection from arbitrary memory or
hardware access.

An Integer-indexed Char array of at most 255 elements acts as a fixed-length
string in expressions, comparisons and string APIs. Assigning to a Char array
requires an exact-length string constant or the same declared array type;
computed strings and string variables are rejected. Other ordinal-indexed
Char arrays remain ordinary arrays.

Procedures and functions support named parameters, local declarations,
lexical nesting/shadowing, forward declarations, Exit and native external
addresses. Value arguments are evaluated left to right; scalar and string values
are copied, while Var parameters alias their callers. `$A+` uses static local
storage; `$A-` gives recursive frames with lexical-parent links. `$V+` checks
Var-string capacities and `$V-` permits relaxed matching. Untyped Var parameters
support forwarding and Addr. Functions return scalars or named short-string
types. String results can be nested, compared, printed, assigned and passed as
values. Concatenation accepts String/Char operands; a result over 255 bytes
reports runtime error 10. Length reports the current string length. String
selections read or write a Char at indices 0..declared capacity; index 0 holds
the length byte. With R+,
invalid indices report error 90. A selected Char can be passed by Var, and
SizeOf checks its type without evaluating the index expression.

Copy and Delete clip at the current end; Pos returns the first matching position
or zero, including for an empty pattern. Concat joins String/Char arguments in
order and reports error 10 above 255 bytes. Insert snapshots its source before
other arguments and truncates the result to the destination capacity. Copy,
Delete and Insert require positions 1..255; negative Copy/Delete counts report
error 11. Remaining runtime APIs are still in progress.

Addr now supports nested routines through native Pascal ABI bridges, including
captured recursive locals, forward declarations, value/Var arguments, Real
results and short-string results up to255 bytes. A captured address is stable
within its declaring activation; its private22-byte thunk lives in that frame
and becomes invalid when the owner returns. Mark/Release does not reclaim it.
All-static lexical ancestry uses a program-lifetime bridge with no captured
frame; static routines inside recursive ancestry still retain the live lexical
chain. Top-level routine addresses remain ordinary native entries. Output/status
driver pointers use these bridges as described below.
Replacement input pointers and ErrorPtr remain pending; no procedural-parameter
syntax is added.

The [control-flow fixture](tests/native/control.pas),
[ordinal fixture](tests/native/ordinals.pas), [Case fixture](tests/native/cases.pas)
and enum boundary fixtures exercise these features in Memory and saved native
COM execution. The [routine fixture](tests/native/routines.pas) covers calls,
recursion and string parameter copies. The string selection/operation fixtures
cover index checks, aliasing, byte values and cleanup. The string result and
boundary fixtures cover temporary cleanup and 255-byte values. The Real fixtures
cover six-byte storage, decimal literals, constants/Pi, initializers, Integer
promotion, arithmetic/comparisons, selected storage, value/Var arguments and
lexical/recursive functions. Real function results use the native BCDEHL register
convention, checked against an independently assembled external routine. Arithmetic
uses the recovered Real6 rules, including truncation and canonical underflow.
Real output defaults to an 18-character scientific field. `value:width` selects
a minimum field width; negative widths add no padding and widths above255 use255.
For Real values, `value:width:decimals` selects fixed point for0..24 decimals;
other decimal counts select scientific notation. Fields retain their values
before evaluating width/precision functions, and nested output remains in order.
Str uses the same Integer/Real formatting and copies the prefix that fits its
String destination, including selected fields and Var parameters. For example:

```pascal
WriteLn(price:10:2);
Str(count:6, text);
```

Val converts String/Char/compatible Char-array input into Integer or Real and
sets an Integer Code: zero on success, otherwise the one-based error position.
Selected destinations and Var parameters are supported. Empty input succeeds
without changing the value. Integer destinations and Code accept two-byte
Integer-family types, including wide subranges. The native implementation parses
the complete string (up to 255 bytes), treats embedded NUL as invalid input,
and correctly accepts -32768; it does not silently accept a truncated prefix.

Sqrt accepts Integer or Real and returns Real. Positive square roots use an
exact integer algorithm with an error below one final Real6 step throughout
the input range. Zero returns zero; negative inputs report runtime03 and
return to their source statement in Memory mode. Ln and Exp use wider private
intermediates while ordinary Real arithmetic retains its dialect behavior.
Ln requires a positive argument (runtime04 otherwise); Exp reports overflow01
and flushes results below the minimum positive Real to zero. Both return Real
and accept Integer or Real arguments. Sin/Cos use radian arguments with full
Real6-range argument reduction; ArcTan returns the principal radian angle.
Their private calculations retain enough precision for the specified accuracy
and correctly handle the minimum-magnitude input.

Random without arguments returns Real in [0,1). Random(N) returns Integer in
[0,N) for a positive Integer bound; zero/negative bounds report runtime91 even
with R-. Randomize mixes a varying native timer seed into the generator.
Each program activation starts with its own generator state. No particular
sequence or cryptographic quality is promised. The helper is linked into the
program, with no extra runtime library to install.

Labels may be identifiers or decimal digit names. Numeric labels retain their
exact spelling, so 01 and 1 are different names; their digits are not limited to
Integer values. Forward and backward Goto transfers stay within the current routine
block. Jumps may leave For/With scopes while releasing their owned stack state;
entering a deeper For reports 71. An unused label need not have a definition.
Transfers into an unevaluated With base retain the original undefined behavior.

Console Read/ReadLn uses a bounded line editor over Zeal's raw keyboard API
and restores the IDE or shell keyboard mode before returning. Numeric fields skip leading whitespace and consume at most
30 characters, leaving the delimiter and any following characters for later
fields. Invalid numbers report I/O error 10 and return to source; numeric fields
already at the fresh-line Ctrl-Z marker retain their values. Byte stores truncate
and input does not add subrange checks. A Char read at the end of a fresh `$B+`
line returns Ctrl-Z (`#26`); consuming that marker permits a later field to refill
the buffer. Numeric reads and bare Read retain the marker; Char consumes it,
ReadLn ends the line, and a fresh read replaces it. Ordinary `$B-` and explicit Input lines retain CR/LF instead.
`$B+` (default) selects a fresh console line for each call; `$B-` retains its unread
tail between calls. Bare `Read;` fills the buffer without consuming a value, and ReadLn discards the remaining line.

BufLen defaults to127, limits the next physical input line to0..127 characters,
then resets to127. Larger values report runtime91 before reading. Excess input
is ignored; Backspace/DEL, Ctrl-H and Delete make room for replacement text.
Escape/Ctrl-X clear the line. Ctrl-D recalls one previous-line character and
Ctrl-R recalls the remaining previous line, preserving any typed prefix.
CBreak defaults toTrue: Ctrl-C terminates the program and Escape returns to the
input statement in the unsaved source. With CBreak=False, Ctrl-C is ignored.
This applies to buffered console input and immediate Trm input; Kbd still
provides raw Ctrl-C as character3.
Ctrl-Z ends console input. Echo uses the video console separately from Pascal
Output, so redirected program output does not collect typed input.
`$I+` is the default and returns checked I/O errors to their source statement.
`$I-` permits recovery: `IOResult` returns the pending error as an Integer and
clears it; a second query returns zero. Pending errors suppress further transfers
while argument expressions still run. Retrieve the status before reporting it:

```pascal
{$I-} ReadLn(N); Code := IOResult; {$I+}
if Code <> 0 then WriteLn('Invalid input');
```

This applies to console I/O and the implemented disk operations, including Text.
Unchecked I/O does not disable division, range or stack errors. Str and Val
preserve pending status under the native scoped-I/O correction. Explicit Input
and Output and their console predicates use the same scoped error handling.
KeyPressed checks for keyboard input without waiting for Enter. Repeated checks
retain the pending character for Read/ReadLn; unread text already in Input's
line buffer does not count as a new physical key. Polling preserves the shared
modifier/release state and restores the normal IDE/shell keyboard mode.
Unavailable keyboard input reports I/O20.

Kbd and Text files assigned to `KBD:` provide unechoed raw input, including
through Var Text parameters. Character reads return without Enter. Strings stop
at capacity, CR or Ctrl-Z; numeric fields retain their delimiter and use the same
conversion rules as disk Text and Val. Eof/Eoln/SeekEof/SeekEoln may fetch a
character and retain it in that binding's own cache. A character read consumes
even Ctrl-Z, so raw EOF does not permanently close the keyboard. ReadLn stops at
CR without waiting for another character or consuming a following LF. Physical
Enter/Escape/Delete produce CR/ESC/DEL; Ctrl-J remains LF and raw Ctrl-C is byte3.
Assign accepts `KBD:` case-insensitively without a disk path or directory.
Reset/Rewrite clear the binding's lookahead; Close on an assigned logical file
is a no-op. Kbd is input-only (I/O3 on writes); Assign/Reset/Rewrite/Close on the
preassigned Kbd report I/O22.

Con and Text assigned to `CON:` use the shared buffered console line source,
with a separate current-byte cache for each binding. They support Char/String/
numeric Read/ReadLn, Eof/Eoln/SeekEof/SeekEoln and Var Text aliases. Cached bytes
survive another binding loading a new physical line. Con ReadLn consumes CR and
an LF already present in the buffer, without fetching another physical key; it
also handles a CR consumed by an earlier Char read. Consuming Con's Ctrl-Z with
Char permits another line; primary Input retains its existing sticky EOF rule.

Trm and Text assigned to `TRM:` read immediately without Enter. They use the
same raw decoder, typeahead, conversions and predicates as Kbd. Printable keys
echo to the physical console; CR echoes CR/LF and other controls do not echo.
Raw ReadLn stops at CR without consuming a following LF. Con/Trm formatted
Write/Writeln goes to the physical video console independently of Pascal
Output. Physical output errors report I/O F0; input-service errors report I/O20.
Pending earlier errors retain their code and suppress transfers.

Logical names are case-insensitive and create no disk files or directories.
Assigned CON:/TRM: support immediate input/output, Reset/Rewrite clear only that
binding's lookahead, and Close is a no-op. Preassigned Con/Trm lifecycle calls
report I/O22.

Aux and Text assigned to `AUX:` use Zeal's native `#SER0` serial driver. They
support formatted output, Char/String/numeric input and Text predicates, with
independent current-byte caches. Char consumes even Ctrl-Z; ReadLn stops at CR
without waiting for or consuming a following LF. A cached byte needs no new
device open. Output temporarily selects raw serial mode so Pascal's CR/LF is
sent exactly once, then restores the previous mode and closes its own descriptor.
This does not redirect serial traffic to the video console or keyboard.

Lst and `LST:` are output-only. The current Zeal target has no printer driver:
attempted output reports I/O20, and input reports I/O02. An unavailable serial
adapter also reports I/O20. Failed transfers and cleanup retain their I/O errors.
Assigned LST:/AUX: Reset/Rewrite clear lookahead; Close is a no-op. Preassigned
Lst/Aux lifecycle calls report I/O22. Logical names are case-insensitive and
create no disk files. Native UART transmission is emulator-tested; reception and
faults have deterministic host coverage, while physical serial RX remains an
open hardware gate.

Usr and Text assigned to `USR:` provide raw unechoed console input and physical
console output. Input uses its own binding cache, consumes Ctrl-Z on a Char read,
and permits a subsequent refill, like Kbd. Default Usr output is independent of
ConOutPtr. Assigned Reset/Rewrite clear lookahead and Close is a no-op;
preassigned Usr lifecycle operations report I/O22.

ConStPtr, ConOutPtr, LstOutPtr, AuxOutPtr and UsrOutPtr are writable Integer
addresses. Assign `Addr(callback)` to replace one; save its original value to
restore the default. ConSt callbacks return Boolean without parameters; output
callbacks take one Char. Nested callbacks may capture still-active recursive
locals through the native Addr bridges. A callback address must not outlive its
captured owner.

ConOut intercepts ordinary Output, Con/Trm output and console input echo.
ConSt supplies KeyPressed from the callback's Boolean result; physical
typeahead remains separate. Lst/Aux/Usr output uses the corresponding pointer,
including assigned logical files and Var Text aliases. Saved default pointers
are callable native Pascal adapters: physical video, unavailable printer, or
native SER0 as appropriate. Default ordinary Output retains inherited stdout;
the saved ConOut adapter writes to physical video. Pending I/O errors suppress
callbacks, and a callback's error code survives outer cleanup. Error unwinds
close files owned by abandoned callback frames while retaining caller and heap
file owners. Replacement
ConInPtr/AuxInPtr/UsrInPtr and ErrorPtr dispatch remain pending.

The duplicate-key case during RAM resource calls is fixed and tested with
complete queued taps. Parent resource services retain the caller's interrupt
setting after their stack and mappings are ready. Full physical-input and
hardware acceptance remain open.

Inline machine-code statements support slash-separated additive/subtractive
elements, integer constants, variable/routine operands, and `*` for the current
emission address. Constants in 0..255 emit one byte; other values and address
operands emit a little-endian word. `<` forces a low byte and `>` forces a word.
Address differences and forced bytes remain correct when generated code moves
above globals. Forward and enclosing routine entries have stable addresses.

Operands use the native Zeal ABI. Static variables use absolute addresses;
recursive locals and formal copies use their signed IX-frame offsets. A VAR
operand identifies its pointer slot. Typed constants reside in generated code.
Nested routine operands are raw entries: machine-code callers supply the native
lexical-parent link and arguments. `Addr` retains its captured-thunk behavior
for nested Pascal-callable pointers. Inline code preserves SP, IX/IY and the
other state required by that calling convention.

`{$U+}` enables physical Ctrl-C interruption at statements and loop conditions;
`{$U-}` is the default and leaves Ctrl-C available as raw Kbd character 3.
The switch restores its containing-file value after an include. User break
closes program files and returns to the interrupted source in Memory mode.
Polling preserves ordinary keys in a shared 255-character typeahead queue;
at capacity it leaves further events with the keyboard driver until reads
make space. It preserves operands, flags, alternate registers and Pascal frames.
This is separate from CBreak's existing buffered ReadLn behavior.

`{$C+}` (default) initializes CBreak toTrue and `{$C-}` toFalse when the root
block begins, before declarations. Later C switches do not assign the runtime
variable; set CBreak directly to change it during execution. Include-local C
switches restore the containing-file setting.

With CBreak=True, Ctrl-S pauses physical console output. Ctrl-S or Ctrl-Q
resumes the pending byte, and Ctrl-C while paused terminates the program.
Ordinary queued keys retain their order. At the255-character typeahead limit,
additional ordinary presses during output polling are discarded while resume
and break controls remain usable. Regular-file stdout redirection bypasses
keyboard flow control. Native input batching retains complete release events;
polling restores the borrowed input mode and echo descriptor.

The implemented directives are A, B, C, I checking, R, U, V, W and X.
Pascal CRT calls (ClrScr, ClrEol, GotoXY, InsLine, DelLine, CrtInit/CrtExit,
LowVideo/NormVideo/HighVideo and Delay), replacement input/error callbacks,
and overlays remain pending; Chain/Execute, CHN compilation and source-map
lookup are implemented. CP/M BIOS/BDOS services are intentionally
unavailable on Zeal; explicit unsupported-target diagnostics for them are still
pending. These gaps are separate from the editor's display functions.

Overlay declarations in Memory mode now report compiler error77 at the
`overlay` keyword, retaining source and releasing the failed build. Native
ZOVR group validation and selected-member staging are tested as a
separate component with `python3 tools/test-overlays.py` and its `--native`
mode after `zde cmake`. This does not yet enable disk overlay declarations,
OvrDrive, runtime cache/trampoline handling or overlay source navigation.
Completed-member compiler spooling is also tested independently: it appends
only a member's code and map/relocation suffixes, verifies the disk bytes, and
preserves the parent's staging state on failure. `tools/test-members.py` checks
the native serialization; `tools/test-output-host.py` covers transfer faults and
cancellation. Group ownership also covers nested reservations, shared member
origins, zeroed parent gaps and cleanup retries. Parser, late-helper and final
linker/runtime integration are still pending.

The editor now supports character/word/line movement, smooth scrolling, paging,
virtual columns through128 and horizontal scrolling. Arrow keys, Home/End and
Page Up/Down provide physical-key shortcuts. Ctrl-H moves left without deleting;
Backspace deletes left and Delete deletes under the cursor. Ctrl-V toggles
insert/overwrite, Ctrl-N splits without moving the cursor, Ctrl-Y deletes a line,
Ctrl-T deletes the right word, and Ctrl-Q,Y deletes to line end. TAB uses word
stops from the preceding line; Ctrl-Q,I toggles auto-indent. Ctrl-Q,L restores
the current line's before-edit text until the cursor leaves it; this is a
single-line restore, not general undo. Ctrl-P quotes a control byte, Ctrl-U
interrupts with Escape acknowledgment, and Ctrl-Q,P restores the previous edit
position. Ctrl-K,B and Ctrl-K,K set block begin/end; Ctrl-Q,B and Ctrl-Q,K jump
to them, including hidden or single markers. Ctrl-K,T marks a word; Ctrl-K,H
hides/shows it. Ctrl-K,C copies, Ctrl-K,V moves, and Ctrl-K,Y deletes the visible
selection. Copy/move/read leave the cursor at the new block's start. Moves into
their own selection do nothing. Block edits stage and validate the complete
result before changing the original buffer.

Ctrl-K,R reads a block from a named source file; missing files report an error.
Ctrl-K,W writes the exact selected bytes to a named file, defaulting an absent
extension to PAS. Existing files require overwrite confirmation; N returns to
filename input, and Ctrl-U interrupts with Escape acknowledgment. Block writes
keep the open source's filename, cursor, markers and dirty state. Parents are
created only when writing. Hidden, empty, inverted or incomplete selections
disable copy/move/delete/write. Blocks and literal controls are highlighted;
quoted controls display their capital-letter equivalents and retain their bytes.
Ctrl-Q,F finds, Ctrl-Q,A finds/replaces, and Ctrl-L repeats the previous operation.
Fields accept up to30 bytes, with arrows/Ctrl-S/D, word movement, Home/End,
Backspace/Delete, insert/overwrite and Ctrl-P quoting. Ctrl-F at a field's end
recalls its previous entry; Ctrl-Y clears the field. Empty Find cancels while
retaining prior history. Escape cancels a prompt; Ctrl-U interrupts with Escape
acknowledgment. Ordinary typing during a scan is retained in a bounded queue.

Options are B backward, G global start, U ignore ASCII case, W whole word, and
N skip replacement confirmation, followed optionally by a positive occurrence
count. G Find selects the match as documented, correcting the original's scan
to EOF. Repeat reuses G too. Quoted Ctrl-A is a one-byte wildcard in the search
pattern and a literal in replacement. Quoted CR/LF boundaries normalize to LF.
Replacement counts include N answers; inserted text is not searched again by
the same operation. A rejected replacement preserves earlier accepted edits.
The complete native editor replay passes 3,724 component assertions and 83
workflow checks. The complete physical-control input matrix remains pending.

Native tests cover editor workflows, ownership, error recovery and file safety;
exhaustive reference equivalence and physical hardware acceptance remain open.
Research artifacts and evidence are retained locally
but excluded from Git for now.
