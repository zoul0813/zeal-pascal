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

Launch `build/zpascal.bin` from ZShell on the supported video/PS/2 target.
Internal `pascal_runtime.zpl` and `ram_trampoline.zpl` build assets are embedded
in the executable and require no separate deployment.

| Key | Operation |
| --- | --- |
| W | Open/select a work file by name; default extension PAS |
| E | Edit; Ctrl-K,D returns to the menu with text retained in RAM |
| S | Save source through verified temporary/recovery copies |
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

The currently installed native emulator also rejects TF writes with error 1;
standard `mkdir`/`cp` reproduce this independently of Pascal. The same unchanged
ZOS ROM, utilities and TF images work on the earlier available emulator build.
The difference is in the SPI timing/response model, already present in the
emulator's v1.0.0 source. No OS or emulator patch has been applied. Acceptance
records distinguish the installed emulator's failed TF checks from the earlier
build's successful tests.

Current language support includes Integer, Boolean and `string[1..255]` globals,
compound statements, assignments, Integer arithmetic/bitwise/shift expressions,
eager Boolean logic, ordinal/string comparisons, If/Then/Else, While/Do,
Repeat/Until and For/To/Downto. For controls currently support Integer/Boolean;
the final bound is evaluated once, zero trips retain the old control value and
normal completion leaves the final bound. Write/Writeln accepts strings,
Integers and Booleans; ReadLn currently accepts string targets. Short-string assignment
truncates to the destination capacity. Memory execution preserves IDE pages and
source; checked errors return to their source statement after Escape.

The [native control-flow fixture](tests/native/control.pas) exercises these
features in both Memory and saved native COM execution. Remaining declarations,
types, routines, Case/Goto and the rest of the dialect are still in progress.

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
