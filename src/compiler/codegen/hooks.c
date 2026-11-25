#include "compiler/link/layout.h"
#include "compiler/groups/late.h"
#include "compiler/groups/native.h"
#include "runtime/files/operations.h"

/* Hooks belong to each generated program. Defaults are genuine Pascal ABI
 * entries; the fixed prefix is patched only in that program's owned image. */
void cc_hook_variable(uint16_t id) ROUTINES {
    if(CCS(id,address)||cc.suppress||cc.error->code) return;
    cc_require_real();
    if((uint32_t)GLOBAL_BASE+cc.image->globals_bytes+2>=CC_STATIC_LIMIT) { cc_fail(98); return; }
    CCS_SET(id,address,GLOBAL_BASE+cc.image->globals_bytes); cc.image->globals_bytes+=2;
    cc.hook_variables[CCS(id,initial_type)]=CCS(id,address);
}
void cc_hook_output(void) ROUTINES {
#ifdef ZP_OVERLAYS
    if(cc.sources->overlays&&cc.sources->bank) {
        cc_emit(0xcd);cc_emit(0);cc_emit(0);cc_overlay_chain(cc.emitted-2,LATE_OUTPUT);return;
    }
#endif
    cc.hook_output_calls=cc_link(0xcd,cc.hook_output_calls);
}
static uint16_t word(uint16_t at) {
    return cc_runtime_get(at-0x4000u)|((uint16_t)cc_runtime_get(at-0x4000u+1)<<8);
}
static uint16_t character(void) { return word(RT_CHAR+1); }
static void adapter_call(uint16_t entry) {
    if(entry<CC_CODE_BASE) cc_runtime_op(0xcd,entry);
    else cc_address(0xcd,entry);
}
static void job_index(uint8_t op,uint8_t offset) { cc_emit(0xfd); cc_emit(op); cc_emit(offset); }
static void pending(void) {
    cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); cc_emit(0xc8); cc_op16(0xc3,RT_IOFAIL);
}
/* HL target, A character. Catch unchecked callback failures before restoring
 * IX/IY, iteration registers and the caller PC. Fatal body errors keep body PC.
 * The callback may invoke this same helper recursively with independent state. */
static uint16_t invocation(uint8_t status) {
    uint16_t entry=cc.emitted,body,returned;
    cc_capture_guard(512,1);
    cc_emit(0xc5); cc_emit(0xd5); cc_emit(0xe5);
    cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xfd); cc_emit(0xe5);
    cc_emit(0xed); cc_emit(0x4b); cc_emit((uint8_t)RT_PC); cc_emit((uint8_t)(RT_PC>>8)); cc_emit(0xc5);
    cc_op16(0xcd,RT_IOCALL); body=cc.emitted; cc_relocate(body);
    cc_emit(0); cc_emit(0); cc_emit(0);
    cc_emit(0xe1); cc_op16(0x22,RT_PC);
    cc_emit(0xfd); cc_emit(0xe1); cc_emit(0xdd); cc_emit(0xe1);
    cc_emit(0xe1); cc_emit(0xd1); cc_emit(0xc1); pending();
    cc_word(body,cc.emitted);
    cc_emit(0x4f); cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); cc_op16(0xc2,RT_IOFAIL); cc_emit(0x79);
    if(!status) { cc_emit(0x4f); cc_emit(0x06); cc_emit(0); cc_emit(0xc5); }
    returned=cc.emitted+5; cc_address(0x11,returned); cc_emit(0xd5); cc_emit(0xe9);
    if(status) { cc_emit(0x7d); cc_address(0x32,cc.keyboard_status); }
    cc_emit(0xc9);
    return entry;
}
/* A separately callable physical console procedure(Char). It owns a temporary
 * VID0 descriptor, even inside another input/echo operation. Native errors are
 * classified here, never by rewriting an arbitrary Pascal callback's error. */
