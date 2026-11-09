# Native ZOS syscalls

This is a deliberate Zeal extension, separate from recovered CP/M TP3 BDOS/BIOS
behavior. `ZOS`, `ZOSBuffer` and `ZOSRegisters` are built-ins. Place `ZOS.ZPU`
beside the compiler's ZPL, from the same build; `uses ZOS;` imports its constants and ABI records.
Numeric call constants work without a Uses clause. The compile-time bridge still
requires the matching ZOS.ZPU; stale bridge checksums are rejected. Programs contain their selected
native bridge and need no ZPU at execution time. `ZOS.PAS` is the public interface
reference; its native implementation is built from this repository's C/assembly.

```pascal
uses ZOS;
var Device, Status: Integer; N: Word; Buffer: array[0..255] of Byte;
begin
  Device := ZOS(ZOS_OPEN, 'H:/DATA.BIN', ZOS_O_RDONLY);
  if Device >= 0 then begin
    N := SizeOf(Buffer);
    Status := ZOS(ZOS_READ, Device, Buffer, N);
    Status := ZOS(ZOS_CLOSE, Device)
  end
end.
```

## Checked calls

Every call returns Integer. OPEN and OPENDIR return a descriptor or a negative
error; other calls return zero or a positive native error. Explicit errors do not
change IOResult or depend on `{$I}`. Byte arguments accept Integer/Word expressions
and reject values outside 0..255 rather than truncating them. Call numbers must be
constants; arbitrary variable numbers belong to the register interface.

| Number | Constant | Arguments after the call number |
| --- | --- | --- |
| 0, 1 | ZOS_READ, ZOS_WRITE | Device; var Buffer; var Count: Word |
| 2 | ZOS_OPEN | Path: String; Flags |
| 3 | ZOS_CLOSE | Device |
| 4 | ZOS_DSTAT | Device; var Stat (29 bytes) |
| 5 | ZOS_STAT | Path: String; var Stat (29 bytes) |
| 6 | ZOS_SEEK | Device; var Offset: LongWord; Whence |
| 7 | ZOS_IOCTL | Device; Command; Argument: Word; var Result: Word |
| 8, 9 | ZOS_MKDIR, ZOS_CHDIR | Path: String |
| 10 | ZOS_CURDIR | var Path: String |
| 11 | ZOS_OPENDIR | Path: String |
| 12 | ZOS_READDIR | Device; var Entry (17 bytes) |
| 13 | ZOS_RM | Path: String |
| 14 | ZOS_MOUNT | Device; Letter: Char; FileSystem |
| 15 | ZOS_EXIT | ExitCode (does not return on valid input) |
| 16 | ZOS_EXEC | Mode; Path, Arguments: String; var ExitCode: Byte |
| 17 | ZOS_DUP | Device; NewDevice |
| 18 | ZOS_MSLEEP | Milliseconds: Word |
| 19 | ZOS_SETTIME | Clock; Milliseconds: Word |
| 20 | ZOS_GETTIME | Clock; var Milliseconds: Word |
| 21, 22 | ZOS_SETDATE, ZOS_GETDATE | var Date (8 bytes) |
| 23 | ZOS_MAP | Register interface only |
| 24 | ZOS_SWAP | Device; NewDevice |
| 25 | ZOS_PALLOC | var Page: Byte |
| 26 | ZOS_PFREE | Page |

Unannotated numeric values use the kernel byte domain, except IOCTL Argument and
millisecond values, which use all 16 bits. Fixed-record and buffer arguments may
be variables, fields, indexed elements or dereferenced pointers with sufficient
storage. Use TZOSDate, TZOSStat and TZOSDirectoryEntry for their SDK layouts;
record names are fixed character arrays, not Pascal strings. Dates remain BCD.

SEEK uses unsigned offset bits with ZOS_SEEK_SET and signed two's complement bits
with ZOS_SEEK_CUR/ZOS_SEEK_END. On success Offset receives the resulting unsigned
position. Native seek clamps at EOF. EXEC takes one command-tail string of at most
127 bytes, and returns the child's byte exit code only when preservation succeeds.
The kernel process nesting limit can return CANNOT_REGISTER_MORE, including from
a program already running inside the IDE RAM worker. An override does not return
on success. Normal EXIT uses Pascal terminal cleanup;
inside a RAM run it follows the runtime's Halt return path.

Paths have at most 127 bytes plus NUL. Embedded NULs, oversized paths, invalid
buffer spans and insufficient output string capacity return invalid parameter
(negated for descriptor calls). Scalar outputs and fixed records remain unchanged
on failure. Reads/writes update Count with completed chunks, stop at short/zero
transfers, and preserve known progress if a later chunk fails. They make no kernel
call for a zero request. Calls bypass Pascal file buffering; avoid mixing native
access and buffered Pascal access to the same descriptor without coordination.

