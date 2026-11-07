# Inter-unit dependency checkpoint

A unit can declare `uses` after `interface` and after `implementation`. The
same compiled-unit loader is used by programs and unit authoring. Dependencies
are resolved beside the compilation root, then beside the loaded compiler bank
container. Source text remains optional. Dependency-first executable startup is
now described in [unit-initialization.md](unit-initialization.md).

Each invocation loads one canonical artifact per case-insensitive unit name.
Dependency manifests bind the exact compiled artifacts used to build the unit.
Changed or conflicting dependency artifacts report a stale-unit diagnostic;
missing artifacts and cyclic dependencies receive separate diagnostics. Rebuild
users of a changed unit in dependency order. This unreleased format still uses
ABI1; no reader for the previous experimental layout is retained.

Dependencies do not automatically re-export their namespaces. A program using
LeftDep cannot refer to Shared merely because LeftDep uses Shared. An explicit
program `uses Shared` exposes its public names and reuses the already-loaded
module, including one copy of its private state. The linker retains only code
reachable from program calls and fragment references, across the whole graph.
Saved programs retain their code and have no ZPU or source dependency at runtime.

The generic fragment engine accepts invocation-local identity bindings outside
immutable artifact bytes. Its structural validation precedes dependency loading;
full target validation occurs after binding the graph. Binding space reserves
64 bytes of each 16 KiB cache page, limiting ZPU artifacts to 16320 bytes. The
catalog supports at most 32 loaded modules, subject to actual available memory;
unit artifacts can record at most 31 dependencies. Resource exhaustion fails
through existing compiler cleanup rather than borrowing or losing owned pages.

The final linker coordinator occupies a transient eighteenth code bank.
Dependency traversal and the fragment engine share the retained unit bank,
so loading a dependency needs only the transient object I/O bank. Final
linking temporarily releases the idle file compiler bank and restores it before
helper generation, leaving room for the placement catalog. Startup remains ten code
pages and the optional compiler warm set remains fifteen. Nested inter-unit
calls conservatively request independent execution windows; only leaf calls
retain the shared-page allocation eligibility.

The target dependency/Real suite passes 1,200 assertions, including source-free
three-module diamond execution, one shared private counter, namespace visibility,
missing/stale/cyclic dependencies, and saved execution after library deletion.
The host fragment suite passes 876 assertions under ASAN and UBSAN. This
dependency-only checkpoint preceded [exported constants](unit-exports.md) and
[custom types](unit-types.md), now implemented in subsequent stages. The integrated review and its dispositions are tracked in
[unit-dependencies-v1](../reviews/unit-dependencies-v1.md).
