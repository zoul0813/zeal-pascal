# Errors and diagnostics

Compiler errors, runtime errors, Pascal I/O errors and ZOS statuses are different
categories. Compiler numbers below are decimal. Runtime/I/O tables use hexadecimal
`$NN`; for example I/O `$20` has Integer value 32 in IOResult. A repeated numeric
value in different categories does not identify the same failure.

Jump to: [compiler messages](#compiler-message-catalog),
[common causes](#common-compiler-errors), [runtime errors](#runtime-errors),
[Pascal I/O errors](#pascal-io-errors) or [ZOS status](#native-zos-status).

## Finding and fixing an error

The compiler stops at the first error. Enable explanatory messages at startup
or use the table below, acknowledge the error, and repair its selected source.
Error 98 concerns program storage; error 99 concerns compiler/code/workspace
capacity. Neither is evidence that a valid program needs a syntax change.

Checked runtime/I/O failures report a code and PC. In Memory mode the IDE can
return to the failing source. For saved programs, record the PC, select the
matching source/mode/Start/End, and use **O → F**. See [source lookup](ide.md#errors-and-source-lookup).
Missing or changed Unit source cannot be safely substituted for the compiled text.

`{$I-}` permits IOResult handling of ordinary I/O failures; IOResult returns and
clears the pending code. It does not catch arbitrary runtime failures. Restoring
`{$I+}` is a compile setting for later operations, not a replacement for reading
and clearing a pending result. `{$I-}` does not disable division, range or stack
errors. ZOS returns direct native statuses independently.

### Display conventions

An IDE compiler diagnostic line can be `Compiler error 44`. The source path and
line/column follow separately. IDE runtime diagnostics use the form
`Runtime error NN PC=hhhh`: NN is two hexadecimal digits and hhhh is the
four-digit hexadecimal PC, without a `$` prefix. These letters are field
placeholders, not an actual failing address. Saved programs use
`Runtime error NN, PC=hhhh` or `I/O error NN, PC=hhhh`, with a comma before
`PC`. Preserve the PC printed by a saved program when using source lookup; do not substitute its numeric error code.

## Compiler message catalog

The following entries are the current compiler's explanatory catalog. A numeric
error can still be reported when no explanatory string is available. Missing
numbers are not invitations to infer a DOS Pascal message of that number.

| Decimal code | Explanation |
| --- | --- |
| 1 | ';' expected |
| 2 | ':' expected |
| 3 | ',' expected |
| 4 | '(' expected |
| 5 | ')' expected |
| 6 | '=' expected |
| 7 | ':=' expected |
| 8 | '[' expected |
| 9 | ']' expected |
| 10 | '.' expected |
| 11 | '..' expected |
| 12 | BEGIN expected |
| 13 | DO expected |
| 14 | END expected |
| 15 | OF expected |
| 17 | THEN expected |
| 18 | TO or DOWNTO expected |
| 20 | Boolean expression expected |
| 21 | File variable expected |
| 22 | Integer constant expected |
| 23 | Integer expression expected |
| 24 | Integer variable expected |
| 25 | Integer or real constant expected |
| 26 | Integer or real expression expected |
| 27 | Integer or real variable expected |
| 28 | Pointer variable expected |
| 29 | Record variable expected |
| 30 | Simple type expected |
| 31 | Simple expression expected |
| 32 | String constant expected |
| 33 | String expression expected |
| 34 | String variable expected |
| 35 | Textfile expected |
| 36 | Type identifier expected |
| 37 | Untyped file expected |
| 40 | Undefined label |
| 41 | Unknown identifier or syntax error |
| 42 | Undefined pointer type in preceding type definitions |
| 43 | Duplicate identifier or label |
| 44 | Type mismatch |
| 45 | Constant out of range |
| 46 | Case constant and selector types differ |
| 47 | Operand type(s) does not match operator |
| 48 | Invalid result type |
| 49 | Invalid String length |
| 50 | String constant length does not match type |
| 51 | Invalid subrange base type |
| 52 | Lower bound > upper bound |
| 53 | Reserved word |
| 54 | Illegal assignment |
| 55 | String constant exceeds line |
| 56 | Error in integer constant |
| 57 | Error in real constant |
| 58 | Illegal character in identifier |
| 60 | Constants are not allowed here |
| 61 | Files and pointers are not allowed here |
| 62 | Structured variables are not allowed here |
| 63 | Textfiles are not allowed here |
| 64 | Textfiles and untyped files are not allowed here |
| 65 | Untyped files are not allowed here |
| 66 | I/O are not allowed |
| 67 | Files must be variable parameters |
| 68 | File components can not be files |
| 69 | Invalid ordering of fields |
| 70 | Set base type out of range |
| 71 | Invalid GOTO |
| 72 | Label not within current block |
| 73 | Undefined FORWARD procedure(s) |
| 74 | INLINE error |
| 75 | Illegal use of ABSOLUTE |
| 77 | Overlays unavailable in Memory mode |
| 90 | Source file unavailable |
| 91 | Unexpected end of source |
| 93 | Invalid compiler directive |
| 97 | Too many nested WITH's |
| 98 | Memory overflow |
| 99 | Compiler overflow |
| 100 | Source line exceeds 127 bytes |
| 247 | Cyclic unit dependency |
| 248 | Invalid compiled unit |
| 249 | Unsupported unit feature |
| 250 | Compiled unit file unavailable |
| 251 | Unit runtime profile changed; recompile unit |
| 252 | Ambiguous unit export; qualify its name |
| 253 | Unit interface/implementation mismatch |

## Common compiler errors

Some messages are inherited wording and do not name the actual problem. These
are the situations that most often produce them.

| Code | Typical cause | Fix |
| --- | --- | --- |
| 1 | Missing `;`, a repeated program forward heading, or an expression in an ordinary constant: `const N = 2 + 3;` | Add the semicolon, or write the computed value |
| 12 | `overlay` when compiling to disk; statements with no `begin` | Remove the overlay; check the block structure |
| 23 | `Random` or `ParamStr` given a Word | Convert with `Integer(...)` |
| 41 | Misspelled name; a CRT routine without `uses Crt;`; CP/M `Bdos`/`Bios`; a parameter with an unnamed type | Correct the name, add the Uses clause, or declare a named type |
| 44 | `var` argument of a different type (Byte for Integer); a For control variable passed by `var` | Use the exact declared type; copy the control variable |
| 47 | `/` on Word or LongWord; `<` or `>` between sets; mixing Char with a number | Convert operands explicitly; use `<=` or `>=` for sets |
| 48 | Function returning an array, record, set or an unnamed `string[N]` | Return through a `var` parameter, or declare a named String type |
| 49 | String capacity outside 1..255 | Use a capacity from 1 to 255 |
| 53 | Reserved word used as an identifier | Choose another name |
| 56 | Number out of range, such as `-40000` | Use a value in the type's range, or an explicit cast |
| 58 | Illegal source character, or text after the final `end.` | Remove the character or trailing text |
| 62 | Comparing whole arrays or records | Compare fields or elements |
| 66 | `^` directly after a function call | Assign the result to a pointer variable, then dereference it |
| 67 | File passed by value | Make it a `var` parameter |
| 71 | `goto` into a deeper `for` | Restructure the jump |
| 73 | A `forward` routine has no body | Supply the body in the same block |
| 77 | `overlay` in Memory mode | Remove the overlay |
| 91 | Unterminated comment or unexpected end of file | Close the comment or block |
| 93 | Unknown or malformed directive | See the [directive table](language.md#compiler-directives-and-includes) |
| 97 | Too many active With contexts | Raise `{$W}` before the block's `begin`, or use qualified fields |
| 100 | Physical source line over 127 bytes | Split the line |
| 250 | Missing ZPU, including `SYSFILE.ZPU` | Install SYSFILE.ZPU and ZOS.ZPU beside the loaded `zpascal.zpl`; ordinary user Units may also be beside the source |
| 251 | Stale ZPU | Rebuild it with the current compiler |

## Runtime errors

| Hex code | Meaning |
| --- | --- |
| `$01` | Real overflow. |
| `$02` | Division by zero. |
| `$03` | Negative Sqrt argument. |
| `$04` | Nonpositive Ln argument. |
| `$10` | String expression exceeds 255 bytes, or invalid String-to-Char length. |
| `$11` | Invalid Copy/Delete/Insert position or negative Copy/Delete count. |
| `$90` | Checked array/String index outside its bounds. |
| `$91` | Checked scalar/subrange failure; also invalid Random/Delay argument. |
| `$92` | Trunc/Round outside Integer range, including with range checking off. |
| `$FF` | Heap/recursion/CPU-stack capacity or stack-bound failure. |

Ctrl-C user break is reported as an interruption, not an I/O error to be cleared
and resumed. Arbitrary pointer/Inline corruption is outside the checked-error
contract. A fatal runtime failure does not return to the faulting operation.

## Pascal I/O errors

`IOResult` returns the decimal value.

| Hex code | IOResult | Meaning |
| --- | ---: | --- |
| `$01` | 1 | Assigned disk file not found. |
| `$02` | 2 | File not open/direction disallows input. |
| `$03` | 3 | File not open/direction disallows output. |
| `$04` | 4 | Untyped block file not open. |
| `$10` | 16 | Invalid numeric text input. |
| `$20` | 32 | Disallowed or unavailable logical-device/file operation. |
| `$21` | 33 | Chain/Execute unavailable in direct Memory mode. |
| `$22` | 34 | Illegal lifecycle operation on a preassigned standard file. |
| `$90` | 144 | Typed component-size mismatch or invalid zero-size component. |
| `$91` | 145 | Seek beyond logical end. |
| `$99` | 153 | Unexpected/premature EOF or incomplete required transfer. |
| `$F0` | 240 | Output/flush/commit failure. |
| `$F1` | 241 | Specifically identified directory-full condition. |
| `$F2` | 242 | Typed component index/count would exceed 65535. |
| `$F3` | 243 | Too many open files/resources in the mapped failure category. |
| `$FF` | 255 | File lost or unavailable during required close/finalization. |

An opaque native resource-exhaustion status cannot prove that a directory is
full; it maps to the supported resource category rather than guessing `$F1`.
Native close can release a descriptor even when reporting an error. Retrieve the
status before recovery. Do not blindly repeat Write/BlockWrite after completed
side effects. A failed acquisition during Pascal Close can retain finalization
state for a later Close retry after IOResult; that differs from replaying data.

Programs must Close Pascal files explicitly: terminal/OS cleanup does not flush
all Pascal buffers or finalize logical headers on their behalf. After an error,
a successful earlier operation does not guarantee all intended data reached disk.

**Kernel limits.** The pinned kernel's ZealFS growing-file disk-exhaustion defect
can corrupt control flow. Leave free space when writing large files; Pascal
error handling cannot repair that kernel failure. Its separate false overwrite
status can surface as `$F0` in Pascal Write or typed-header finalization, even
after bytes changed. See [file caveats](runtime.md#pinned-kernel-overwrite-errors).

## Native ZOS status

Checked ZOS calls return their documented status directly, with descriptor calls
using the signed descriptor/error convention. Raw calls expose returned registers.
These values are not Pascal IOResult codes. A kernel error can follow an actual
write side effect on the pinned kernel; read the [ZOS caveats](zos-syscalls.md).

When reporting a problem, retain the error category, numeric code, PC, source,
compiler/build identity, output mode and Start/End settings. Those facts are more
useful than an error number by itself.
