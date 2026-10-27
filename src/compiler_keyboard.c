#include "compiler.h"

/* Each activation owns two bytes in its generated static area: a decoded
 * pending press and its presence flag. The prefix still owns raw release pairs
 * and the one modifier decoder. Its private dispatch consumes this cache before
 * returning to the existing raw reader, so buffered input cannot lose a peek. */
static void engine(void) {
    uint16_t skip,state,empty;
    if(cc.keyboard_entry||cc.suppress||cc.error->code) return;
    skip=cc_forward(0xc3);
    state=cc.emitted; cc_emit(0); cc_emit(0);
    cc.keyboard_entry=cc.emitted;
    cc_address(0x3a,state); cc_emit(0xb7); empty=cc_forward(0xca);
    cc_emit(0xaf); cc_address(0x32,state);
    cc_address(0x3a,state+1); cc_emit(0xc9);
    cc_patch(empty); cc_op16(0xc3,cc_runtime_private(RP_INPUT_RAW_KEY));

    cc.keyboard_poll=cc.emitted;
    cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); cc_op16(0xc2,RT_IOFAIL);
    cc_address(0x3a,state); cc_emit(0xb7); cc_emit(0xc0);
    /* No Pascal callback runs while the buffered editor owns its echo handle.
     * Establish invalid ownership before any mode/read failure can clean up. */
    cc_emit(0x3e); cc_emit(255); cc_op16(0x32,cc_runtime_private(RP_ECHO_FD));
    cc_op16(0x11,4); cc_op16(0xcd,cc_runtime_private(RP_INPUT_MODE));
    cc_emit(0xb7); cc_op16(0xc2,cc_runtime_private(RP_INPUT_FAIL));
    cc_op16(0xcd,cc_runtime_private(RP_INPUT_RAW_KEY));
    cc_emit(0xb7); empty=cc_forward(0xca);
    cc_address(0x32,state+1); cc_emit(0x3e); cc_emit(1); cc_address(0x32,state);
    cc_patch(empty); cc_op16(0xcd,cc_runtime_private(RP_INPUT_CLEANUP));
    cc_emit(0xb7); cc_op16(0xc2,cc_runtime_private(RP_IO_ERROR)); cc_emit(0xc9);
    cc_patch(skip);
}
void cc_keypressed(void) FRONTEND {
    uint16_t done;
    engine();
    cc_reserve(512); cc_op16(0xcd,RT_IOCALL);
    cc_relocate(cc.emitted);
    cc_emit((uint8_t)cc.keyboard_poll); cc_emit((uint8_t)(cc.keyboard_poll>>8)); cc_emit(cc.io_check);
    /* IOCALL owns HL while restoring its landing. Read the owned result only
     * after that restoration; unchecked/pending failures return False. */
    cc_op16(0x21,0); cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); done=cc_forward(0xc2);
    cc_address(0x3a,cc.keyboard_entry-2); cc_emit(0x6f); cc_patch(done);
}
