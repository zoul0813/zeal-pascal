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

The work was rebased onto main `66afe5f` after the qualification below. Conflicts
were resolved to retain main's assembly SYSFILE, Pascal CRT, and build layout.
The syscall packager is now `tools/package-zos.py`. Post-rebase CMake configuration,
host syscall transport, bank-phase and test-runner checks pass; the relocated
packager reproduces the prior artifact. Full native qualification and refreshed
CRT/ZVB compiler bindings must be rerun before treating the new base as qualified.
The results below describe the pre-rebase build.

The interface is implemented. Native emulator acceptance passes 75 syscall
assertions and a direct-shell successful preserve EXEC probe. Production unit
acceptance passes 643 native assertions; sanitized type/reader/loader and bank-phase
checks pass, as do all 60 runtime-profile checks. The shipping ZDE build and
refreshed stock-unit provenance validate. The required review is recorded in the
local milestone record `reviews/native-syscalls-v1.md`.

Qualification is limited to these checks. The legacy monolithic `zunittest`
harness exceeds resident and bank3/11/15 bounds and could not be packaged;
production unit initialization and sanitized loader/phase checks provide the unit
regression evidence for this increment. Hardware has not been run. Source
inspection alone is not acceptance evidence, and emulator acceptance is not
hardware proof.
