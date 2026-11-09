# SYSFILE assembly implementation and measurements

Production SYSFILE is maintained Z80 assembly in `units/sysfile/`. The standard
assembler and linker emit its code, exports, fragment records and relocation
records as a complete ZPU byte stream. `tools/sysfile/seal.c` validates that
container and its assembled address operands, then adds the checksum and runtime
fingerprint. It performs no object reading, library extraction, address placement
or relocation. No SDCC C compilation or SDK archive linkage generates SYSFILE.

The original C implementation remains the legacy-test and host behavior oracle.
The ASM port initially preserved its verified instruction stream and private
calling conventions. Bounds validation, descriptor checks, flags and memory
helpers have been hand-tuned; complex routines still retain stack frames. This
is not a claim that every routine is fully optimized. Selective linking and all
Pascal file behavior remain unchanged.

Compared with commit `ac16467`, on the unmodified Zeal emulator:

| Measure | C backend | ASM backend | Saved |
|---|---:|---:|---:|
| SYSFILE.ZPU disk bytes | 21,960 | 20,546 | 1,414 (6.44%) |
| All Unit code bytes | 13,655 | 12,787 | 868 (6.36%) |
| Linked workspace bytes | 657 | 492 | 165 (25.11%) |
| Minimal program COM | 6,655 | 6,655 | 0 |
| Assign-only COM | 17,036 | 16,367 | 669 |
| Typed-file I/O COM | 19,505 | 18,790 | 715 |
| Block-file I/O COM | 19,991 | 19,322 | 669 |
| Text-file I/O COM | 21,779 | 21,110 | 669 |

Workspace is part of the linked COM: do not add its savings again to COM savings.
All five probes compiled and ran in Memory mode and as cold COM files without
compiler or Units on disk. Minimal uses one execution page; the four file probes
still use two. The stack requirement remains 2,048 bytes for minimal and 2,342
bytes for these file probes. These samples gain free RAM within their existing
pages; they do not release a complete 16 KiB page.

The direct ABI benchmark measures CPU T-states, including CALL and one NOP of
padding. Interrupts remain enabled and disk cases include kernel work. Two runs
of each version returned identical cycle counts. Every measured disk operation
used fewer cycles, by 7.06–41.55%; no measured regression occurred. These are
emulator CPU measurements, not physical disk wall times. Detailed per-operation
counts and hashes are in [the evidence](sysfile-asm-evidence.json).

Verification includes 1,712 native ASM file assertions, the 1,538-assertion direct
C host oracle, ten sanitized sealer tests, and the runner's 27 tests. A second
standard link moves code to $5000 and workspace to $A000: all 12,787 code bytes
match the authored relocation table, covering 742 address fields including
low/high-byte fields. The assembler also rejects out-of-order fragment IDs.
The sealer rejects invalid partitions, patches, targets, exports, initialized
workspace and operands inconsistent with declared relocations before publishing.

Native file tests cover standard streams, KBD/CON/TRM/LST/USR, AUX output overrides
and authenticated lookahead return. A test-only SER0 transport additionally runs
the production ASM AUX input/output paths, CRLF, peek consumption, unavailable
device, read/write errors and failed mode-setting restoration. This transport
lives in the benchmark's own COM; no emulator or kernel code is modified. It does
not qualify physical serial hardware. The real `_ioctl` adapter has been reviewed
and relocation-checked; the AUX mock replaces it, so its kernel execution is not
claimed as covered. Direct ABI queries explicitly test accepted and rejected
spans for both `$4012 = 2` and `$4012 = 4` alias branches. The unrelated full project acceptance
suite is not claimed as rerun. The sanitized host regression suite also passes
180,362 front-end/identity/allocation checks, 4,010 editor checks and 45,809 source
checks; the 60 private-runtime packaging checks pass. A fresh tests-OFF SYSFILE
build matches the main payload, and the final default release build passes.
The compiler BIN and ZPL sizes are unchanged at 16,115 and 279,060 bytes.
The build-layout fixture also passes after the migration.

Build the test prerequisites with tests enabled:

```sh
zde --no-tty exec cmake -S . -B build -DZP_BUILD_TESTS=ON
zde --no-tty cmake build --target zsysfiletest_bin sysfile_zpu sysfile_relocations sysfile_mock
python3 tools/test-file-runtime.py --direct-host --asm-native
python3 tools/measure-sysfile.py --output analysis/sysfile-measurements.json
python3 tools/measure-sysfile.py --clients --output analysis/sysfile-programs.json
```

Routine test reports go under unique `analysis/native-slice/` run directories.
Raw captures remain local and ignored by Git; the tracked evidence summary binds
intentional snapshots by hash. Rerunning tests does not overwrite those snapshots.

The old build-layout and SYSFILE-packager evidence files describe their respective
historical milestones; the new report above records this migration's products.
CRT and ZVB were re-authored against the changed private runtime fingerprint;
their bytes are unchanged outside the profile and checksum fields.

Opus reviewed the implementation in two rounds and found no shipped-code defects.
Final small harness corrections (stale-Unit prerequisites/checking, unique report
paths and observing buffers before RET) were tested afterward, including a host
regression that rejects the stale default Unit and explicit alias queries. They
were not submitted for a third review; no unchanged-input review finalization is
claimed for those corrections. The shipped bytes and original disk cycle counts
remain unchanged.
