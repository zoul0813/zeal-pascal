#include "compiler.h"
#ifdef __SDCC
#include "library.h"
#endif

/* The immutable template lives at offset zero in verified library bank 8.
 * Access through the MMU helper; no bank-local pointer escapes to a caller. */
uint16_t cc_runtime_size(void) { return (uint16_t)(runtime_end-runtime_start); }
uint8_t cc_runtime_get(uint16_t offset) {
#ifdef __SDCC
    return far_get(code_pages[7],offset);
#else
    return runtime_start[offset];
#endif
}
uint16_t cc_runtime_private(uint8_t entry) {
#ifdef __SDCC
    uint16_t offset=cc_runtime_size()+2*entry;
    return far_get(code_pages[7],offset)|((uint16_t)far_get(code_pages[7],offset+1)<<8);
#else
    extern const uint8_t runtime_private[];
    return runtime_private[2*entry]|((uint16_t)runtime_private[2*entry+1]<<8);
#endif
}
uint16_t cc_file_asset_size(uint8_t gate) {
    return gate?(uint16_t)(file_gate_end-file_gate_start):(uint16_t)(file_end-file_start);
}
uint8_t cc_file_asset_get(uint8_t gate,uint16_t offset) {
#ifdef __SDCC
    uint16_t base=(uint16_t)(gate?file_gate_start:file_start)&0x3fffu;
    return far_get(code_pages[10],base+offset);
#else
    return (gate?file_gate_start:file_start)[offset];
#endif
}
