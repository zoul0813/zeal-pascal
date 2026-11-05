# Experimental Pascal units

Worktree: `/private/tmp/zeal-pascal-units`, branch `explore/pascal-units`.
The isolated native author/import path is implemented. Its observed acceptance
is recorded below; this is not production IDE readiness. `zunittest` is an
experimental native target; the normal IDE is unchanged.

The experiment adds source compilation rather than requiring manually authored
linker objects:

```pascal
unit TileMath;
interface
function TileOffset(X,Y:Integer):Integer;
implementation
function TileOffset(X,Y:Integer):Integer;
begin TileOffset:=Y*80+X end;
end.
```

A program imports it with `uses TileMath;` and may call
`TileMath.TileOffset(2,3)` or the unqualified export. Local declarations shadow
imports; ambiguous unqualified exports are errors. Unit names are case insensitive,
with 1–11 characters, and resolve to `UPPERCASE.ZPU` in the program's directory.
No search path or filename truncation is implied.

The native authoring API is `compile_unit(source, scratch, diagnostic, fd,
interruptible)`. The caller supplies a fresh output file and writable descriptor and an empty
Build scratch owner, closes both, and publishes the artifact after success.
The API does not replace an existing library safely on the caller's behalf.
There is no IDE menu command for it yet.

The initial interface accepts procedures/functions with Integer, Byte, Char and
Boolean results/parameters, plus untyped VAR parameters. Full repeated
implementation headings must match interface names, modes and types; the existing
shorthand forward-definition form remains available. Private implementation
routines use the existing compiler. Unit dependencies, initialization, exported
types/variables, source-absent imports and optional runtime-helper dependencies
are outside this first subset. Unsupported constructs must fail compilation.

A `.ZPU` contains primitive export signatures, source identity/fingerprint,
fragment code, references, guard PCs and source maps. It is bounded to one16 KiB
page and bound to unit ABI1 and the exact runtime profile. Its FNV checksum detects
accidental corruption; it is not an authentication or native-code sandbox.
Matching unit source remains required for this experiment's error-location and
build-validity contract.

Libraries stay on disk between compilations. Importing reads and validates their
artifacts in compiler-owned temporary pages and checks matching source files.
It retains source descriptors and fingerprints for error navigation. Linking copies
only reachable code fragments and their dependencies into the program, then
releases the temporary artifact and placement pages. It emits directly into the
compiler staging image, avoiding a duplicate linked image. The runtime prefix is
materialized on first mutation or after linking. The executable owns its copied bytes;
there is no runtime loader for `.ZPU` files. Static unit workspace is currently a
shared data fragment, so one used workspace address can retain all of that unit's
workspace. Recursive invocation retains the compiler's existing activation ABI.

Code bytes saved are not automatically whole RAM pages saved. Allocation changes
only when the resulting image crosses a page boundary. This path does not yet
extract Real or Screen, remove the normal fixed runtime prefix, or combine the
separate compact-layout experiment with units. Code-review findings and dispositions are recorded in
[the milestone review](../reviews/pascal-units-v1.md).


## Observed native acceptance

The [hash-bound evidence](pascal-units-evidence.json) records279 native unit
assertions, all passing. The target authors real Pascal units and executes their
clients in RAM and saved COM form. It executes Integer and Boolean value parameters/results and VAR parameters
for Integer, Byte, Char and Boolean, plus a private typed constant, two imports,
qualified lookup, ambiguity and local routine shadowing. A division-by-zero
inside an imported function resolves to `H:/FAULTS.PAS` and the original statement
offset; the saved image returns the same runtime error status.

The initial TileMath payload is418 bytes. Increasing only its unused body raises
that payload to798 bytes (the full artifact grows from863 to1843 bytes). The
resulting program remains byte-for-byte identical at17634 bytes. This proves
actual Pascal-generated unused code is omitted. The client prints its expected
marker in both RAM and COM runs, then again through direct SDK execution after
both imported unit artifacts and source files are removed. No whole RAM page
boundary is crossed by this fixture.

Negative tests cover unsupported Real exports, mismatched/duplicate interface
headings, missing implementations, initialization, missing artifacts, stale
sources, truncated artifacts, qualified misses, include/descriptor aliasing,
and runtime-helper dependencies (system variables, heap operations, console,
Text, keyboard, random, CRT and Inline). Each authoring test checks page cleanup;
the host compares captured compiler bytes before and after all executions.
Byte/Char value parameters and results and untyped VAR are accepted by the
implementation but are not exercised by these positive unit fixtures. Private
helpers, transitive/nested unit calls, recursion and compound unit statements
also need direct unit-level acceptance. The ordinary-program corpus does not
substitute for those checks. A known conservative rejection remains: declaring
a local name that shadows a restricted runtime symbol (such as BufLen) can mark
the unit unsupported. Fixed public runtime services such as IOResult remain
available; IOResult clears the program's shared I/O status, just as in an ordinary
routine. The optional-helper gate does not make library routines pure.
Container fuzzing, all resource limits and injected unit-loader cleanup failures
are not exhaustively covered by this acceptance fixture.

The experimental compiler also passes1982 assertions from15 existing language
families: control flow, ordinals, cases, routines, strings, string operations,
arrays, records, sets, pointers, heap, memory, labels, Reals and formatting.
These exercise ordinary programs with the deferred runtime prefix and new lookup
wrapper. Media and interactive input families are outside that regression run.
The normal macro-off compiler passes its64 control-flow and75 routine assertions.
The fragment layer passes862 sanitized host and801 native assertions, including
embedded offsets, identity binding, plan-only placement and cleanup retries.
The normal runtime and file-runtime payload hashes remain unchanged.

Reproduce using the existing development environment:

```sh
zde --no-tty cmake --target rebuild_cache
zde --no-tty cmake --target zpascal_bin zptest_bin --parallel 6
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-units.py
ZP_ZEAL_ROOT=/Users/higginsd/Projects/zeal python3 tools/test-unit-corpus.py
```

These are incremental native builds with retained source, executable, emulator
and ROM hashes, not clean-toolchain reproducibility or physical-hardware proof.
Publishing units from the IDE and integrating worker-owned authoring remain
separate work. The next verification step is exercising private unit implementations and fixing
restricted-name shadowing. IDE authoring and optional runtime extraction follow.