static uint16_t physical(uint16_t open,uint16_t echo) {
    uint16_t entry=cc.emitted,body;
    cc_capture_guard(512,0);
    cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); cc_op16(0xc2,RT_IOFAIL);
    cc_op16(0x21,2); cc_emit(0x39); cc_emit(0x7e); cc_emit(0xf5);
    cc_op16(0x2a,RT_PC); cc_emit(0xe5);
    cc_runtime_op(0x3a,cc_runtime_private(RP_ECHO_FD)); cc_emit(0xf5);
    cc_emit(0x3e); cc_emit(255); cc_runtime_op(0x32,cc_runtime_private(RP_ECHO_FD));
    cc_op16(0x21,4); cc_emit(0x39); cc_emit(0x23); cc_emit(0x7e);
    cc_op16(0xcd,RT_IOCALL); body=cc.emitted; cc_relocate(body);
    cc_emit(0); cc_emit(0); cc_emit(0);
    cc_emit(0xc1); cc_emit(0x78); cc_runtime_op(0x32,cc_runtime_private(RP_ECHO_FD));
    cc_emit(0xe1); cc_op16(0x22,RT_PC); cc_emit(0xf1);
    cc_emit(0xd1); cc_emit(0xc1); cc_emit(0xd5);
    cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); cc_emit(0xc8);
    cc_emit(0x3e); cc_emit(0xf0); cc_op16(0xc3,RT_IOFAIL);
    cc_word(body,cc.emitted);
    cc_emit(0x4f); cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); cc_op16(0xc2,RT_IOFAIL); cc_emit(0x79);
    cc_emit(0xf5); adapter_call(open); cc_emit(0xf1); adapter_call(echo);
    cc_runtime_op(0xcd,cc_runtime_private(RP_INPUT_CLOSE)); cc_emit(0xb7); cc_emit(0xc8);
    cc_runtime_op(0xc3,cc_runtime_private(RP_IO_ERROR));
    return entry;
}
static void restore(void) { cc_emit(0x79); cc_emit(0xe1); cc_emit(0xd1); cc_emit(0xc1); }
/* kind0 stdout, kind1 physical echo, kind2 echo-open. Default stdout keeps
 * inherited redirection; default physical output and saved adapters use VID0. */
