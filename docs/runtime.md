# Runtime and predefined routines

This reference covers the current compiler's built-ins and predefined runtime
state. [Language syntax](language.md), [error codes](errors.md) and
[IDE operation](ide.md) are separate chapters. The optional stock libraries have
full references: [CRT](crt-library.md), [ZVB](zvb-library.md),
[ZOS](zos-syscalls.md), and [Pascal units](units.md).

Tables show call syntax, not declarations to copy into source. Writable arguments
are described as variables/destinations; `var` is shown explicitly in selected
heap signatures. Arguments to Read, Delete's string, Insert's destination,
Str's destination, Val's value/code, file operations and raw block destinations
must be writable storage. Predefined names can be shadowed by declarations.
Routines without parameters are normally written without parentheses.

## Contents

- [Predefined name index](#predefined-name-index)
- [Numeric and ordinal routines](#numeric-and-ordinal-routines)
- [String routines](#string-routines)
- [Text input and output](#text-input-and-output)
- [Files](#files)
- [Heap and block memory](#heap-and-block-memory)
- [Runtime state and driver hooks](#runtime-state-and-driver-hooks)
- [Program control and arguments](#program-control-and-arguments)
- [Library and system interfaces](#library-and-system-interfaces)

## Predefined name index

Every name below is available without a Uses clause.

| Group | Names |
| --- | --- |
| [Arithmetic](#numeric-and-ordinal-routines) | `Abs`, `ArcTan`, `Cos`, `Exp`, `Frac`, `Int`, `Ln`, `Round`, `Sin`, `Sqr`, `Sqrt`, `Trunc` |
| [Ordinal](#numeric-and-ordinal-routines) | `Chr`, `Hi`, `Lo`, `Odd`, `Ord`, `Pred`, `Succ`, `Swap`, `UpCase` |
| [Random numbers](#numeric-and-ordinal-routines) | `Random`, `Randomize` |
| [Constants](#numeric-and-ordinal-routines) | `False`, `MaxInt`, `Pi`, `True`, and the [16 color names](crt-library.md#colors) |
| [Strings](#string-routines) | `Concat`, `Copy`, `Delete`, `Insert`, `Length`, `Pos`, `Str`, `Val` |
| [Text I/O](#text-input-and-output) | `Eof`, `Eoln`, `KeyPressed`, `Read`, `ReadLn`, `SeekEof`, `SeekEoln`, `Write`, `WriteLn` |
| [Standard streams](#standard-streams) | `Aux`, `Con`, `Input`, `Kbd`, `Lst`, `Output`, `Trm`, `Usr` |
| [Files](#files) | `Assign`, `BlockRead`, `BlockWrite`, `Close`, `Erase`, `FilePos`, `FileSize`, `Flush`, `IOResult`, `Rename`, `Reset`, `Rewrite`, `Seek` |
| [Heap](#heap-and-block-memory) | `Dispose`, `FreeMem`, `GetMem`, `Mark`, `MaxAvail`, `MemAvail`, `New`, `Release` |
| [Storage](#heap-and-block-memory) | `Addr`, `FillChar`, `Mem`, `Move`, `Port`, `Ptr`, `SizeOf` |
| [Runtime state](#runtime-state-and-driver-hooks) | `AuxOutPtr`, `BufLen`, `CBreak`, `ConOutPtr`, `ConStPtr`, `HeapPtr`, `LstOutPtr`, `RecurPtr`, `StackPtr`, `UsrOutPtr` |
| [Program control](#program-control-and-arguments) | `Chain`, `Execute`, `Exit`, `Halt`, `ParamCount`, `ParamStr` |
| [System calls](zos-syscalls.md) | `ZOS`, `ZOSBuffer`, `ZOSRegisters` |

## Numeric and ordinal routines

| Routine | Result and rules |
| --- | --- |
| `Abs(X)` | Absolute value; Integer or Real retains its kind. Unsigned forms are supported. `Abs(-32768)` stays -32768. |
| `Sqr(X)` | X times itself in its numeric kind. Integer/unsigned multiplication can wrap. |
| `Sqrt(X)` | Real square root of Integer/Real X; X < 0 raises runtime `$03`. |
| `Sin(X)`, `Cos(X)` | Real sine/cosine of an Integer/Real radian argument. |
| `ArcTan(X)` | Principal Real arctangent, in radians. |
| `Exp(X)` | Real natural exponential; overflow raises `$01`, underflow becomes zero. |
| `Ln(X)` | Real natural logarithm; X <= 0 raises `$04`. |
| `Int(X)` | Real integer part, toward zero. |
| `Frac(X)` | Real X minus Int(X); nonzero fractional part keeps X's sign. |
| `Trunc(X)` | Real to Integer, toward zero; out-of-range result raises `$92`. |
| `Round(X)` | Real to nearest Integer, exact halves away from zero; range failure `$92`. |
| `Odd(X)` | Boolean low-bit test, including negative Integer and unsigned values. |
| `Ord(X)` | Ordinal value as Integer; pointer overload returns the address bit pattern. A one-character String/Char-array form requires length one. |
| `Chr(I)` | Char of ordinal I; `{$R+}` checks 0..255. |
| `Succ(X)`, `Pred(X)` | Next/previous value of the ordinal type; subrange checks apply with `{$R+}`. One-character text forms are supported. |
| `Hi(X)`, `Lo(X)` | Integer/Word high or low byte; for LongWord, high or low Word. |
| `Swap(X)` | Exchange Integer/Word bytes; exchange LongWord's two Words. |
| `UpCase(C)` | Char, or text of exactly one character, to uppercase ASCII; other byte values unchanged. |
| `Random` | Real value in [0,1). |
| `Random(N)` | Integer in [0,N) for positive Integer N, even with `{$R-}`; zero or negative N raises `$91`. Word is not accepted. |
| `Randomize` | Procedure. Vary the pseudorandom state; no cryptographic guarantee. |

`MaxInt` is 32767. `Pi` is a predefined Real constant. `True` and `False` are
Boolean values, with ordinals 1 and 0. Unsigned overloads and mixed arithmetic
follow [Word and LongWord](unsigned-integers.md); these do not imply automatic
unsigned/Real conversions.

Real values use the six-byte dialect format, not IEEE 754. Primitive addition
aligns significands by discarding low magnitude bits; multiplication/division
also truncate discarded magnitude bits. There are no NaNs, infinities or
subnormal values. The minimum positive magnitude is 2^-128 (about 2.9E-39);
smaller results flush to canonical zero. Results at or above 2^127 (about 1.7E38)
raise `$01`. Approximately eleven decimal digits are useful, but
mathematical identities and decimal-to-binary round trips need not be exact.
Trunc/Round's Integer-range errors remain active with `{$R-}`.

Examples: `Trunc(-2.9)` is -2, `Round(-2.5)` is -3, `Int(-2.75)` is -2.0,
`Frac(-2.75)` is -0.75, `Hi($1234)` is 18, `Lo($1234)` is 52 and `Swap($1234)` is
13330. Random uses the preserved 32-bit TP3-style sequence rather than a modern
generator. Each program activation starts with its own state. Do not depend on
Randomize for repeatable seeds or security.

## String routines

Strings contain bytes and a current length, bounded by their declared capacity.
Positions are one-based. Index zero exposes the length byte, but is not a valid
Copy/Delete/Insert position.

| Routine | Contract |
| --- | --- |
| `Length(S)` | Integer current length, not declared capacity. |
| `Copy(S, P, N)` | Up to N bytes from P, clipped at end; empty when P is beyond current length. |
| `Concat(S1, S2, ...)` | Concatenate in argument order; result over 255 bytes raises `$10`. String `+` has the same expression-length constraint. |
| `Pos(Pattern, S)` | Integer first matching position, or zero. Empty Pattern returns zero. |
| `Delete(S, P, N)` | Remove up to N bytes from writable S, clipped at end. |
| `Insert(Source, Dest, P)` | Insert before position P in writable Dest; truncate to Dest's capacity. |
| `Str(Value[:Width[:Decimals]], Dest)` | Format Integer/Real, or supported unsigned Value, into writable String Dest. |
| `Val(S, Value, Code)` | Parse Integer or Real into writable Value; writable Integer Code is zero or the one-based error position. |

Copy/Delete/Insert positions must be 1..255; invalid positions or negative
Copy/Delete counts raise `$11`. Zero counts produce an empty Copy/no deletion.
A position after the current string permits the routine's bounded end behavior;
it does not authorize writing beyond the declared capacity. Assignment and
Insert truncate to destination capacity, whereas an oversized intermediate
concatenation fails before destination truncation.

`Str` follows the numeric Write field rules below and retains the prefix that
fits Dest. `Val` accepts String, Char or compatible Char-array input, parses the
complete value and rejects embedded NUL or trailing junk. Integer `$` hexadecimal
input is accepted. The signed minimum `-32768` is valid. Empty input succeeds
without changing Value. Leading/trailing spaces are not silently trimmed: they
give a nonzero Code. For example, `'12x'` gives Code 3, and the out-of-range
`'32768'` gives Code 6, one past its end.
Code does not use IOResult or trigger checked I/O abort, and `Str`/`Val` leave a
pending I/O status alone. Value and Code can be two-byte Integer-family variables,
including wide subranges. After a conversion error, do not use Value as a
successfully parsed result. Unsigned Val targets are not supported. Do not alias
Value and Code.

```pascal
program ConvertNumber;
var TextValue: string[20]; Value, Code: Integer;
begin
  Val('-42', Value, Code);
  if Code = 0 then begin
    Str(Value:6, TextValue);
    WriteLn('[', TextValue, ']')
  end
end.
```

## Text input and output

### Read, ReadLn, Write and WriteLn

| Call | Meaning |
| --- | --- |
| `Read(V1, V2, ...)` | Read from default Input. |
| `Read(F, V1, V2, ...)` | Read from a selected Text/logical stream or typed file. |
| `ReadLn(...)` | Perform text reads, then consume the remainder of that line. |
| `Write(E1, E2, ...)` | Write formatted fields to default Output. |
| `Write(F, E1, E2, ...)` | Write to a selected Text/logical stream or typed file. |
| `WriteLn(...)` | Write text fields and append CR/LF; no arguments writes a newline. |

Text reads accept Char, String, Integer-family targets including Byte/subranges,
and Real. Selectors, pointer targets and `var` parameters can be destinations.
Unsigned Word/LongWord Read and automatic Real conversion are not provided.

| Target | What a text read consumes |
| --- | --- |
| Char | One byte |
| String | Bytes up to capacity, line end or EOF; the line ending stays unread |
| Integer family, Real | Leading whitespace, then a token of at most 30 characters; the delimiter stays unread |

Invalid numeric input gives I/O `$10`. A numeric target at the fresh-line
Ctrl-Z/EOF marker retains its previous value. Ordinary CR/LF whitespace is
skipped, so disk Text or continuing buffered input may advance to the next line. A Byte target keeps the low byte, and input adds no
subrange check. ReadLn discards unconsumed text in that line.

```pascal
program AskNumber;
var N, Status: Integer;
begin
  N := 0;
  Write('Number: ');
  {$I-}
  ReadLn(N);
  Status := IOResult;
  {$I+}
  if Status = 0 then WriteLn('Twice: ', N * 2)
  else WriteLn('Not a number')
end.
```

A blank Enter in this fresh-line example is not a conversion error: Status is
zero and the initialized N stays zero. To require a number, read a String and
validate its nonempty contents with Val.

Text output accepts Char, String, Boolean, Integer, Word, LongWord and Real.
Boolean output is TRUE/FALSE. Typed-file Read/Write transfers components and
has no formatted-field semantics; use variables of the compatible component type.

### Field formatting

`WriteLn(Count:6, Price:10:2)` uses a minimum width, right-justified with spaces.
A width too small does not cut the value. Negative width adds no padding;
width above 255 is bounded to 255. Char, String and Integer default to unpadded
output. Real defaults to an 18-character scientific field; Real `:Width` keeps
scientific notation with that minimum width. Real `:Width:Decimals`
selects fixed point for 0..24 decimals; other decimal counts select scientific
notation. Zero decimals emits no decimal point. Unsigned fields are decimal.

The field value is captured before its width and precision expressions are
evaluated; nested output stays ordered. Str shares these rules.

### Standard streams

| Name | Meaning and restrictions |
| --- | --- |
| `Input`, `Output` | Default input/output; inherited native redirection is honored where applicable. |
| `Con` | Buffered console input and physical video output. |
| `Trm` | Immediate terminal input without Enter. Printable keys echo, CR echoes CR/LF, other controls do not echo. Output goes to physical video. |
| `Kbd` | Unechoed immediate keyboard input. Writing reports `$03`. Ctrl-C reads as byte 3. |
| `Lst` | Output-only logical printer. There is no default printer: output reports `$20`, input `$02`. `LstOutPtr` can redirect it. |
| `Aux` | Native `#SER0` serial input/output; unavailable adapter operations report `$20`. |
| `Usr` | Unechoed immediate console input and physical video output. `UsrOutPtr` can replace the output. |

Do not Assign, Reset, Rewrite or Close a predefined standard stream; these
operations report I/O `$22`. An ordinary Text variable can be assigned a logical
name such as `'CON:'`, `'TRM:'`, `'KBD:'`, `'LST:'`, `'AUX:'` or `'USR:'`, in any
letter case. These names create no disk file. Direction restrictions still apply.
Unsupported logical-device operations give `$20`, not a fabricated physical
device. Reset and Rewrite on an assigned logical stream clear that binding's
lookahead; Close does nothing.

### Buffered console input

With `{$B+}` (default), each unqualified Read starts a fresh buffered line, so
unread text from an earlier call does not satisfy a later one. `{$B-}` permits
successive unqualified Reads to consume the same buffer; explicit
`Read(Input, ...)` uses the continuing buffered stream. ReadLn advances the line.
Bare `Read;` fills the default buffer without consuming a value.

The console's buffered limit is 127 bytes. Setting predefined Byte `BufLen`
to 0..127 restricts the next buffered read, then it resets to 127; a larger value
raises `$91` before reading. Excess typing is ignored until edits make room. Input
echo uses physical video, separately from redirected Output.

| Key during buffered input | Effect |
| --- | --- |
| Backspace, Delete, Ctrl-H | Remove the previous byte |
| Escape, Ctrl-X | Clear the line |
| Ctrl-D | Recall one character from the previous line |
| Ctrl-R | Recall the rest of the previous line, keeping what you typed |
| Enter | Accept the line |
| Ctrl-Z | End input as EOF |
| Ctrl-C | Break when `CBreak` is True; ignored when False |

**End of a fresh line.** A Char read past the end of a fresh `{$B+}` line returns
Ctrl-Z (`#26`), and consuming that marker lets a later field refill the buffer.
Numeric reads and bare Read leave the marker in place. Lines read under `{$B-}`
or from explicit Input keep CR/LF instead: Char can read both bytes, String stops
before CR, and a numeric read skips them and continues on the next physical line.
ReadLn(Input) consumes the current line through LF.

### Break and flow control

`CBreak` is initially True under the default `{$C+}` and False under `{$C-}`. See
[directives](language.md#compiler-directives-and-includes). With CBreak True,
Ctrl-C during buffered Con or immediate Trm input ends the program; in a Memory
run, Escape then returns to the input statement in source. Kbd still delivers
Ctrl-C as byte 3.

With CBreak True, physical output also responds to Ctrl-S pause, Ctrl-S/Ctrl-Q
resume and Ctrl-C break. Redirected disk stdout bypasses keyboard flow control.
This is separate from `{$U+}`, which polls for Ctrl-C at statements and loop
conditions during normal execution. A user break performs no Pascal Close.

### Keyboard polling

`KeyPressed` tests the keyboard without consuming the pending key. Read it with
`Read(Kbd, C)` or [CRT ReadKey](crt-library.md). KeyPressed is a built-in and
does not require `uses Crt`; ReadKey does. Keyboard input is physical keyboard
input, distinct from a redirected disk Input stream. Text already in Input's line
buffer does not count as a new key. Unavailable keyboard input reports `$20`.

### Raw and device stream details

`Kbd`, `Trm`, `Aux` and `Usr` return Char reads without Enter. A String read stops
at capacity, CR or Ctrl-Z. Physical Enter, Escape and Delete arrive as CR, ESC and
DEL; Ctrl-J stays LF. A raw Char read consumes even Ctrl-Z, so a later read can
continue. Raw ReadLn stops at CR without fetching a following LF; Con consumes an
LF only when already buffered. Primary Input has its own sticky EOF state.

Each binding keeps its own one-byte lookahead, so Con and an assigned `'CON:'`
file do not share it; Input and Output share one. Con and Trm output goes to
physical video independently of Pascal Output. A physical output failure reports
`$F0`, and an input-service failure `$20`.

Aux serial output temporarily selects raw mode so CR/LF is sent once, then
restores its mode and closes its temporary descriptor. Usr output is independent
of ConOutPtr. Physical serial RX and device-specific timing still require board
acceptance.

### Text predicates

| Function | Result |
| --- | --- |
| `Eof[(F)]` | True at end of file |
| `Eoln[(F)]` | True at end of line or end of file |
| `SeekEoln[(F)]` | Skip spaces and tabs, then test as Eoln |
| `SeekEof[(F)]` | Skip spaces, tabs and line terminators, then test as Eof |

All return Boolean and use Input when the file argument is omitted. Disk text
predicates inspect the next byte: Ctrl-Z/physical end is EOF, and CR is line end.
On the console they read a line when needed and inspect its unread text. Raw
devices can fetch one byte into that binding's lookahead, which may wait for a
key. These functions can consume skipped whitespace. Seek, FilePos and FileSize
are not text-stream operations.

## Files

```pascal
program WriteNumbers;
var F: file of Integer;
    I, Value: Integer;
begin
  Assign(F, 'NUMBERS.DAT');
  Rewrite(F);
  for I := 1 to 4 do begin
    Value := I;
    Write(F, Value)
  end;
  Close(F)
end.
```

Typed-file Write requires a variable argument; the example copies the For
control into Value because a loop control cannot be passed by reference.

Text files use Read/ReadLn and Write/WriteLn with the same field rules as the
console:

```pascal
program CopyLines;
var Source, Dest: Text;
    Line: string[127];
begin
  Assign(Source, 'IN.TXT');
  Assign(Dest, 'OUT.TXT');
  Reset(Source);
  Rewrite(Dest);
  while not Eof(Source) do begin
    ReadLn(Source, Line);
    WriteLn(Dest, Line)
  end;
  Close(Source);
  Close(Dest)
end.
```

Always Close explicitly after both input and output. Program termination,
Dispose/Release and a successful Execute do not perform Pascal file finalization.
Omitting Close can lose buffered data, typed counts or a Text EOF marker.

**Full disks.** The pinned kernel's ZealFS has a growing-file disk-exhaustion
defect that can corrupt control flow. Leave free space when writing large files.

### Declaration and on-disk formats

| Declaration | File format and unit of position |
| --- | --- |
| `Text` | CR/LF text; Ctrl-Z marks EOF. No typed-file header. |
| `file of T` | Four-byte header: little-endian component count, little-endian component size; contiguous component bytes follow. Positions count components. |
| `file` | Raw data, accessed in 128-byte records. No typed header. Positions count records. |

Typed component counts are limited to 65535; another component write raises
I/O `$F2`. FileSize/FilePos/Seek carry unsigned 16-bit positions through Integer
bit patterns: a size of 65535 appears as Integer -1, but passing that pattern
back to Seek selects that position. Zero-size components fail with `$90` before
creating/truncating a file. CP/M padding is accepted on input but not required
for native physical allocation. File variables have native runtime control
blocks; copying their bytes is not a way to duplicate an association.

### Lifecycle and positioning

| Routine | Contract |
| --- | --- |
| `Assign(F, Name)` | Bind a filename/logical name without opening, truncating or creating parent directories. Replaces any earlier association without closing it. |
| `Reset(F)` | Open existing data; typed files validate component size. An already-open binding needs Close or a new Assign first. |
| `Rewrite(F)` | Create/truncate the assigned output file. This destroys previous contents. |
| `Close(F)` | Flush and finalize the file, reporting errors. |
| `Seek(F, N)` | Select typed/untyped component N; zero-based, allowing logical EOF for append. Beyond EOF gives `$91`. |
| `FilePos(F)` | Current typed/untyped logical component position as Integer bits. |
| `FileSize(F)` | Typed/untyped logical component count as Integer bits. |
| `Eof(F)` | Whether current position is at logical end. |
| `Flush(F)` | Flush typed-file data and invalidate cached read state. Not a Text/untyped operation. |
| `Erase(F)` | Remove the assigned disk file. |
| `Rename(F, Name)` | Rename a closed assigned disk file and update its association; existing targets are rejected. |
| `IOResult` | Integer. Return and clear the pending I/O status; see [recovery](#io-checking-and-recovery). |

Text predicates and formatted Read/Write apply only to Text/logical streams.
File parameters are `var`, and file components cannot themselves be files.

Rename copies the bytes, verifies the copy and then removes the old name, so it is
not atomic. On failure the old association stays, and a partial or complete
destination file may remain; remove it before retrying.

Keep an open file's pathname stable. Between operations a Pascal file holds no
kernel descriptor and reopens its path when next used, so deleting or replacing
the file through another binding can make that fail or select the new file.

### File names

Ordinary data filenames use native Zeal paths, case behavior and device roots,
not forced CP/M 8.3 names. Assign accepts relative names, absolute names on the
current disk and drive-qualified absolute names such as `T:/DATA/LOG.TXT`.
A drive-relative name such as `T:FILE` is rejected, and an invalid name keeps the
previous association.

The name is resolved once, against the current directory at Assign, with `.`, `..`
and repeated separators normalized. Changing directory later does not retarget
the file. Names are limited to 127 bytes, with at most 15 bytes per component.
Data filenames get no implicit extension.

Runtime Assign/Rewrite does not create missing parent directories; IDE
source/executable saving has its own directory-creation behavior.

### BlockRead and BlockWrite

`BlockRead(F, Buffer, Count[, Actual])` and
`BlockWrite(F, Buffer, Count[, Actual])` operate only on untyped files. Count and
the optional writable Actual are Integer values representing record counts,
not byte lengths. Provide at least `Count * 128` accessible bytes, without
address-space overflow. The compiler cannot infer that capacity from the call.

With Actual, a normal short read reports completed records, including zero at
EOF, without treating it as an obligatory full read. Without Actual, an
incomplete requested read reports I/O `$99`. Handle write/flush/close errors
explicitly when data matters; a successful earlier transfer does not certify
later finalization. For arbitrary byte lengths, see [ZOS Read/Write](zos-syscalls.md).

### Pinned-kernel overwrite errors

The kernel overwrite-status defect described under [ZOS](zos-syscalls.md) can
also reach Pascal file operations. SYSFILE maps a nonzero write status to an I/O
failure; it does not hide a false status after a successful write. Seek followed
by Write, or a typed-count header update at Close, can therefore report `$F0`
after changing bytes. Completed side effects are not rolled back. Check IOResult
with unchecked I/O, and avoid blindly replaying a transfer after an error.

### I/O checking and recovery

`{$I+}` is the default: a failed I/O operation aborts with its code and PC.
`{$I-}` leaves a pending status; retrieve it with `IOResult`, which returns and
clears that status. A second IOResult returns zero. Retrieve it before attempting
further I/O: a pending failure suppresses later transfers, although their argument
expressions still run. `{$I-}` does not disable division, range or stack errors.
ZOS status values are returned directly and have a separate convention from
Pascal IOResult.

```pascal
program OpenSafely;
var F: Text; Status: Integer;
begin
  Assign(F, 'NOTES.TXT');
  {$I-}
  Reset(F);
  Status := IOResult;
  {$I+}
  if Status = 0 then Close(F)
  else WriteLn('Open failed: ', Status)
end.
```

See [the error reference](errors.md) for category/code meanings. A native close
error can occur after the data transfer completed and the kernel released its
descriptor. Do not blindly replay a Write or BlockWrite after such an error.

## Heap and block memory

| Routine | Contract |
| --- | --- |
| `New(var P)` | Allocate P's declared target type, storing the address in pointer variable P. |
| `GetMem(var P, N)` | Allocate N bytes (Integer or Word), storing the address in P. |
| `Dispose(var P)` | Free the individual New allocation. |
| `FreeMem(P, N)` | Free the GetMem allocation with its original requested byte count. |
| `Mark(var P)` | Save the current heap frontier in pointer variable P. |
| `Release(P)` | Reclaim storage allocated after the saved frontier. |
| `MemAvail` | Total usable free payload; native metadata and stack limits affect it. |
| `MaxAvail` | Largest allocatable contiguous payload. |
| `Ptr(I)` | Construct a pointer from a 16-bit Integer or Word address. LongWord is rejected. |
| `Addr(V)` | Integer address of variable/selected storage, `Mem[I]` or a supported routine. |
| `SizeOf(T)` / `SizeOf(V)` | Integer byte size of type or storage; does not evaluate index expressions. Addr does not accept a type name. |
| `Move(Source, Dest, N)` | Copy N raw bytes, handling overlap. |
| `FillChar(Dest, N, Value)` | Set N bytes to the low byte of an ordinal Value, or to a one-character String. |

```pascal
program Stack;
type PNode = ^TNode;
     TNode = record Value: Integer; Next: PNode end;
var Head, Node: PNode;
    I: Integer;
begin
  Head := nil;
  for I := 1 to 3 do begin
    New(Node);
    Node^.Value := I;
    Node^.Next := Head;
    Head := Node
  end;
  while Head <> nil do begin
    Node := Head;
    Head := Head^.Next;
    WriteLn(Node^.Value);
    Dispose(Node)
  end
end.
```

Use either frontier Mark/Release or individual Dispose/FreeMem reclamation in a
heap; do not mix regimes. Allocated data is not promised zero initialization.
Freed pointer bits remain dangling. Close any files in an object before freeing
its storage. Odd allocations may round up, and even a zero-width object consumes
allocator metadata. Heap/stack bounds failures report runtime `$FF`.

Move/FillChar accept 16-bit byte counts, including Word values. They operate on
storage, not string characters, so include a String's length byte only when
intended. They do not perform object construction, pointer ownership or Pascal
file cleanup. Avoid spans outside accessible mapped memory.

MemAvail/MaxAvail results use Integer bit patterns; quantities above 32767 can
appear negative. Compare them as suitable unsigned values when needed. `Mem`,
`Port` and absolute variables are documented under [native access](language.md#native-machine-access).

## Runtime state and driver hooks

| Name | Type and meaning |
| --- | --- |
| `BufLen` | Byte, next buffered-input bound; initially 127 and reset after each read. |
| `CBreak` | Boolean controlling Ctrl-C and output flow control; initial value comes from the root `{$C}` setting. |
| `HeapPtr` | Integer bit-pattern address of the heap frontier. |
| `RecurPtr` | Integer bit-pattern address of the separate recursion-stack frontier. |
| `StackPtr` | Special read/write CPU-stack pointer expression/assignment. Not an addressable ordinary variable. |
| `ConStPtr` | Integer address of a no-argument Boolean status callback. |
| `ConOutPtr`, `LstOutPtr`, `AuxOutPtr`, `UsrOutPtr` | Integer addresses of Pascal `procedure(C: Char)` output callbacks. |

Save a hook's old address before installing `Addr(Handler)`, then restore it
before its captured owner returns. Nested callbacks are supported while the
captured activation is alive; they see the currently installed Pascal workspace.
ConOutPtr intercepts default Output, Con/Trm output and console echo; its saved
default adapter writes physical video, while ordinary default Output can retain
inherited redirection. ConStPtr supplies KeyPressed from its Boolean result.
Lst/Aux/Usr output uses the corresponding pointer. Pending I/O errors suppress
callbacks and preserve the earlier status. The default adapters are native
routines, not CP/M BIOS addresses. A callback's
I/O/error policy follows the documented Pascal call, rather than granting it an
unlimited lifetime or unchecked memory access. There is no general procedure
parameter syntax. Replacement `ConInPtr`, `AuxInPtr`, `UsrInPtr` and `ErrorPtr`
are not implemented in the current compiler.

```pascal
program CountOutput;
var Saved, Count: Integer;
procedure CountChar(C: Char);
begin
  Count := Count + 1
end;
begin
  Count := 0;
  Saved := ConOutPtr;
  ConOutPtr := Addr(CountChar);
  Write('abc');
  ConOutPtr := Saved;
  WriteLn(Count)
end.
```

Do not move HeapPtr/RecurPtr/StackPtr while live allocations/frames depend on
them. RecurPtr initially lies 1024 bytes below StackPtr and `{$A-}` routines save
Pascal workspace on that separate recursion stack. StackPtr assignment validates
the owned stack range but cannot make destruction of live frames safe. Always
use actual addresses/headers, not historical fixed global-placement measurements.

## Program control and arguments

| Routine | Meaning |
| --- | --- |
| `Exit` | Leave the current routine; at main-program level, finish normally. |
| `Halt` | Terminate; this dialect accepts no exit-status argument. |
| `ParamCount` | Number of saved-program command-tail parameters. |
| `ParamStr(N)` | Nth parameter; zero, negative and out-of-range indexes return empty. |
| `Chain(F)` | Replace saved-program code with a compatible native CHN, retaining the specified shared globals/runtime contract. |
| `Execute(F)` | Transfer to a native saved executable; does not return on success. |

Memory runs have no command arguments. Native saved-program arguments preserve
case; spaces/tabs separate them, and quote characters are literal bytes rather
than a quoted-argument syntax. The complete command tail is limited to 127 bytes;
oversize tails are rejected, not silently truncated. The IDE's external **X**
command supplies no command-tail arguments.

Chain/Execute take an assigned file variable, not a string expression. They are
available in saved mode; direct Memory execution reports I/O `$21`. With `{$I-}`,
a failed load can return for IOResult handling. Successful transfers never return
to the next statement. Close files before Execute if their final data matters;
terminal cleanup does not finalize Pascal buffers. Chain preserves its documented
logical associations, but requires a compatible CHN built with matching layout
settings. See [saved programs and transfers](ide.md#saved-programs-and-transfers).
For a returning child call with explicit arguments, see ZOS EXEC preserve mode.
For an exit status, use the checked ZOS EXIT interface.

## Library and system interfaces

Three stock interfaces have full reference pages: 16 CRT routines, 27 ZOS calls
and 45 ZVB routines.

Text screen functions are in `uses Crt;`, not automatically imported globals:
`ClrScr`, `ClrEol`, `GotoXY`, `WhereX`, `WhereY`, `InsLine`, `DelLine`, `CrtInit`,
`CrtExit`, `LowVideo`, `NormVideo`, `HighVideo`, `TextColor`, `TextBackground`,
`Delay`, `ReadKey`. See [CRT](crt-library.md) for ranges, cleanup and errors.
Named color constants Black through White are predefined Integer constants.
Delay is an approximate OS wait, not an exact cycle timer. Negative durations
are invalid. The Pascal screen implementation favors clarity over hand-written
Z80 speed.

`ZOS`, `ZOSBuffer` and `ZOSRegisters` invoke native syscalls. `uses ZOS;` adds
constants/record types; the compiler needs its matching ZOS.ZPU bridge even when
using numeric selectors. These APIs do not simulate BDOS/BIOS. See the complete
[ZOS reference](zos-syscalls.md), including kernel write-status caveats.

The 45 ZVB routines and their types/constants are documented in the
[ZVB reference](zvb-library.md) and [register reference](zvb-registers.md).
Importing a library does not automatically initialize hardware or add every
routine to the saved program. See [Unit linking](units.md) for reachability,
initialization and supported Unit-body facilities.
