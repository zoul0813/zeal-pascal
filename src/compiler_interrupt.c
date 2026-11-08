#include "compiler_layout.h"
#include "compiler_late.h"

/* Execution polls borrow the same nondestructive physical lookahead as
 * KeyPressed. Only Ctrl-C is consumed here. The prefix owns mode restoration,
 * error classification and the nonlocal break/cleanup path. No Pascal input
 * or status callback is invoked by an asynchronous interruption check. */
static void engine(void) {
    uint16_t skip,probe,stop;
    if(cc.interrupt_entry||cc.suppress||cc.error->code)return;
    if(cc_root_member()) {cc_root_request(LATE_ROOT,LATE_INTERRUPT);return;}
    cc_keyboard_engine();
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
    cc_runtime_op(0xc3,cc_runtime_private(RP_USER_BREAK));
    cc_patch(skip);

    cc_root_bind(LATE_ROOT,LATE_INTERRUPT,cc.interrupt_entry);
}
void cc_interrupt_finish(void) FRONTEND {if(cc_root_pending(LATE_ROOT,LATE_INTERRUPT))engine();}
void cc_user_interrupt(void) FRONTEND {
    if(!cc.user_interrupt||cc.suppress||cc.error->code)return;
    engine();
    /* A poll is transparent to all live operands, Pascal frames, alternate
     * registers and flags. Reserve before pushing; I- failures unwind to this
     * invocation's own landing and retain the pending I/O result. */
    cc_reserve(512);
    cc_preserve_registers(0);
    cc_op16(0xcd,RT_IOCALL);cc_root_operand(LATE_ROOT,LATE_INTERRUPT,cc.interrupt_entry,0);cc_emit(cc.io_check);
    cc_preserve_registers(1);
}
