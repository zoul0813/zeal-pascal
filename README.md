# Zeal Pascal

A native Pascal IDE, compiler and runtime for the MMU Zeal 8-bit Computer.
This is an incremental implementation of the recovered Turbo Pascal 3 dialect;
full language, editor and runtime conformance is still in progress.

Build using the existing Zeal development environment:

```sh
zde cmake
python3 tools/test-native-host.py
python3 tools/test-native.py
```

Deploy the matching `build/zpascal.bin` and `build/zpascal.zpl` from one build.
Run the BIN from ZShell on the supported video/PS/2 target; its default library
path is `T:/lib/zpascal.zpl`. Create that installation directory and copy the
ZPL there with the normal shell utilities. To select another library path:

```text
H:/zpascal.bin H:/zpascal.zpl
```

The launcher verifies the complete library before entering any application
bank. A missing, damaged or mismatched library returns to the shell. The editor,
compiler, file operations and compiler front end use separate owned RAM banks.
Internal `pascal_runtime.zpl` and `ram_trampoline.zpl` templates remain embedded
in the launcher and require no separate deployment. Generated Pascal COM
programs contain their own runtime and need no IDE library.

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

Current language support includes Integer, Byte, Char, Boolean and `string[1..255]` globals,
compound statements, assignments, Integer arithmetic/bitwise/shift expressions,
eager Boolean logic, ordinal/string comparisons, If/Then/Else, While/Do,
Repeat/Until, For/To/Downto and Case/Of/Else. Case supports ordinal
selectors, comma lists, inclusive ranges and nested alternatives.
For controls support the implemented ordinal families;
the final bound is evaluated once, zero trips retain the old control value and
normal completion leaves the final bound. Write/Writeln accepts strings,
Integers, Characters and Booleans; ReadLn currently accepts string targets. Short-string assignment
truncates to the destination capacity. Memory execution preserves IDE pages and
source; checked errors return to their source statement after Escape.

Declarations may repeat in any order. Ordinary constants alias literal/constant
values; scalar and string typed constants are initialized writable storage.
Named types, aliases, enumerations and ordinal subranges are supported. Byte
and Integer share arithmetic compatibility; distinct enums retain their type
identity. `$R+` enables ordinal bounds checks and `$R-` disables them.
Char/string conversions and comparisons are supported; converting a string
whose length is not one to Char reports runtime error 10.

Implemented functions include Ord, Chr, Succ, Pred, SizeOf, Addr, UpCase,
Odd, Hi, Lo, Swap and the Integer forms of Abs/Sqr. The compiler currently has
512 symbol entries and 64 type descriptors (including five built-in entries).
Exhaustion reports resource error 98. Char arrays and selected-variable forms
of Addr/SizeOf await aggregate support.

The [control-flow fixture](tests/native/control.pas),
[ordinal fixture](tests/native/ordinals.pas), [Case fixture](tests/native/cases.pas)
and enum boundary fixtures exercise these features in Memory and saved native
COM execution. Real6, aggregates,
pointers, routines, Goto and the rest of the dialect remain in progress.

ReadLn currently uses Zeal's cooked keyboard line editor and restores raw mode
before returning. It supports string fields, empty lines and discarding the rest
of a line. Numeric input, Turbo console controls (including Ctrl-C), BufLen and
the exact 127-character editing boundary remain pending. Strings retain at most
127 input characters in this increment. String concatenation/indexing and the
remaining dialect are also pending.

The editor remains partial. Native tests cover the implemented slice, ownership,
error recovery and file safety; full reference replay and physical hardware
acceptance remain open. Research artifacts and evidence are retained locally
but excluded from Git for now.