static uint16_t console(uint8_t kind,uint16_t native,uint16_t invoke) {
    uint16_t entry=cc.emitted,default_path,owned=0;
    cc_capture_guard(512,1); cc_emit(0xc5); cc_emit(0xd5); cc_emit(0xe5); cc_emit(0x4f);
    cc_data_op(0x2a,cc.hook_variables[HOOK_CON_OUT]);
    cc_address(0x11,cc.hook_defaults[HOOK_CON_OUT]); cc_emit(0xb7); cc_emit(0xed); cc_emit(0x52);
    default_path=cc_forward(0xca);
    if(kind!=2) {
        cc_data_op(0x2a,cc.hook_variables[HOOK_CON_OUT]); cc_emit(0x79); cc_address(0xcd,invoke);
    }
    restore(); cc_emit(0xc9);
    cc_patch(default_path);
    if(kind==1) {
        cc_runtime_op(0x3a,cc_runtime_private(RP_ECHO_FD)); cc_emit(0xfe); cc_emit(255); owned=cc_forward(0xc2);
        cc_address(0x21,cc.hook_defaults[HOOK_CON_OUT]); cc_emit(0x79); cc_address(0xcd,invoke);
        restore(); cc_emit(0xc9); cc_patch(owned);
    }
    restore(); cc_address(0xc3,native);
    return entry;
}
static void target(uint8_t kind,uint16_t fallback) {
    if(cc.hook_variables[kind]) cc_data_op(0x2a,cc.hook_variables[kind]);
    else cc_address(0x21,fallback);
}
static void mask(void) {
    uint16_t entry=cc.emitted,write,skip;
    uint8_t k;
    if(!cc.hook_mask_call) return;
    if(!cc.hook_variables[HOOK_LST_OUT]&&!cc.hook_variables[HOOK_AUX_OUT]) {
        for(k=0;k<3;++k) cc_put(cc.hook_mask_call+k,0);
        return;
    }
    cc_word(cc.hook_mask_call+1,entry); cc_relocate(cc.hook_mask_call+1);
    job_index(0x7e,0); cc_emit(0xfe); cc_emit(ZF_TEXT_WRITE); write=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(ZF_TEXT_WRITELN); cc_emit(0xc0); cc_patch(write);
    job_index(0x7e,9); cc_emit(0xcb); cc_emit(0x7f); cc_emit(0xc0);
    cc_emit(0xc5); cc_emit(0xd5); cc_emit(0xe5); cc_emit(0x0e); cc_emit(0);
    for(k=HOOK_LST_OUT;k<=HOOK_AUX_OUT;++k) if(cc.hook_variables[k]) {
        cc_data_op(0x2a,cc.hook_variables[k]); cc_address(0x11,cc.hook_defaults[k]);
        cc_emit(0xb7); cc_emit(0xed); cc_emit(0x52); skip=cc_forward(0xca);
        cc_emit(0xcb); cc_emit(k==HOOK_LST_OUT?0xc1:0xc9); cc_patch(skip);
    }
    job_index(0x71,8); cc_emit(0x3e); cc_emit(0x40); job_index(0x77,9);
    cc_emit(0xe1); cc_emit(0xd1); cc_emit(0xc1); cc_emit(0xc9);
}
void cc_hooks_finish(void) ROUTINES {
    uint16_t open=0,echo=0,output=0,invoke=0,poll,entry,skip,deferred;
    uint8_t k;uint8_t needed=0;uint8_t pending=cc_root_pending(LATE_ROOT,LATE_OUTPUT);
    if(cc.error->code) return;
    for(k=HOOK_CON_OUT;k<HOOK_COUNT;++k) if(cc.hook_variables[k]) needed=1;
    if(needed||cc.hook_output_calls||pending) {
        open=cc_runtime_private(RP_INPUT_OPEN); echo=cc_runtime_private(RP_INPUT_ECHO);
        if(cc.hook_variables[HOOK_CON_OUT]) { open=cc_original_entry(open,0); echo=cc_original_entry(echo,0); }
        output=physical(open,echo); invoke=invocation(0);
        cc.hook_defaults[HOOK_CON_OUT]=cc.hook_defaults[HOOK_USR_OUT]=output;
        if(cc.hook_variables[HOOK_LST_OUT]) {
            cc.hook_defaults[HOOK_LST_OUT]=cc.emitted;
            cc_emit(0xd1); cc_emit(0xc1); cc_emit(0xd5);
            cc_emit(0x3e); cc_emit(0x20); cc_op16(0xc3,RT_IOFAIL);
        }
        if(cc.hook_variables[HOOK_AUX_OUT]) cc.hook_defaults[HOOK_AUX_OUT]=cc_file_default_output();
        if(cc.hook_variables[HOOK_CON_OUT]) {
            entry=cc_original_entry(character(),0); cc.hook_character=console(0,entry,invoke);
            cc.hook_echo=console(1,echo,invoke); cc.hook_open=console(2,open,invoke);
        }
        if(cc.hook_output_calls||pending) {
            deferred=cc.emitted;
            cc_patch_chain(cc.hook_output_calls,cc.emitted);
            cc_capture_guard(512,1); cc_emit(0xe5); cc_emit(0xf5);
            job_index(0x7e,34); cc_emit(0xfe); cc_emit(ZF_STANDARD_USR); skip=cc_forward(0xc2);
            target(HOOK_USR_OUT,output); entry=cc_forward(0xc3); cc_patch(skip);
            if(cc.hook_variables[HOOK_LST_OUT]) {
                cc_emit(0xfe); cc_emit(ZF_STANDARD_LST); skip=cc_forward(0xc2);
                target(HOOK_LST_OUT,0); poll=cc_forward(0xc3); cc_patch(skip);
            } else poll=0;
            if(cc.hook_variables[HOOK_AUX_OUT]) {
                cc_emit(0xfe); cc_emit(ZF_STANDARD_AUX); skip=cc_forward(0xc2);
                target(HOOK_AUX_OUT,0); open=cc_forward(0xc3); cc_patch(skip);
            } else open=0;
            target(HOOK_CON_OUT,output);
            cc_patch(entry); if(poll) cc_patch(poll); if(open) cc_patch(open);
            cc_emit(0xf1); cc_address(0xcd,invoke); cc_emit(0xe1); cc_emit(0xc9);
            cc_root_bind(LATE_ROOT,LATE_OUTPUT,deferred);
        }
    }
    if(cc.hook_variables[HOOK_STATUS]) {
        cc_keyboard_engine(); poll=cc_original_entry(cc.keyboard_poll,1);
        cc.hook_defaults[HOOK_STATUS]=cc.emitted;
        cc_capture_guard(512,0); cc_address(0xcd,poll); cc_op16(0x21,0); cc_address(0x3a,cc.keyboard_status); cc_emit(0x6f); cc_emit(0xc9);
        invoke=invocation(1); entry=cc.emitted;
        cc_capture_guard(512,0); cc_emit(0xe5); cc_emit(0xd5);
        cc_data_op(0x2a,cc.hook_variables[HOOK_STATUS]); cc_address(0x11,cc.hook_defaults[HOOK_STATUS]);
        cc_emit(0xb7); cc_emit(0xed); cc_emit(0x52); skip=cc_forward(0xc2);
        cc_emit(0xd1); cc_emit(0xe1); cc_address(0xc3,poll); cc_patch(skip);
        cc_emit(0xaf); cc_address(0x32,cc.keyboard_status);
        cc_data_op(0x2a,cc.hook_variables[HOOK_STATUS]); cc_address(0xcd,invoke);
        cc_emit(0xd1); cc_emit(0xe1); cc_emit(0xc9);
        cc_put(cc.keyboard_poll,0xc3); cc_word(cc.keyboard_poll+1,entry); cc_relocate(cc.keyboard_poll+1);
    }
    mask();
}
void cc_hooks_place(uint16_t delta) ROUTINES {
    uint16_t at,value;
    uint8_t k;
    for(k=0;k<3;++k) {
        at=k==0?character():cc_runtime_private(k==1?RP_INPUT_ECHO:RP_INPUT_OPEN);
        value=k==0?cc.hook_character:k==1?cc.hook_echo:cc.hook_open;
        if(value) { cc_put(cc_runtime_address(at)-0x4000u,0xc3); cc_word(cc_runtime_address(at)-0x4000u+1,value+delta); }
    }
    if(cc.error_entry) {
        at=cc_runtime_address(cc_runtime_private(RP_NORMAL_ERROR))-0x4000u;
        cc_put(at,0xc3);cc_word(at+1,cc.error_entry+delta);
    }
}
