# Zeal Pascal

A native Pascal IDE, compiler and runtime for the MMU Zeal 8-bit Computer.
This is an incremental implementation of the recovered Turbo Pascal 3 dialect;
full language, editor and runtime conformance is still in progress.

Build using the existing Zeal development environment:

```sh
zde cmake
python3 tools/test-native-host.py --sanitize
python3 tools/test-editor-native.py
python3 tools/test-native.py
python3 tools/test-input.py
python3 tools/test-math.py
python3 tools/test-output.py
python3 tools/test-real.py --host --sanitize --native
python3 tools/test-real.py --trig --sanitize --native
```

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
The runtime template occupies a verified library bank; the RAM trampoline
remains in the launcher. Internal `pascal_runtime.zpl` and `ram_trampoline.zpl`
build products require no separate deployment. Generated Pascal COM
programs contain their own runtime and need no IDE library.

Startup is currently slow: allow roughly 2½ minutes at the target's normal
speed for library loading and verification before the IDE prompt appears.

| Key | Operation |
| --- | --- |
| W | Open/select a work file by name; default extension PAS |
| E | Edit; Ctrl-K,D returns to the menu with text retained in RAM |
| S | Save source through verified temporary/recovery copies |
| A | Save current source under another filename; Escape cancels |
| C | Compile in the selected output mode |
| R | Run the current build, compiling first when needed |
| O | Select M Memory or C native Com-file; Q returns |
| Q | Quit, resolving unsaved source with save/discard/cancel |

Startup opens `T:/src/pascal/UNTITLED.pas` when available. Missing source or
parent directories start an empty RAM buffer silently. W accepts native absolute
or relative paths, including a new filename under missing parents. Loading
creates no directories. S and native COM publication create missing parents
recursively when saving. A parent that is an existing file is an error; failed
loads/saves retain the previous RAM buffer.

To change the filename after editing, press Ctrl-K,D to return to the menu,
then A, enter a path such as `T:/untitled.pas`, and press Enter. Save as keeps
the current text and cursor. A verified successful save adopts the new path;
failure or cancellation retains the old path and RAM contents. W opens a file
and should not be used to rename unsaved source. Changing the source path
invalidates an existing build so its source map cannot retain the old filename.

In Com-file mode, compiling the default source writes
`T:/src/pascal/UNTITLED.COM` and `T:/src/pascal/UNTITLED.ZMP`.
Run `T:/src/pascal/UNTITLED.COM` from ZShell. These are native Zeal Z80
executables. Source must be saved separately with S.

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

Declarations may repeat in any order. Ordinary constants alias literal/constant
values; scalar and string typed constants are initialized writable storage.
Named types, aliases, enumerations and ordinal subranges are supported. Byte
and Integer share arithmetic compatibility; distinct enums retain their type
identity. `$R+` enables ordinal bounds checks and `$R-` disables them.
Char/string conversions and comparisons are supported; converting a string
whose length is not one to Char reports runtime error 10.

Implemented functions include Ord, Chr, Succ, Pred, SizeOf, Addr, UpCase, Length,
Odd, Hi, Lo, Swap, Abs, Sqr, Int, Frac, Trunc and Round. Ord, Succ and Pred accept
String/Char-array values of exactly one character; other lengths report runtime
error 10. The compiler currently has
512 symbol entries (four reserved for HeapPtr/RecurPtr/BufLen/CBreak) and 64 type descriptors
(including six built-in types).
Exhaustion reports resource error 98. Arrays support ordinal bounds, nested or
comma-separated dimensions, packed layout, whole-array assignment, typed
initializers and value/Var parameters. R+ checks each index before access.
A value parameter passes a source address; the callee makes an independent
copy before its body runs. Array- and record-valued user functions remain invalid.

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
error 11. Nested routine address bridges and remaining runtime APIs are
still in progress.

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
already at line end retain their values. Byte stores truncate and input does
not add subrange checks. A Char read at buffered line end returns Ctrl-Z (`#26`);
later fields can refill the buffer. `$B+` (default) selects a fresh console line for each
call; `$B-` retains its unread tail between calls. Bare `Read;` fills the buffer
without consuming a value, and ReadLn discards the remaining line.

BufLen defaults to127, limits the next physical input line to0..127 characters,
then resets to127. Larger values report runtime91 before reading. Excess input
is ignored; Backspace/DEL, Ctrl-H and Delete make room for replacement text.
Escape/Ctrl-X clear the line. Ctrl-D recalls one previous-line character and
Ctrl-R recalls the remaining previous line, preserving any typed prefix.
CBreak defaults toTrue: Ctrl-C terminates the program and Escape returns to the
input statement in the unsaved source. With CBreak=False, Ctrl-C is ignored.
Ctrl-Z ends console input. Echo uses the video console separately from Pascal
Output, so redirected program output does not collect typed input.
Unbuffered KeyPressed/Kbd, explicit Pascal files/devices and `$I-`/IOResult
remain pending.

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
Search/replace and the complete physical-control input matrix remain pending.

The editor remains partial. Native tests cover the implemented slice, ownership,
error recovery and file safety; full reference replay and physical hardware
acceptance remain open. Research artifacts and evidence are retained locally
but excluded from Git for now.
