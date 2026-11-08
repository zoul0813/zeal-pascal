#include "compiler.h"
#ifdef ZP_OPTIONAL_REAL
#include "optional_runtime.h"
#endif
#ifdef __SDCC
#include "library.h"
#endif

/* The immutable template lives at offset zero in verified library bank 8.
 * Access through the MMU helper; no bank-local pointer escapes to a caller. */
uint16_t cc_runtime_size(void) {
#ifdef ZP_OPTIONAL_REAL
    return OPTIONAL_RUNTIME_BYTES;
#else
    return (uint16_t)(runtime_end-runtime_start);
#endif
}
uint8_t cc_runtime_get(uint16_t offset) {
#ifdef ZP_OPTIONAL_REAL
    return cc_integer_get(offset);
#elif defined(__SDCC)
    return far_get(code_pages[CODE_RUNTIME],offset);
#else
    return runtime_start[offset];
#endif
}
/* Callers use retained scratch and spans bounded to256 bytes. */
uint8_t cc_runtime_read(uint16_t offset,uint8_t *out,uint16_t count) {
    if(count>256u||(uint32_t)offset+count>cc_runtime_size())return 0;
#ifdef ZP_OPTIONAL_REAL
    return cc_integer_read(offset,out,count);
#elif defined(__SDCC)
    return far_read(code_pages[CODE_RUNTIME],offset,out,count);
#else
    memcpy(out,runtime_start+offset,count);return 1;
#endif
}
uint8_t cc_runtime_copy(uint8_t page) {
    extern uint8_t console_scratch[256];
    uint16_t at,n;
    for(at=0;at<cc_runtime_size();at+=n) {
        if(!cc_poll())return 0;
        n=cc_runtime_size()-at;if(n>256)n=256;
        if(!cc_runtime_read(at,console_scratch,n)){cc_fail(99);return 0;}
#ifdef __SDCC
        {uint8_t old=get_bank();code_restore(page);
         memcpy((void*)(0x8000u+at),console_scratch,n);code_restore(old);
         page_versions[page]+=n;}
#else
        {uint16_t i;for(i=0;i<n;++i)far_set(page,at+i,console_scratch[i]);}
#endif
    }
    return 1;
}
uint16_t cc_runtime_private(uint8_t entry) {
#ifdef ZP_OPTIONAL_REAL
    uint16_t offset=cc_runtime_size()+2u*entry;
    return cc_integer_get(offset)|((uint16_t)cc_integer_get(offset+1)<<8);
#elif defined(__SDCC)
    uint16_t offset=cc_runtime_size()+2*entry;
    return far_get(code_pages[CODE_RUNTIME],offset)|((uint16_t)far_get(code_pages[CODE_RUNTIME],offset+1)<<8);
#else
    extern const uint8_t runtime_private[];
    return runtime_private[2*entry]|((uint16_t)runtime_private[2*entry+1]<<8);
#endif
}
#ifndef ZP_STATIC_LAYOUT
uint16_t cc_file_asset_size(uint8_t gate) {
    return gate?(uint16_t)(file_gate_end-file_gate_start):(uint16_t)(file_end-file_start);
}
uint8_t cc_file_asset_get(uint8_t gate,uint16_t offset) {
#ifdef __SDCC
    uint16_t base=(uint16_t)(gate?file_gate_start:file_start)&0x3fffu;
    return far_get(code_pages[CODE_FILE_RUNTIME],base+offset);
#else
    return (gate?file_gate_start:file_start)[offset];
#endif
}

#endif
