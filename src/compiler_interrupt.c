#include "compiler.h"

/* Execution polls borrow the same nondestructive physical lookahead as
 * KeyPressed. Only Ctrl-C is consumed here. The prefix owns mode restoration,
 * error classification and the nonlocal break/cleanup path. No Pascal input
 * or status callback is invoked by an asynchronous interruption check. */
void cc_user_interrupt(void) FRONTEND {
    uint16_t skip,probe,stop;
    if(!cc.user_interrupt||cc.suppress||cc.error->code)return;
    cc_keyboard_engine();
    if(!cc.interrupt_entry) {
        skip=cc_forward(0xc3);cc.interrupt_entry=cc.emitted;
        /* KeyPressed under U- may already own Ctrl-C as its current key. All
         * other queued keys came from these polls, which never queue Ctrl-C. */
        cc_address(0x3a,cc.keyboard_state);cc_emit(0xb7);probe=cc_forward(0xca);
        cc_address(0x3a,cc.keyboard_state+1);cc_emit(0x6f);cc_emit(0x26);cc_emit(0);
        cc_address(0x11,cc.keyboard_state+2);cc_emit(0x19);cc_emit(0x7e);
        cc_emit(0xfe);cc_emit(3);stop=cc_forward(0xca);
        cc_patch(probe);cc_address(0xcd,cc.keyboard_probe);
        cc_emit(0xfe);cc_emit(3);cc_patch(stop);
        probe=cc_forward(0xca);cc_address(0xc3,cc.keyboard_enqueue);cc_patch(probe);
        cc_op16(0xc3,cc_runtime_private(RP_USER_BREAK));
        cc_patch(skip);
    }
    /* A poll is transparent to all live operands, Pascal frames, alternate
     * registers and flags. Reserve before pushing; I- failures unwind to this
     * invocation's own landing and retain the pending I/O result. */
    cc_reserve(512);
    cc_emit(0xf5);cc_emit(0xc5);cc_emit(0xd5);cc_emit(0xe5);
    cc_emit(0xdd);cc_emit(0xe5);cc_emit(0xfd);cc_emit(0xe5);
    cc_emit(0x08);cc_emit(0xf5);cc_emit(0x08);
    cc_emit(0xd9);cc_emit(0xc5);cc_emit(0xd5);cc_emit(0xe5);cc_emit(0xd9);
    cc_op16(0xcd,RT_IOCALL);cc_relocate(cc.emitted);
    cc_emit((uint8_t)cc.interrupt_entry);cc_emit((uint8_t)(cc.interrupt_entry>>8));cc_emit(cc.io_check);
    cc_emit(0xd9);cc_emit(0xe1);cc_emit(0xd1);cc_emit(0xc1);cc_emit(0xd9);
    cc_emit(0x08);cc_emit(0xf1);cc_emit(0x08);
    cc_emit(0xfd);cc_emit(0xe1);cc_emit(0xdd);cc_emit(0xe1);
    cc_emit(0xe1);cc_emit(0xd1);cc_emit(0xc1);cc_emit(0xf1);
}
