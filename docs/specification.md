# Specification readiness: native syscall extension

This branch adds the deliberate native Zeal syscall interface specified in
[zos-syscalls.md](zos-syscalls.md). It does not change or establish completeness of
the recovered CP/M TP3 specification. Original binaries remain unchanged.

The 27-call SDK/kernel ABI inventory is source-verified and hash-bound in
[zos-syscall-evidence.json](zos-syscall-evidence.json). Checked calls, output
conventions, staging, raw-register obligations and stock-unit delivery are defined
in the interface document. MAP is raw-only; generic IOCTL covers scalar arguments
and flat buffers up to 128 bytes, with nested-pointer requests left to raw calls.

## Rebase qualification

The feature is rebased onto main `66afe5f` and retains main's assembly SYSFILE,
Pascal CRT, and build layout. The syscall packager is `tools/package-zos.py`.
Post-rebase native acceptance passes all112 checks across22 stages, including
transfer boundaries, resource reuse, READDIR, descriptor duplication/release,
SWAP, and the pinned kernel status reproducer. Direct EXEC preserves status0;
injected EXIT cleanup failure preserves primary73 and records secondary13.

Production unit acceptance passes643 native assertions; assembly SYSFILE passes
1712, including relocation and serial-mock qualification. CRT passes512 native
assertions, exact output and standalone Chain; ReadKey passes ordinary and
pending-error input probes. Sanitized transport/type/reader/loader and bank-phase
checks pass, as do all60 runtime-profile checks. Native-authored CRT/ZVB prebuilts
and compiler bindings are refreshed. The clean shipping ZDE build with tests off
produces9 artifacts identical to the test-enabled build. The tightest production
compiler bank has134 bytes remaining.

All21 emulator CPU-cycle benchmark cases pass. Merely importing ZOS adds no code;
the minimal checked and register examples add4675 and4767 bytes respectively and
use the required three-page layout. Full window and baseline-subtracted timings,
actual compiler/artifact hashes, observed stack writes and preservation evidence
are recorded in [zos-syscall-validation.json](zos-syscall-validation.json).
Shared statement-level guard/probe overhead is subtracted with the empty window;
these are HostFS emulator CPU costs, not board or storage throughput. The required
read-only Opus initial review and follow-up are reconciled in the local milestone
record `reviews/native-syscalls-qualification-v1.md`.

The pinned kernel can report a false error after an overwrite. Checked transfers
stop on that error: only their first128-byte chunk may have changed even when
Count is0, while a raw single call may complete a larger transfer. The emulator
also reports stale file size after truncating an existing HostFS file. Tests
remove/recreate disposable transfer files; neither external component is patched.

The legacy monolithic `zunittest` cannot be packaged because bank3/11/15 exceed
bounds. The same failure is independently reproduced from untouched parent main
`66afe5f`; no passing claim is made for that suite. Hardware testing remains
pending at the user's direction because no board is connected. Source inspection
and emulator acceptance do not establish hardware behavior or completeness of
the recovered CP/M specification.
