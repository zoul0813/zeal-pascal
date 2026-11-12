# Pascal language reference

This is the source-language reference for the current native Zeal Pascal compiler.
It describes a CP/M-80 Turbo Pascal 3 style dialect with explicit Zeal extensions.
It is not a reference for DOS Turbo Pascal or Object Pascal. For editing and
compilation, see the [IDE guide](ide.md). The [runtime reference](runtime.md)
documents predefined routines; [compatibility](compatibility.md) distinguishes
native additions and unsupported facilities.

## Contents

- [A first program](#a-first-program)
- [Source text and tokens](#source-text-and-tokens)
- [Program structure and declarations](#program-structure-and-declarations)
- [Types and storage](#types-and-storage)
- [Expressions](#expressions)
- [Statements](#statements)
- [Procedures and functions](#procedures-and-functions)
- [Compiler directives and includes](#compiler-directives-and-includes)
- [Native machine access](#native-machine-access)
- [Syntax summary](#syntax-summary)
- [Limits and unsupported language features](#limits-and-unsupported-language-features)

## A first program

```pascal
program Hello;
var I: Integer;
begin
  for I := 1 to 3 do
    WriteLn('Hello from Zeal ', I)
end.
```

Save as `HELLO.PAS`. In the IDE, choose **W**, enter its path, then **R** to
compile and run in Memory mode. **O → C → Q**, followed by **C**, writes a native
COM executable. See [compilation modes](ide.md#compilation-modes).

Keywords, built-ins and ordinary identifiers are case-insensitive. Indentation
is for the reader. Semicolons separate statements; a semicolon before `else`
ends the `if` and is incorrect. A program ends in a period.

## Source text and tokens

Identifiers begin with an ASCII letter or underscore, followed by letters,
digits or underscores. All characters are significant, up to the 127-byte source
line limit. An inner declaration can shadow an outer declaration or a predefined
identifier. Reserved words cannot be identifiers.

The reserved words are:

```text
absolute and array begin case const div do downto else end external file
for forward function goto if implementation in inline interface label mod
nil not of or overlay packed procedure program record repeat set shl shr
string then to type unit until uses var while with xor
```

`native`, `callerio`, `fullmemory` and `Runtime` have special meanings in native
Unit routines; they are not general Object Pascal constructs.

These names are predefined rather than reserved, so a declaration can shadow them:
the types `Integer`, `Byte`, `Word`, `LongWord`, `Boolean`, `Char`, `Real` and
`Text`; the constants `True`, `False`, `MaxInt`, `Pi` and the sixteen
[color names](crt-library.md#colors); and every name in the
[predefined name index](runtime.md#predefined-name-index).

Spaces, tabs, newlines, other control bytes and comments separate tokens. Both
`{ comment }` and `(* comment *)` are accepted. A comment ends only at the
delimiter matching its opening, so comments do not nest, and the other form can
appear inside one. An unterminated comment is compiler error 91. `//` line
comments are not supported. Directive comments have `$` immediately after the
opening delimiter, for example `{$R+}` or `(*$R+*)`.

| Form | Meaning |
| --- | --- |
| `123`, `-42` | Decimal integer; a sign is an operator |
| `$2A`, `$FFFF` | Hexadecimal, one to eight digits; 16-bit forms retain Integer bit-pattern semantics |
| `1.5`, `2E3`, `-1.25E-2` | Real literals; decimal only |
| `'text'`, `'Don''t'`, `''` | Byte string; doubled quote embeds a quote. One character is a Char; `''` is the empty string |
| `#65`, `#$41`, `^G` | Character code 0..255, hexadecimal character code, control character |
| `'Line'#13#10` | Adjacent character/string fragments make one literal |
| `:=`, `<>`, `<=`, `>=`, `..` | Assignment, comparison and range tokens |
| `(.` and `.)` | Alternative spellings of `[` and `]` |

Characters are bytes, not Unicode. A literal cannot cross a physical source
line, and one literal holds at most 255 bytes. Build long values with `+` across
several lines; String expression values cannot exceed 255 bytes.

The native source importer accepts LF, CRLF and CR, normalizes source to LF,
recognizes CP/M Ctrl-Z source padding and removes trailing ASCII spaces from
lines. It checks the full physical line before normalization: more than 127
bytes is compiler error 100, even if the extra bytes are spaces. Saved source
uses LF and no CP/M record padding. Data-file bytes have separate rules.

### Numeric literal types

Decimal 0..32767 is Integer, 32768..65535 is Word, and larger values through
4294967295 are LongWord. The spelling `-32768` is accepted. More-negative signed
literals and values above LongWord's maximum are rejected. `$FFFF` is Integer
-1; write `Word($FFFF)` or `65535` for an unsigned value. Hexadecimal values
requiring more than 16 bits are LongWord. See [unsigned arithmetic](unsigned-integers.md)
for casts and promotion, including the difference between `-40000` and negating
a previously declared unsigned constant.

## Program structure and declarations

```text
[program Name [(identifier, ...)];]
[uses UnitName, ...;]
{ label / const / type / var / procedure / function declarations }
begin
  statements
end.
```

The program heading is optional. Names in its parenthesized list do not bind
external files; use `Assign`. Declaration sections can repeat. Define a name
before using it, except for forward pointer targets within a type group and
forward routine declarations. Only spaces and comments can follow the final
period; other text is compiler error 58. Units have their own
`unit/interface/implementation` structure, documented in the
[Unit reference](units.md).

A name is visible from its declaration to the end of its block, including nested
routines, unless a nested block declares the same name. Unit exports can be
qualified, as in `Calc.Twice`.

### Constants

An ordinary constant names a literal or an earlier constant, optionally signed:

```pascal
const Limit = 10;
      Caption = 'Count';
      Minimum = -32768;
```

An ordinary `const` declaration is not a general compile-time expression
language: `const Limit = 2 + 3;` is rejected. Such constants can be bounds,
labels and literal operands where their type permits. `True`, `False`, `MaxInt`,
`Pi` and the color names can be used wherever a constant is expected.

Typed constants allocate initialized, writable storage:

```pascal
type TPoint = record X, Y: Integer end;
const Counter: Integer = 0;
      Origin: TPoint = (X: 0; Y: 0);
```

`Counter := Counter + 1` is legal. A typed constant is not a compile-time bound.
In a Memory build its changed value persists across Runs until recompilation.
Structured initializers use parentheses for arrays and declaration-order
`Field: value` entries separated by semicolons for records. Omitted trailing
record bytes are zero-filled. A variant record initializes only the fields of its
selected alternative and must include any tag field. Structured typed constants
cannot contain pointers or files.

### Types, variables and aliases

```pascal
type TIndex = 1..10;
     TValues = array[TIndex] of Integer;
var Values: TValues;
    I, Total: Integer;
```

A type alias names an existing type. Separately declared enumerations and
structured types have distinct identity where assignment or `var` parameters
require it. Ordinary variables should be initialized before reading them;
declaring a variable does not promise a useful initial value.

`var View: SomeType absolute Address;` overlays storage at an Integer logical
address or another variable/parameter's first byte. It names exactly one variable
and allocates no second object. The declared type controls interpretation.
Absolute declarations are low-level aliasing facilities; CP/M addresses do not
acquire Zeal meanings.

## Types and storage

Storage is little-endian and tightly packed; arrays and records add no alignment
padding. `packed` is accepted but does not introduce a separate packed layout.
Use `SizeOf` rather than assuming generated addresses or file control-block sizes.

| Type | Range or meaning | Bytes |
| --- | --- | --- |
| `Integer` | -32768..32767, two's complement | 2 |
| `Byte` | 0..255, Integer family | 1 |
| `Word` | 0..65535 | 2 |
| `LongWord` | 0..4294967295 | 4 |
| `Boolean` | `False` = 0, `True` = 1 | 1 |
| `Char` | Ordinal byte 0..255 | 1 |
| `Real` | Six-byte binary Real, about 11 significant decimal digits | 6 |
| `string[N]` | Length byte followed by N character slots, N = 1..255 | N + 1 |
| Typed pointer | Native 16-bit logical address; `nil` is zero | 2 |
| Enumeration | Ordinals start at zero; one byte through 256 elements | 1 or 2 |
| Subrange | Inclusive constant ordinal bounds | Depends on base/range |
| Set | Ordinal domain contained within 0..255 | `(High div 8) - (Low div 8) + 1`, at most 32 |
| File/Text | Runtime file association and buffers | Currently 176; query with `SizeOf` |

Real is not IEEE single or double. Nonzero magnitudes run from 2^-128 (about
2.9E-39) to just under 2^127 (about 1.7E38). Primitive arithmetic discards low
magnitude bits rather than promising IEEE rounding. Underflow flushes to zero;
overflow is an error. See [Real functions and formatting](runtime.md#numeric-and-ordinal-routines).

### Enumerations and subranges

```pascal
type TDay = (Monday, Tuesday, Wednesday, Thursday, Friday);
     TWorkDay = Monday..Friday;
     TSmall = 0..99;
```

Enumerators have their type's identity, consecutive ordinals and ordering.
`Ord`, `Succ` and `Pred` work with ordinal types. An Integer-family subrange
wholly within 0..255 occupies one byte; otherwise it occupies two. Bounds must
belong to a compatible ordinal family, with lower bound no greater than upper.
`{$R+}` checks assignments against their declared ranges and reports runtime
`$91`. With `{$R-}`, an assignment to a one-byte destination keeps the low byte.

### Strings and character arrays

Every String type specifies its capacity: `string[40]`, not bare `string`.
Index 0 exposes its length byte; characters start at 1. `{$R+}` checks indexes
against 0..capacity and reports runtime `$90`. Assignment truncates a
longer value to destination capacity, while a concatenation or function result
longer than 255 is an error before assignment. Comparisons are lexicographic by
unsigned byte, with a proper prefix preceding its extension.

A Char is compatible with a one-character string. Converting a string to Char
requires current length exactly one, or runtime error `$10` results. An
Integer-indexed Char array of at most 255 elements converts to its full
fixed-length character sequence in expressions, comparisons and String routines.
Assigning to a Char array requires a same-length string constant or the same
declared array type; String variables and computed Strings are rejected. Char
arrays with other index types are ordinary arrays.

`{$V+}` requires matching capacities for `var` String parameters. `{$V-}` relaxes
that check; the caller must still provide enough storage for every access.
String routines and aliasing rules are in the [runtime reference](runtime.md#string-routines).

### Arrays

Index types are ordinals, so a Real index type is invalid; bounds are inclusive.
`{$R+}` checks each index and reports runtime `$90`. Both forms below have the
same layout, with the rightmost index varying fastest:

```pascal
type TGrid = array[1..3, 1..4] of Integer;
     TRows = array[1..3] of array[1..4] of Integer;
```

`Grid[I,J]` and `Grid[I][J]` select the same component. Arrays can contain other
arrays, records, Strings and ordinary data types. Whole-array assignment and
value/`var` parameters are supported when the declared types match. A value
parameter makes an independent copy for the routine body. Array-valued user
functions are not supported. Word indexes are supported; LongWord indexes are not.

### Records, variants and With

```pascal
type TItem = record
       Code: Integer;
       Name: string[20];
       Price: Real
     end;
```

Use `Item.Name`, whole-record assignment, value parameters or `var` parameters.
Record-valued user functions are not supported. Empty records occupy zero bytes.

A variant part follows fixed fields, for example:

```pascal
type TValue = record
       case Kind: Boolean of
         False: (Number: Integer);
         True: (Letter: Char)
     end;
```

Variants share storage sized for the largest alternative. The optional tag is
programmer-managed; changing it does not convert, initialize or validate the
union contents. Omit the tag name, as in `case Boolean of`, for a free union.
A record has one variant part at each level, after its fixed fields.

`with A, B do Statement` makes fields visible, evaluates each record base once,
and lets the last record take precedence. `{$W0}`..`{$W9}`, placed before a
block's `begin`, sets how many With contexts it can hold at once; the default is
four, and exceeding it is compiler error 97. Use qualified fields when names are
ambiguous. `Exit` restores active With context.

```pascal
program Stock;
type TItem = record Code: Integer; Price: Real end;
var Item: TItem;
begin
  with Item do begin
    Code := 7;
    Price := 1.25
  end;
  WriteLn(Item.Code, ' ', Item.Price:0:2)
end.
```

### Sets

```pascal
type TLetterSet = set of 'A'..'Z';
var Letters: TLetterSet;
begin
  Letters := ['A', 'E', 'I', 'O', 'U'];
  if 'E' in Letters then WriteLn('vowel')
end.
```

Sets support `+` union, `*` intersection, `-` difference, `=`/`<>`, `<=` subset,
`>=` superset and `in` membership; `<` and `>` are rejected. Constructors allow
ranges, for example `[1..3, 7]`, and `[]` suits every set type. The base must fit
0..255. Variables retain the bytes intersecting
the declared domain; temporary/value-parameter sets use a 32-byte snapshot.
`var` set parameters require the same declared type. Constructor runtime bounds
use their low eight bits; out-of-domain membership is False. Do not assume
unchecked constructor expressions reject every out-of-range value. Word set
constructors are not supported. Typed constant elements must fit their base.

### Pointers

```pascal
type PNode = ^TNode;
     TNode = record Value: Integer; Next: PNode end;
var P: PNode;
```

Forward targets must resolve within that type group. `P^`, `P^.Value` and
repeated dereferencing select the target. Pointers to the same target type can
be assigned, compared for equality/inequality and passed by `var`; `nil` is
compatible with pointer types. No automatic validity check makes a dangling or
arbitrary address safe. Use [heap routines](runtime.md#heap-and-block-memory).
`Ptr` constructs an address value; pointer type casts and pointer constants are
rejected. `Ord(P)` or `Word(P)` gives a pointer's address. Assign a pointer-valued
function result to a pointer variable before dereferencing it.

### Files

`Text`, `file of T` and untyped `file` have different operations and external
formats. File variables cannot be copied by assignment, returned as functions,
or passed by value. A component cannot itself be a file. See the complete
[file reference](runtime.md#files).

### Assignment and type compatibility

| Destination | Accepted value |
| --- | --- |
| Integer, Byte, Integer subrange | Any Integer-family value; see [unsigned conversions](unsigned-integers.md#conversion-and-range-checking) for Word/LongWord sources |
| Word, LongWord | See [Word and LongWord](unsigned-integers.md#conversion-and-range-checking) |
| Real | Real or Integer-family value; Word and LongWord do not convert automatically |
| Char | Char, or a String whose current length is one |
| `string[N]` | String, Char or convertible Char array; longer values truncate |
| Enumeration, Boolean | Same type |
| Pointer | Pointer to the same target, `nil` or `Ptr(...)` |
| Set | Compatible base type or `[]` |
| Array, record | Same declared type |
| File | Never assignable |

A value parameter accepts what assignment accepts. A `var` parameter is stricter:
Byte does not match Integer, and structured/scalar types normally must match.
Compatible pointer targets and file types are accepted; String formals require
matching capacities with `{$V+}`, relaxed by `{$V-}`.

## Expressions

| Precedence, high to low | Operators |
| --- | --- |
| Factor | Parentheses; unary `+`, `-`, `not`; selectors and function calls |
| Multiplicative | `*`, `/`, `div`, `mod`, `and`, `shl`, `shr` |
| Additive | `+`, `-`, `or`, `xor` |
| Relational | `=`, `<>`, `<`, `<=`, `>`, `>=`, `in` |

Use parentheses to make intent explicit. Operators at one binary level associate
left to right. An expression holds at most one relational operator unless
parentheses group it, so write `(A < B) and (B < C)`. Unary minus is a factor:
`-A * B` means `(-A) * B`.

| Operator | Operands | Result |
| --- | --- | --- |
| `+`, `-`, `*` | Integer family, Word, LongWord, Real | Integer/Real arithmetic or unsigned arithmetic; see the mixing rule below |
| `/` | Integer family, Real | Real. Word and LongWord are rejected |
| `div`, `mod` | Integer family, Word, LongWord | Integer division and remainder |
| `shl`, `shr` | Integer family, Word, LongWord | Logical shift; `$FFFF shr 1` is `$7FFF` |
| `and`, `or`, `xor`, `not` | Boolean, or Integer family, Word, LongWord | Boolean logic, or bitwise |
| `+` | String, Char, Char array | Concatenated String |
| `+`, `-`, `*` | Compatible sets | Union, difference, intersection |
| `=`, `<>` | Compatible scalars, strings, pointers, sets | Boolean |
| `<`, `<=`, `>`, `>=` | Compatible ordinals, Reals, strings | Boolean. Sets allow `<=` and `>=` only |
| `in` | Ordinal, then a set | Boolean |

Real arithmetic mixes with Integer-family values only; mixing Real with Word
or LongWord is rejected rather than automatically converted.

Whole arrays and records cannot be compared, except Char arrays that convert to
strings. Boolean `and` and `or` evaluate both operands: they do not guard an
invalid pointer or a division. Use nested `if` statements for conditional access.

Signed Integer arithmetic wraps in 16 bits; range checking is not a general
overflow-checking mode. Integer `div` truncates the quotient toward zero, and the
sign of `mod` follows the sign of the quotient. Dividing by zero is runtime `$02`.

| Expression | `div` | `mod` |
| --- | ---: | ---: |
| `7`, `3` | 2 | 1 |
| `-7`, `3` | -2 | -1 |
| `7`, `-3` | -2 | -1 |
| `-7`, `-3` | 2 | 1 |

Unsigned promotion changes division and comparisons; consult the
[Word/LongWord rules](unsigned-integers.md) before mixing signed and unsigned.

An ordinal cast uses `TypeName(Expression)`, for example `Char(65)` or
`Integer(SomeOrdinal)`, and follows the destination's checks. The source must normally be
ordinal, or a one-character String for a Char cast; `Word(PointerValue)` is the
explicit pointer-address exception. Real and pointer type-cast
destinations are rejected; use Trunc/Round for Real and Ptr for an address. The
unsigned extensions have their own explicit representation-cast rules and do not
add unsigned Boolean/enum conversions.

An assignment uses `:=`, not `=`. Structured assignments copy storage rather
than bind an alias. `Addr` and low-level memory aliases can intentionally share
storage. Function calls with side effects should not be used to infer a
mathematical simplification or short-circuit rule.

## Statements

| Form | Operation |
| --- | --- |
| `begin S1; S2 end` | Compound statement; declarations precede statements |
| `if B then S1 else S2` | Boolean test; `else` belongs to the nearest unmatched `if` |
| `case E of C: S; ... else S end` | Ordinal dispatch, comma lists and ranges of constant labels; optional Else |
| `while B do S` | Pre-test loop; may execute zero times |
| `repeat S1; S2 until B` | Post-test loop; executes at least once |
| `for I := A to B do S` | Inclusive ascending ordinal loop |
| `for I := A downto B do S` | Inclusive descending ordinal loop |
| `with R do S` | Temporary record-field scope |
| `goto LabelName` | Transfer to a label in the current block |
| `Exit` | Return from current routine, or finish the main program |
| `Halt` | Terminate the program; no status argument |
| `Inline(...)` | Emit machine code; see [native machine access](#native-machine-access) |
| *(empty)* | Does nothing; `begin end` is valid |

The For control is a simple ordinal variable identifier, not a selected field
or indexed element. Bounds are evaluated once before the loop. Do not assign the control variable
inside its loop; passing it as `var` is compiler error 44. A zero-trip loop leaves
the control variable unchanged, and normal completion leaves it at the final
bound. Word controls/selectors work; LongWord For/Case does not.

Case selectors are ordinal, and label types must match the selector. At least
one labelled arm is required. The semicolon before Case Else is optional. Without an
`else`, a value that matches no label does nothing. The `else` part can hold
several statements.

```pascal
program Classify;
var I: Integer;
begin
  for I := 0 to 3 do
    case I of
      0: WriteLn('zero');
      1, 2: WriteLn('small');
    else
      WriteLn('other')
    end
end.
```

A `label` declaration precedes its labelled statement. Labels may be identifiers
or decimal digit names; `01` and `1` are distinct spellings. An unused label needs
no definition. A `goto` stays in the current routine block, forward or backward.
It can leave a For or With scope, but entering a deeper For is compiler error 71.

## Procedures and functions

```pascal
procedure AddTo(var Total: Integer; Amount: Integer);
begin
  Total := Total + Amount
end;

function Twice(Value: Integer): Integer;
begin
  Twice := Value * 2
end;
```

Separate parameter groups with semicolons. Arguments are evaluated left to right.
Value parameters are local copies; `var` parameters alias writable caller storage
and follow [the reference-type rules](#assignment-and-type-compatibility). Every typed formal
uses a previously named type, so declare a type name for a String or structured
parameter. File parameters must be `var`.

A function sets its result by assigning to the function name; the last executed
assignment wins. Scalar, pointer, Real, LongWord and named String results are
supported. Arrays, records, sets and files cannot be returned.

A `var` parameter with no type accepts any variable. Inside the routine it can
only be used as raw storage, such as with `Move`, `FillChar`, `Addr`, `BlockRead`,
`BlockWrite`, `absolute` or forwarding to another untyped parameter:

```pascal
program ClearBytes;
var Data: array[1..4] of Byte;
procedure Clear(var Buffer; Size: Integer);
begin
  FillChar(Buffer, Size, 0)
end;
begin
  Data[1] := 9;
  Clear(Data, SizeOf(Data));
  WriteLn(Data[1])
end.
```

Routines may be nested and access enclosing declarations. Recursion requires
recursive workspace, enabled with `{$A-}` before the routine declaration; `{$A+}`
is the default static-workspace convention and is not reentrant storage. Under
`{$A-}`, do not pass a local as a `var` argument into a recursive call that saves
and restores that same workspace.

```pascal
program Factorials;
{$A-}
function Factorial(N: Integer): Integer;
begin
  if N <= 1 then Factorial := 1
  else Factorial := N * Factorial(N - 1)
end;
{$A+}
begin
  WriteLn(Factorial(7))
end.
```

Declare `procedure Name(...); forward;` or the equivalent function heading when
a call must precede the body. In a program the completing heading must contain only the name and semicolon,
using the recorded parameters/result. Repeating parameters or a function result
type there is error 1. Unit implementation headings have their separate repeated-heading rules. Unresolved forwards are compile errors.

```pascal
program Forwarded;
function Twice(Value: Integer): Integer; forward;
function Twice;
begin
  Twice := Value * 2
end;
begin
  WriteLn(Twice(21))
end.
```

`Addr` can obtain a Pascal-callable routine address for supported callbacks.
A nested callback captures an active owner and must not outlive it. There is no
general procedural-parameter or procedure-variable type syntax. The runtime
supports selected output/status hooks, described in [runtime state](runtime.md#runtime-state-and-driver-hooks).

## Compiler directives and includes

A directive is a comment with `$` immediately after its opening delimiter:
`{$R+}` or `(*$R+*)`. With a space, as in `{ $R+}`, it is an ordinary comment.
Letters are case-insensitive. Multiple switches can be comma-separated, such as
`{$R+,I-}`. Their settings affect subsequently compiled code. Unsupported
switches report compiler error 93 rather than silently enabling a modern Pascal
feature.

| Directive | Default | Effect |
| --- | --- | --- |
| `{$A+}` / `{$A-}` | A+ | Static routine workspace / save and restore recursive workspace |
| `{$B+}` / `{$B-}` | B+ | Fresh default buffered Read / continue buffered input stream |
| `{$C+}` / `{$C-}` | C+ | Start the program with `CBreak` True / False; see below |
| `{$I+}` / `{$I-}` | I+ | Checked I/O abort / explicit IOResult handling |
| `{$R+}` / `{$R-}` | R- | Ordinal/index checks enabled / disabled |
| `{$U+}` / `{$U-}` | U- | Ordinary execution Ctrl-C polling enabled / disabled |
| `{$V+}` / `{$V-}` | V+ | Strict / relaxed `var` String-capacity matching |
| `{$W0}`..`{$W9}` | W4 | Per-block With-depth allowance |
| `{$X+}` / `{$X-}` | X+ | Faster / smaller array-index scaling |
| `{$I path}` | None | Include source at this location |
| `{$RUNTIME REAL}` | Demand-driven | Explicitly retain the full Real runtime |

Place `{$C+}` or `{$C-}` before the first declaration. It is read at entry
to the program block, to choose the initial
value of [`CBreak`](runtime.md#runtime-state-and-driver-hooks). A later `{$C}` does
not reassign it; set `CBreak` directly to change it while the program runs.

`{$I+}` and `{$I-}` are the checking switch; any other text after `{$I` is an
include. `{$I parts/math.inc}` belongs on its own line, and cannot share a comment
with switches. Relative paths resolve from the
containing source's directory; a missing extension defaults to PAS. Native
filename case is preserved. One include level is supported; nested/self includes
are rejected. A missing file is compiler error 90. Includes are read from disk, so
save edited include Work before compiling Main. The IDE performs that save/reload
workflow when needed. A Unit must stay in one source file.

Switches and the With limit changed inside an include return to the containing
file's values when it ends, except `{$B}`: its last value persists across includes.

Normal Real operations automatically request their helpers. Raw Inline code
whose private assumptions require the full Real runtime must explicitly request
it; raw memory access alone is not a Real-use declaration.

## Native machine access

`Mem[Address]` is a byte at a 16-bit native logical address. `Port[Address]` reads
or writes an 8-bit Z80 port using a 16-bit port address, with Integer or Word
indexes; a Port write sends the low byte of an Integer or Word value. `Mem[I]` is
addressable Byte storage and can be passed by `var` or used with `Addr`, `SizeOf`,
`Move` and `FillChar`. `Port[I]` is an I/O operation only: it cannot be passed by
`var` or have its address taken. Neither can be used as a whole array. `Addr`,
`Ptr`, `SizeOf`, `Move` and `FillChar` support ordinary storage inspection and
copying. Arbitrary memory/port writes can corrupt the running program, IDE or
hardware state; range checks do not turn them into protected operations.

`Inline($00)` emits a Z80 NOP. Elements separated by `/` are literals, constants,
storage/routine names, `*` for the current emission position, and supported
`+`/`-` address expressions. A constant in 0..255 emits one byte; other values and
all address operands emit a little-endian word. Prefix `<` forces the low byte
and `>` forces a word; `<` and `>` are emission widths, and neither selects a high
byte. A named variable denotes its storage address, not its current value. A
`var` formal's slot contains a pointer. Keep internal branches relative to
movable code. Arbitrary assembler mnemonics are not accepted inside Inline.

`procedure Name(...); external Address;` declares an existing native entry.
The address is an Integer-family constant, so write high addresses in hexadecimal.
External declarations cannot complete a `forward` and are not allowed in Units. An
Integer address is not a safe interface to arbitrary CP/M, SDK C or kernel
routines. Use [ZOS](zos-syscalls.md) for OS access. Native Unit leaves, `callerio`,
`fullmemory` and symbolic `Runtime(Name)` operands are documented in
[the Unit guide](units.md#native-implementation-leaves) and
[hardware access](unit-hardware.md).

### Calling convention for External and Inline

Arguments are pushed left to right, so the last is nearest the return address. A
nested routine receives its lexical-parent link before the arguments. The callee
removes its arguments.

| Argument | On the stack |
| --- | --- |
| `var` parameter | 2-byte address |
| Integer family, Word, Char, Boolean, enumeration, pointer | 2-byte word; one-byte values are zero-extended |
| LongWord | 4 bytes |
| Real | 6 bytes |
| Set | 32 bytes |
| String | Length byte and current characters, at most 256 bytes |
| Array, record | 2-byte address of the source; the callee copies it |

Scalar results of at most two bytes and pointer results return in HL; Real
results use HL for bytes 0–1, DE for 2–3 and BC for 4–5. A LongWord result returns
HL pointing to four readable result bytes. Compiler-generated routines use a
private runtime result buffer; consume/copy it before another call reuses that
storage. External code must provide result storage with an adequate lifetime. A String result uses a slot the caller
reserves before its arguments. Machine
code must preserve SP, IX and IY, and restore any changed memory mapping.

## Syntax summary

Braces mean repetition, square brackets an optional part, and quoted text a
literal token. The prose sections add semantic rules. Unit source files use the
[Unit structure](units.md#creating-a-unit) instead of `program`.

```text
program      = [ "program" id [ "(" id-list ")" ] ";" ]
               [ "uses" id-list ";" ] block "." ;
block        = { declaration } compound ;
declaration  = "label" label { "," label } ";"
             | "const" { const-def ";" }
             | "type" { id "=" type ";" }
             | "var" { id-list ":" type [ "absolute" address ] ";" }
             | routine ;
const-def    = id "=" constant | id ":" type "=" typed-value ;
constant     = [ "+" | "-" ] ( number | constant-id ) | string | ordinal-id ;
label        = id | digits ;

type         = type-id | ordinal-type | "^" type-id
             | "string" "[" constant "]"
             | [ "packed" ] "array" "[" ordinal-type { "," ordinal-type } "]" "of" type
             | [ "packed" ] "record" field-list "end"
             | [ "packed" ] "set" "of" ordinal-type
             | [ "packed" ] "file" [ "of" type ] ;
ordinal-type = type-id | "(" id-list ")" | constant ".." constant ;
field-list   = [ fixed-fields ] [ variant-part ] ;
variant-part = "case" [ id ":" ] type-id "of"
               case-label ":" "(" field-list ")"
               { ";" case-label ":" "(" field-list ")" } ;

routine      = heading ";" ( block | "forward" | "external" constant ) ";"
             | ( "procedure" | "function" ) id ";" block ";" ;
heading      = "procedure" id [ formals ]
             | "function" id [ formals ] ":" type-id ;
formals      = "(" section { ";" section } ")" ;
section      = [ "var" ] id-list ":" type-id | "var" id-list ;

compound     = "begin" statement { ";" statement } "end" ;
statement    = [ label ":" ] ( empty | assignment | call | compound | if
             | case | for | while | repeat | with | goto | inline ) ;
assignment   = variable ":=" expression ;
call         = qualified-id [ "(" expression { "," expression } ")" ] ;
if           = "if" expression "then" statement [ "else" statement ] ;
case         = "case" expression "of" arm { ";" arm }
               [ [ ";" ] "else" statement { ";" statement } ] [ ";" ] "end" ;
arm          = case-label ":" statement ;
case-label   = range { "," range } ;
range        = constant [ ".." constant ] ;
for          = "for" id ":=" expression ( "to" | "downto" ) expression
               "do" statement ;
while        = "while" expression "do" statement ;
repeat       = "repeat" statement { ";" statement } "until" expression ;
with         = "with" variable { "," variable } "do" statement ;
goto         = "goto" label ;
inline       = "inline" "(" element { "/" element } ")" ;

variable     = qualified-id { "[" expression { "," expression } "]" | "." id | "^" } ;
expression   = simple [ relop simple ] ;
simple       = term { addop term } ;
term         = factor { mulop factor } ;
factor       = number | string | variable | function-call | type-id "(" expression ")"
             | set | "nil" | "(" expression ")" | ( "+" | "-" | "not" ) factor ;
set          = "[" [ member { "," member } ] "]" ;
member       = expression [ ".." expression ] ;
addop        = "+" | "-" | "or" | "xor" ;
mulop        = "*" | "/" | "div" | "mod" | "and" | "shl" | "shr" ;
relop        = "=" | "<>" | "<" | ">" | "<=" | ">=" | "in" ;
```

The name-only routine alternative completes an earlier forward declaration.
The following auxiliary productions complete the summary; lexical tokens
`id`, `digits`, `number`, `integer` and `string` follow the source-text rules.
Identifiers such as `type-id` and `constant-id` name earlier declarations.

```text
id-list      = id { "," id } ;
qualified-id = id [ "." id ] ;
function-call = qualified-id [ "(" expression { "," expression } ")" ] ;
fixed-fields = id-list ":" type { ";" id-list ":" type } [ ";" ] ;
typed-value  = constant | "[" [ range { "," range } ] "]"
             | "(" typed-value { "," typed-value } ")"
             | "(" id ":" typed-value { ";" id ":" typed-value } ")" ;
address      = [ "+" | "-" ] ( integer | constant-id ) | variable-id ;
element      = [ "<" | ">" ] operand { ( "+" | "-" ) operand } ;
operand      = [ "+" | "-" ] ( integer | constant-id | storage-id | routine-id )
             | "*" | "Runtime" "(" id ")" ;
```

Qualified names distinguish an imported Unit export from a local declaration;
ordinary record selectors follow the same dot spelling but have record semantics.
Inline operands and native Unit leaves retain the restrictions described above.

`Read`, `ReadLn`, `Write`, `WriteLn` and `Str` have their own argument forms; see
the [runtime reference](runtime.md#text-input-and-output).

## Limits and unsupported language features

The compiler uses physical RAM for growing symbols/types; workspace remains
finite and reports error 99 when exhausted. Static storage/code and stack needs
also constrain each native program. Current memory/window rules belong to the
[Unit memory guide](units.md#program-memory-layout), not fixed CP/M addresses.

This compiler does not supply objects/classes, exceptions, generics, dynamic
arrays, managed/Unicode strings, open-array parameters, overloaded routines,
operator overloading, arbitrary constant expressions, record/array/set function
results, LongWord For/Case/indexing, or modern unit finalization.
Saved COM programs support [overlay routines and OvrDrive](overlays.md), using
MMU banks and demand-loaded native companions. Memory compilation and the
combinations listed in that guide remain unavailable. External CP/M BDOS/BIOS
calls are not available; use native interfaces instead. See the
[compatibility guide](compatibility.md) for the complete target boundary.
