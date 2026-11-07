# Word and LongWord

The native compiler provides `Word` (0..65535, two bytes) and `LongWord`
(0..4294967295, four bytes). `Integer` remains signed 16-bit and `Byte`
remains unsigned 8-bit. Records and arrays store these values little-endian,
without padding. Units can export aliases, record fields and constants using
both unsigned types. LongWord value parameters and function results retain
all four bytes; `var` parameters refer to the original value.

## Literals and arithmetic

Positive decimal literals through 32767 are Integer; 32768..65535 are Word;
larger values through 4294967295 are LongWord. Larger literals fail compilation.
Four-digit hexadecimal literals retain the established Integer bit-pattern
behavior: `$FFFF` is -1. Use `65535` or `Word($FFFF)` for an unsigned mask.
Hexadecimal values requiring more than 16 bits are LongWord. Negative decimal
literals below -32768 are rejected. Use explicit casts when constructing an
unsigned representation of a negative Integer.

Arithmetic wraps modulo 65536 or 4294967296. In a mixed expression, Word
promotes Integer to its unsigned 16-bit bit pattern; LongWord promotes Word
and Byte by zero extension and Integer by sign extension. Comparisons,
`div` and `mod` then operate on those unsigned representations. For example,
with `W:Word=5` and `I:Integer=-1`, `W<I` is True and `-10 div W` is 13105.
Use explicit conversions when mixing signed and unsigned quantities.
A shift count does not widen its left operand. Shifts at or above that operand's
bit width produce zero. Division by zero reports runtime error 2.

## Conversion and range checking

Explicit casts truncate, zero extend or sign extend the representation:
`Word(-1)` is 65535 and `LongWord(-1)` is 4294967295. Explicit narrowing
casts truncate even under `{$R+}`. Implicit narrowing checks the destination
range under `{$R+}` and reports runtime error $91; under `{$R-}` it truncates.
Negative Integer assignments to unsigned destinations also fail under `{$R+}`.
Typed constant initializers must fit their destination and cannot implicitly
convert negative signed Integer constants to unsigned values. Negating an
already unsigned constant follows modular unsigned arithmetic: with
`const Big=40000`, `-Big` is Word 25536. This differs from the out-of-range
signed literal `-40000`.

Explicit casts between Integer, Byte, Word, LongWord and Char accept unsigned
values; Boolean and enum casts remain unsupported. Implicit unsigned-to-Char
assignments and arguments are rejected. `Chr` checks its 0..255 result under
`{$R+}`. `Hi`, `Lo`, `Swap`, `Odd`, `Succ`, `Pred`, `Abs` and `Sqr` accept unsigned
values. Word subrange `Succ` and `Pred` retain range checking.

For Word, `Hi` and `Lo` select bytes and `Swap` exchanges bytes. For LongWord,
`Hi` and `Lo` select Words and `Swap` exchanges the two Words. `Write`,
`WriteLn` and `Str` print unsigned decimal values and support field widths.

## Initial limits

Word supports `for`, `case` and array indexes. Case labels must fit the
selector's signedness and width; no LongWord label is accepted. LongWord
`for`/`case` and array indexes are not supported. `Mem` and `Port` require
16-bit addresses. Set constructors do not accept Word expressions.
Direct unsigned `Read`/`ReadLn`, `Val` and automatic Real conversions are
not implemented; these forms fail compilation. Parse into an appropriate
supported type before explicitly converting, or handle a wider input manually.

## Memory and saved programs

Ordinary Word addition, subtraction, multiplication, bit operations, shifts and
comparisons use existing integer instructions/helpers. They do not include the
wide arithmetic engine. Unsigned division and LongWord arithmetic include that
engine only when used. Units store compile-time metadata on disk and contribute
only reachable code to a saved executable. Saved programs need no ZPU at run
time. Compiler code still occupies 18 logical banks, allocated as 10 cold and
15 warm pages; this stage adds no compiler code page.

## Checkpoint verification

[Evidence](unsigned-integers-evidence.json) records1,188 native unsigned checks,
26,004 generated Z80 arithmetic checks,80 sanitized unit decoder checks,1703
unit/Real checks,623 initialization checks,404 general unit checks, CRT and
host regressions. The simple Word fixture retains two execution pages. A
source-free imported constant-return function and Swap also run with the shared
stack; ordinary local routine calls still conservatively require full-memory
mode. The integrated production launcher is16,031 bytes and its ZPL277,338 bytes, in the
existing18 logical banks.

Opus completed its follow-up and found additional conversion and scalar
initializer issues. The fixes and expanded native checks pass; the current integration remains
unapproved after changes made following the last review turn. ZVB's library, documentation and demos are a separate milestone.
