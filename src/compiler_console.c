#include "compiler.h"

/* Console flow control owns no buffered input. It removes only interpreted
 * controls from physical typeahead; every other queued character keeps its
 * order. A is zero for ordinary output, one while output is paused. */
static uint16_t controls(void) {
    uint16_t entry=cc.emitted,loop,take,append[3],next;uint8_t i;
    cc_emit(0x57);cc_address(0x3a,cc.keyboard_state);cc_emit(0xb7);cc_emit(0xc8);
    cc_emit(0x47);cc_emit(0x0e);cc_emit(0);loop=cc.emitted;
    cc_emit(0xc5);cc_emit(0xd5);cc_address(0xcd,cc.keyboard_entry);cc_emit(0xd1);cc_emit(0xc1);
    cc_emit(0x5f);cc_emit(0x79);cc_emit(0xb7);append[0]=cc_forward(0xc2);
    cc_emit(0x7b);cc_emit(0xfe);cc_emit(19);take=cc_forward(0xca);
    cc_emit(0x7a);cc_emit(0xb7);append[1]=cc_forward(0xca);
    cc_emit(0x7b);cc_emit(0xfe);cc_emit(17);next=cc_forward(0xca);
    cc_emit(0xfe);cc_emit(3);append[2]=cc_forward(0xc2);
    cc_patch(take);cc_patch(next);cc_emit(0x4b);next=cc_forward(0xc3);
    for(i=0;i<3;++i)cc_patch(append[i]);
    cc_emit(0x7b);cc_emit(0xc5);cc_emit(0xd5);cc_address(0xcd,cc.keyboard_enqueue);
    cc_emit(0xd1);cc_emit(0xc1);cc_patch(next);cc_emit(0x05);cc_address(0xc2,loop);
    cc_emit(0x79);cc_emit(0xc9);return entry;
}
/* A console check must still accept resume/break at the typeahead limit.
 * Ordinary presses beyond that bounded queue are discarded, as with a full
 * physical keyboard FIFO, rather than preventing the user from resuming. */
static uint16_t append(void) {
    uint16_t entry=cc.emitted;
    cc_emit(0xb7);cc_emit(0xc8);cc_emit(0x4f);cc_address(0x3a,cc.keyboard_state);
    cc_emit(0xfe);cc_emit(255);cc_emit(0xc8);cc_emit(0x79);cc_address(0xc3,cc.keyboard_enqueue);
    return entry;
}
static uint16_t flow(uint16_t scan,uint16_t enqueue) {
    uint16_t entry=cc.emitted,pause[2],paused,got,stop;
    cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);cc_op16(0xc2,RT_IOFAIL);
    cc_emit(0xaf);cc_address(0xcd,scan);cc_emit(0xfe);cc_emit(19);pause[0]=cc_forward(0xca);
    cc_address(0xcd,cc.keyboard_console_probe);cc_emit(0xfe);cc_emit(19);pause[1]=cc_forward(0xca);
    cc_address(0xc3,enqueue);
    paused=cc.emitted;cc_patch(pause[0]);cc_patch(pause[1]);
    cc_emit(0x3e);cc_emit(1);cc_address(0xcd,scan);cc_emit(0xb7);got=cc_forward(0xc2);
    cc_address(0xcd,cc.keyboard_console_probe);cc_emit(0xb7);cc_address(0xca,paused);
    cc_patch(got);cc_emit(0xfe);cc_emit(3);stop=cc_forward(0xca);
    cc_emit(0xfe);cc_emit(19);cc_emit(0xc8);cc_emit(0xfe);cc_emit(17);cc_emit(0xc8);
    cc_address(0xcd,enqueue);cc_address(0xc3,paused);
    cc_patch(stop);cc_op16(0xcd,cc_runtime_private(RP_INPUT_CLEANUP));
    cc_op16(0xc3,cc_runtime_private(RP_USER_BREAK));return entry;
}
/* Only video stdout has console flow control. A regular redirected file does
 * not acquire a keyboard dependency. Classification uses the supported video
 * area query; the actual writer still owns write/descriptor errors. */
