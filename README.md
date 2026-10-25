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

Startup opens `B:/UNTITLED.PAS` when available. A missing file on an available
drive starts empty. Unavailable/invalid media reports failure and keeps the RAM
buffer. W accepts native absolute or relative paths and preserves the previous
buffer when loading fails.

In Com-file mode, compiling `B:/UNTITLED.PAS` writes `B:/UNTITLED.COM` and
`B:/UNTITLED.ZMP`. Run `B:/UNTITLED.COM` from ZShell. These are native Zeal Z80
executables. Source must be saved separately with S.

Source saves retain the previous source as BAK. An existing BAK is preserved in
the displayed recovery directory and requires confirmation before replacement.
Executable/map publication likewise retains verified recovery copies. The OS
has no atomic rename/fsync guarantee; recovery directories are intentionally
kept. No save failure discards the RAM source or silently reports success.

Current language support includes Integer and `string[1..255]` globals, compound
statements, assignments, signed Integer expressions, string/Integer
Write/Writeln, string-target ReadLn and Halt/Exit. Short-string assignment
truncates to the destination capacity. Memory execution preserves IDE pages and
source; checked errors return to their source statement after Escape.

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
