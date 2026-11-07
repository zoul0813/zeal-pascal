# Unit initialization

> Historical checkpoint/proposal. Its limits, feature status and measurements
> describe that stage. Use [Pascal units and ZPU libraries](units.md) for current
> support, authoring instructions and resource limits.

This deliberate Zeal extension follows the dependency-first initialization
contract recorded in library-exploration.md. A unit may end its implementation
with `begin ... end.` instead of `end.`. Its declarations stay global; the body
is compiled as a private anonymous procedure. `Exit` returns from initialization,
and root labels belong to that procedure. No finalization syntax is introduced.
A syntactically empty body (including empty statements) has no initializer.

Each program activation, including another Memory Run, calls initializers once
per canonical unit. Dependency manifests are traversed before their user, in
recorded order, with a visited bit in compilation state. Direct program uses
order determines order where dependencies permit it. Unit authoring records
only its own initializer and dependency manifest; it does not run initializers.
The existing Chain activation prefix precedes these calls. Each CHN initializes its own image-local copy of a unit. Parent unit state is
not visible to it; shared parent/Chain unit storage is not a supported contract.

The linker treats every initializer call as a root, even when no export from
that unit is referenced. Its transitive code/data references and Real flags
are retained through the ordinary fragment plan. Calls run before the main
program block. Initializers share each unit's canonical private storage with
its routines. Re-running executes the body again; it does not introduce an
automatic reset for mutable globals or typed constants beyond those explicit
assignments. Saved executables contain the retained code and data and have no
ZPU or Pascal-source runtime dependency. Source maps retain initializer locations.

ABI1 header word46 contains zero or the initializer code-fragment index+1.
The entry is at offset0. The loader rejects an out-of-range index or the globals
data fragment. Checksums cover this field. The48-byte header and stock CRT
artifact (zero initializer) remain unchanged. This unreleased format evolves
in place; no older ABI readers are retained.

No new resident compiler page is introduced. A retained initializer conservatively
requires the existing three independent execution windows, like a nested unit
call; a no-initializer program retains existing allocation eligibility.
The existing unit-authoring helper restrictions still apply to initializer bodies.

The focused target passes623 native assertions and17 Python checks. Regression
qualification passes1703 type/dependency and502 CRT assertions,66 IDE checks,
51 failure/cancellation/retry checks and52 host ASAN/UBSAN assertions. See
[unit-init-evidence.json](unit-init-evidence.json) for captures, hashes and program
measurements; the live review record is
[unit-initialization-v1](../reviews/unit-initialization-v1.md). The older unit target also passes404 assertions and navigation passes15 checks.
The focused orchestrator passes its sanitized host prerequisites,24 runner tests
and6 chunk tests. The review record discloses build/test corrections verified
locally after its second turn.

Production launcher size remains15546 bytes. The ZPL is255407 bytes,1723 more
than the preceding review-fix checkpoint, with the same ten cold/fifteen warm
code-page budgets. The empty no-I/O client uses two execution pages; the retained
initializer clients use three. Tests are registered as tools/test-unit-init.py
and share production compiler objects. This is not a claim of full TP4–TP7
recovery or hardware qualification.