static uint16_t stdout_flow(uint16_t target) {
    uint16_t entry=cc.emitted;
    /* Native area_t has four bytes and must not cross an MMU page. Code/data
     * placement is variable; use the prefix's reserved syscall scratch. No
     * callback can run during this query and its contents need not survive. */
    cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);cc_op16(0xc2,RT_IOFAIL);
    cc_op16(0x11,cc_runtime_private(RP_INPUT_CONSOLE_AREA));cc_emit(0x0e);cc_emit(1); /* CMD_GET_AREA */
    cc_op16(0x2a,RT_PC);cc_emit(0xe5);cc_op16(0x21,7);cc_emit(0xcf);
    cc_emit(0xe1);cc_op16(0x22,RT_PC);cc_emit(0xb7);cc_emit(0xc0);
    cc_address(0xc3,target);return entry;
}
/* This is a transparent prefix around either a native entry or its existing
 * callback adapter. No push precedes the complete guard on the active path.
 * An echo failure retires the input owner's resources before its I- landing. */
static uint16_t wrap(uint16_t original,uint8_t staged,uint16_t target,uint8_t kind) {
    uint16_t fallback=cc_original_entry(original,staged),character=cc.emitted,entry,active,failed,body;
    cc_emit(0);entry=cc.emitted;
    cc_address(0x32,character);cc_op16(0x3a,RT_CBREAK);cc_emit(0xb7);active=cc_forward(0xc2);
    cc_address(0x3a,character);cc_address(0xc3,fallback);cc_patch(active);
    if(kind==2) {
        cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);cc_op16(0xc2,RT_IOFAIL);
    }
    cc_capture_guard(512,0);cc_address(0x3a,character);cc_preserve_registers(0);
    cc_op16(0xcd,RT_IOCALL);body=cc.emitted;cc_relocate(body);cc_emit(0);cc_emit(0);cc_emit(0);
    cc_op16(0x3a,RT_IOSTATUS);cc_emit(0xb7);failed=cc_forward(0xc2);
    cc_preserve_registers(1);cc_address(0xc3,fallback);
    cc_patch(failed);if(kind==1)cc_op16(0xcd,cc_runtime_private(RP_INPUT_CLEANUP));
    cc_preserve_registers(1);
    /* This check runs before the physical adapter can establish its own I-
     * landing. Its native polling failures belong to that transport's F0
     * category. Callback and ordinary stdout errors keep their original code. */
    if(kind==2) {cc_emit(0x3e);cc_emit(0xf0);}
    else cc_op16(0x3a,RT_IOSTATUS);
    cc_op16(0xc3,RT_IOFAIL);
    cc_word(body,cc.emitted);cc_address(0xc3,target);return entry;
}
void cc_console_finish(void) FRONTEND {
    uint16_t scan,enqueue,poll,stdout,entry,native,physical;
    if(cc.error->code||(!cc.console_used&&(!cc.file_entry||cc.text_type==255)&&!cc.hook_character&&!cc.hook_echo))return;
    /* With a fixed False CBreak these transparent wrappers always take their
     * fallback. Raw memory/code, aliases and user-visible hooks keep them live. */
    if(!cc.console_dynamic&&!far_get(cc.image->pages[0],RT_CBREAK-0x4000u))return;
    cc_keyboard_engine();scan=controls();enqueue=append();poll=flow(scan,enqueue);stdout=stdout_flow(poll);
    native=cc_runtime_get(RT_CHAR-0x4000u+1)|((uint16_t)cc_runtime_get(RT_CHAR-0x4000u+2)<<8);
    cc.hook_character=wrap(cc.hook_character?cc.hook_character:native,cc.hook_character!=0,stdout,0);
    native=cc_runtime_private(RP_INPUT_ECHO);
    cc.hook_echo=wrap(cc.hook_echo?cc.hook_echo:native,cc.hook_echo!=0,poll,1);
    physical=cc.hook_defaults[HOOK_CON_OUT];
    if(physical) {
        entry=wrap(physical,1,poll,2);cc_put(physical,0xc3);cc_word(physical+1,entry);cc_relocate(physical+1);
    }
}
