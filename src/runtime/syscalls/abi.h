#ifndef ZP_SYSCALL_ABI_H
#define ZP_SYSCALL_ABI_H
#include <stdint.h>
#define ZS_JOB_BYTES 44u
/* All fields are little-endian words. Arguments retain their original widths
 * until validation; no byte argument is truncated by the compiler. */
typedef struct {
    uint16_t call,mode,args[5],sizes[5],result;
    uint16_t heap_floor,heap_frontier,static_end,stack_pointer,data_base;
    uint16_t scratch,reserved[3];
} ZOSJob;
typedef struct { uint8_t a,h; uint16_t bc,de; } ZOSRegisters;
#ifdef __SDCC
#define ZS_CALL __sdcccall(1)
#else
#define ZS_CALL
#endif
void zs_dispatch(ZOSJob *job) ZS_CALL;
void zs_invoke(ZOSRegisters *regs,uint16_t call) ZS_CALL;
#endif