The bridge saves 256 bytes of existing private runtime data on its checked stack,
uses that page-1 storage as two 128-byte staging buffers, then restores it. Paths,
record buffers and page-3 locals therefore obey kernel page constraints without
allocating a new page. Every built-in opts into the full-memory calling layout.

The current kernel can return INVALID_FILESYSTEM for DSTAT on a writable handle;
STAT by path or DSTAT on a read-only handle avoids that kernel limitation.
The pinned kernel also has an overwrite-status defect in
`kernel/disks.asm`, `_zos_disk_write_epilogue`: an early `ret nz` can return a
file-size byte in A after the driver has already written the data. The native
`syscall-kernel.pas` fixture creates a 1024-byte file and overwrites its first byte;
both raw and checked calls return 4 (NO_SUCH_ENTRY), while readback proves the
write occurred. Checked Count is zero because no chunk reported success. Count
therefore describes confirmed successful chunks, not every possible device side
effect on error. The bridge preserves the kernel status; it does not hide this
platform defect.

The pinned emulator HostFS also reports the pre-truncation size when opening an
existing file with TRUNC (`hw/hostfs.c`, `fs_open`). Transfer fixtures and write
benchmarks remove and recreate their disposable files to start with a genuinely
zero-size kernel handle. Neither external component is modified by this feature.

Driver IOCTL support is command-specific; the video driver leaves GET_ATTR
unimplemented. The bridge returns these native errors unchanged.

## Flat-buffer IOCTL and raw registers

```pascal
Status := ZOSBuffer(ZOS_IOCTL, Device, Command, Buffer, Size, Result);
Status := ZOSRegisters(CallNumber, Registers);
```

ZOSBuffer copies at most ZOS_IOCTL_BUFFER_MAX (128) bytes into staging and copies
them back on success; Result captures DE. The size must fit the selected Pascal
variable. Pointer-containing records and pointer-valued results require driver-
specific interpretation; staging does not translate their internal addresses.

TZOSRegisters is exactly six bytes: A, H: Byte; BC, DE: Word. Initialize all fields.
The register built-in loads these inputs, places CallNumber in L, invokes RST $08,
and captures A, BC and DE. H retains its input because the OS preserves HL. The
function returns unsigned A, including raw OPEN/OPENDIR results; interpret their
high bit as the signed descriptor/error convention yourself.

The raw interface performs no pointer staging, terminal cleanup, buffer splitting,
error translation or ownership management. Kernel buffers must remain mapped and
within one 16-KiB page; nested pointers must independently satisfy driver rules.
MAP takes DE as virtual destination and HBC as the 24-bit physical byte address (H is bits 16..23, BC is
bits 0..15). For a 16-KiB page P, use H=P shr 2 and BC=(P and 3) shl 14.
The SDK prose calls this “upper 24 bits”; its PMAP implementation and the kernel
MMU macro establish the actual register encoding. It persists the requested mapping. Its caller must preserve a usable
code/stack path and restore mappings before resuming Pascal; replacing active
stack backing cannot be made safe by saving registers. Only free pages actually
returned by PALLOC and no longer in use. PMAP and kernel_config are SDK helpers,
not extra syscall numbers.

## Verification and evidence

The syscall matrix follows the assembly SDK and kernel, rather than copying the
older prose syscall summary. In particular PALLOC/PFREE are 25/26, time counters
are direct DE values, PALLOC returns B, and EXEC returns the child's status in D.
Source revisions and hashes are recorded in zos-syscall-evidence.json. Source
inspection is documented evidence; host mocks and native-emulator runs are
separate observed evidence. Hardware qualification remains separate.

Build with `zde cmake build --target zpascal_bin zos_zpu zsyscalltest_bin -j4` after
enabling ZP_BUILD_TESTS. Run `python3 tools/test-syscalls.py` for native acceptance.
The host transport fixture is tests/host/syscalls.c, compiled with ZS_HOST and the
SDK include directory. Runtime profile, production unit acceptance and sanitized loader regressions
must also pass because the private runtime table and ZPU export flags changed.


`tools/measure-syscalls.py PATH/TO/ZBENCH.COM` measures the saved benchmark
produced by native acceptance. It reports 21 CPU-cycle cases, minimal linked
program sizes/pages, stack writes observed with sentinels, scratch preservation,
MMU/SP preservation, and IFF2 at window boundaries. The empty window removes
shared probe overhead. Checked transfers use 128-byte chunks; zero-byte requests
make no kernel call. HostFS CPU cycles are not board or storage throughput, and
the observed stack watermark is not an upper bound on reserved stack. Exact
results and platform hashes belong in `zos-syscall-validation.json`.
