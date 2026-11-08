# Temporary code capacity and final placement

The compiler stages code at a temporary $8000 origin, then assigns its final
address after the selected runtime and globals. The old $F400 staging ceiling
reserved 2 KiB for a stack in that temporary address space, even when compact
linking moved all code down by more than 10 KiB. Snake exposed this limit while
appending startup code after Unit linking.

Temporary code may now end at $FFFF exclusive: at most 32,767 bytes. No address
wraps through zero, and relocation offsets still fit their existing low 15 bits.
Unit authoring retains its separate SOURCE_LIMIT. Fragment placement, code
fixup sites and code-range checks use the temporary code limit consistently.

Final image size, actual runtime/global placement, option bounds and required
physical stack checks are unchanged. Raising a temporary ceiling allocates no
extra RAM and adds no padding to saved programs. A full-prefix program that
cannot physically fit still fails final placement.

Verification uses the actual physical-limit implementation, sanitized fragment
linking at $FFF4..$FFFE with both word fixups and an overflowing case, native
fragment acceptance, compiler-bank residency checks and the saved Snake demo.

The group closer checks its widened size sum before narrowing to16 bits or
zero-filling. Four real-function boundary cases cover a valid final byte, an
one-byte overflow, a sum that wraps16 bits and a cross-page reservation, with
canaries unchanged on failure. A full temporary stream with live Unit roots
reports capacity error99 before fragment planning.
