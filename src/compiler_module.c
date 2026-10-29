#include "compiler_late.h"
extern const uint8_t unpack_start[],unpack_end[];

/* File and transfer acquisition share a single private decoder in each saved
 * or Memory image. Its input constants are authenticated compiler assets. */
void cc_unpack_reference(uint16_t operand) FRONTEND {
    if(cc.error->code||cc.suppress)return;
    cc_word(operand,cc.unpack_entry);cc_relocate(operand);cc.unpack_entry=operand;
}
void cc_unpack_finish(void) FRONTEND {
    uint16_t i,head=cc.unpack_entry,entry=cc.emitted;
    if(cc.error->code||(!head&&!cc_root_pending(LATE_ROOT,LATE_UNPACK)))return;
    /* This function and its immutable template share BANK11; only scalar
     * bytes cross cc_emit's bank boundary. No bank-local pointer escapes. */
    for(i=0;i<(uint16_t)(unpack_end-unpack_start);++i)cc_emit(unpack_start[i]);
    cc_patch_chain(head,entry);cc.unpack_entry=entry;cc_root_bind(LATE_ROOT,LATE_UNPACK,entry);
}
