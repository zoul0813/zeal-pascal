# Exported compile-time unit metadata

This checkpoint adds ordinary `const` declarations to the public interface.
Integer, Boolean, Char, String and Real values are stored in the compiled unit.
Clients and other units can use them with qualified or unqualified names after
`uses`, without opening the original source. Private implementation constants
stay private. Imported constants do not retain executable fragments.

String data is length-prefixed and Real data uses the native six-byte value.
Imported literals use immutable artifact references in the existing compiler
reference arena, so re-exporting them does not depend on a parser cursor or
source-file lifetime. Export records have an explicit role, type ID, optional
parameter list and bounded literal payload. The loader validates every record,
including unused constants. This unreleased container continues to use ABI1.

The unit keywords `unit`, `interface`, `implementation` and `uses` are reserved
in the unit-enabled compiler. Typed constants remain unsupported in interfaces;
they represent mutable storage, rather than these compile-time values. Public
custom type metadata is the next checkpoint. Unit initialization remains out
of scope. The native suite passes 1,337 assertions, including source-free String and Real
re-export and saved execution after deleting both unit artifacts. Independent
Opus review remains pending because the account usage limit prevented review.
