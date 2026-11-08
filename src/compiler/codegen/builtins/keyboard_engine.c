#include "compiler/link/layout.h"
#include "compiler/groups/late.h"

/* Each activation owns a 255-character circular typeahead queue (count, head,
 * 256 byte slots). Interruption polling must continue beyond ordinary keys,
 * retaining them for later reads. At capacity leave further events with the
 * physical driver. The prefix owns raw release pairs and modifier decoding. */
void cc_keyboard_engine(void) FRONTEND {
    uint16_t skip,state,empty,i,body;
    if(cc.keyboard_entry||cc.suppress||cc.error->code) return;
    if(cc_root_member()) {cc_root_request(LATE_ROOT,LATE_KEYBOARD);return;}
    skip=cc_forward(0xc3);
    state=cc.keyboard_state=cc.emitted;
    for(i=0;i<258;++i)cc_emit(0);
    cc.keyboard_status=cc.emitted; cc_emit(0);
    cc.keyboard_entry=cc.emitted;
    cc_address(0x3a,state); cc_emit(0xb7); empty=cc_forward(0xca);
    cc_emit(0x3d);cc_address(0x32,state);
    cc_address(0x3a,state+1);cc_emit(0x6f);cc_emit(0x26);cc_emit(0);
    cc_emit(0x3c);cc_address(0x32,state+1);
    cc_address(0x11,state+2);cc_emit(0x19);cc_emit(0x7e);cc_emit(0xc9);
    cc_patch(empty); cc_runtime_op(0xc3,cc_runtime_private(RP_INPUT_RAW_KEY));

    cc.keyboard_probe=cc.emitted;
    cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); cc_op16(0xc2,RT_IOFAIL);
    cc_address(0x3a,state);cc_emit(0xfe);cc_emit(255);empty=cc_forward(0xc2);
    cc_emit(0xaf);cc_emit(0xc9);cc_patch(empty);
    cc.keyboard_console_probe=cc.emitted;
    cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);cc_op16(0xc2,RT_IOFAIL);
    /* A nested console check borrows input without taking its caller's echo
     * handle or raw mode. Catch physical failures locally before restoring
     * those owners and propagating the primary I/O result. */
    cc_runtime_op(0x3a,cc_runtime_private(RP_ECHO_FD));cc_emit(0xf5);
    cc_runtime_op(0x3a,cc_runtime_private(RP_INPUT_MODE_VALUE));cc_emit(0xf5);
    cc_emit(0x3e); cc_emit(255); cc_runtime_op(0x32,cc_runtime_private(RP_ECHO_FD));
    cc_op16(0xcd,RT_IOCALL);body=cc.emitted;cc_relocate(body);
    cc_emit(0);cc_emit(0);cc_emit(0);
    cc_emit(0xf5);cc_op16(0x21,3);cc_emit(0x39);cc_emit(0x5e);cc_emit(0x16);cc_emit(0);
    cc_runtime_op(0xcd,cc_runtime_private(RP_INPUT_MODE));cc_emit(0x47);
    cc_emit(0xf1);cc_emit(0xd1);cc_emit(0xd1);cc_emit(0x4f);
    cc_emit(0x7a);cc_runtime_op(0x32,cc_runtime_private(RP_ECHO_FD));
    cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);cc_op16(0xc2,RT_IOFAIL);
    cc_emit(0x78);cc_emit(0xb7);cc_runtime_op(0xc2,cc_runtime_private(RP_IO_ERROR));
    cc_emit(0x79);cc_emit(0xc9);
    cc_word(body,cc.emitted);
    cc_op16(0x11,4); cc_runtime_op(0xcd,cc_runtime_private(RP_INPUT_MODE));
    cc_emit(0xb7); cc_runtime_op(0xc2,cc_runtime_private(RP_INPUT_FAIL));
    cc_runtime_op(0xc3,cc_runtime_private(RP_INPUT_RAW_KEY));

    /* Probe guarantees space. Zero means no decoded key, never queue data. */
    cc.keyboard_enqueue=cc.emitted;
    cc_emit(0xb7);cc_emit(0xc8);cc_emit(0x47);
    cc_address(0x3a,state);cc_emit(0x3c);cc_address(0x32,state);cc_emit(0x3d);
    cc_address(0x21,state+1);cc_emit(0x86);cc_emit(0x6f);cc_emit(0x26);cc_emit(0);
    cc_address(0x11,state+2);cc_emit(0x19);cc_emit(0x70);cc_emit(0xc9);

    cc.keyboard_poll=cc.emitted;
    cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);cc_op16(0xc2,RT_IOFAIL);
    cc_address(0x3a,state);cc_emit(0xb7);empty=cc_forward(0xc2);
    cc_address(0xcd,cc.keyboard_probe);cc_address(0xcd,cc.keyboard_enqueue);
    cc_patch(empty);cc_address(0x3a,state);cc_emit(0xb7);empty=cc_forward(0xca);
    cc_emit(0x3e);cc_emit(1);cc_patch(empty);
    cc_address(0x32,cc.keyboard_status);cc_emit(0xc9);
    cc_patch(skip);
    cc_root_bind(LATE_ROOT,LATE_KEYBOARD,cc.keyboard_entry);
    cc_root_bind(LATE_ROOT,LATE_KEY_POLL,cc.keyboard_poll);
    cc_root_bind(LATE_ROOT,LATE_KEY_STATUS,cc.keyboard_status);
    cc_root_bind(LATE_ROOT,LATE_KEY_STATE,cc.keyboard_state);
    cc_root_bind(LATE_ROOT,LATE_KEY_PROBE,cc.keyboard_probe);
    cc_root_bind(LATE_ROOT,LATE_KEY_ENQUEUE,cc.keyboard_enqueue);
    cc_root_bind(LATE_ROOT,LATE_CONSOLE_PROBE,cc.keyboard_console_probe);
}
void cc_keyboard_finish(void) FRONTEND {if(cc_root_pending(LATE_ROOT,LATE_KEYBOARD))cc_keyboard_engine();}
