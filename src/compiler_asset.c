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
