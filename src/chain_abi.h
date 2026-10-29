#ifndef ZP_CHAIN_ABI_H
#define ZP_CHAIN_ABI_H
/* Executable entry stays separate from build-bound private-runtime identity.
 * The public ZCHN header and jump ABI do not describe private helper offsets. */
#define CHAIN_ENTRY_BYTES 96u
#define CHAIN_PROFILE_BYTES 32u
#define CHAIN_ACTIVATION_BYTES (CHAIN_ENTRY_BYTES+CHAIN_PROFILE_BYTES)
#endif
