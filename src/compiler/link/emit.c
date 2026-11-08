#include "compiler/link/layout.h"
#include "compiler/link/link.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "units/internal.h"
#endif
#ifdef ZP_STATIC_LAYOUT
#define ZP_RUNTIME_COPY_OPERANDS
#include "runtime-layout.h"
#endif
#ifdef __SDCC
#pragma codeseg BANK9
#pragma constseg BANK9
#endif
uint8_t cc_layout_active(void) FRONTEND {
#ifdef ZP_STATIC_LAYOUT
    return !cc.sources->unit_mode&&!cc.transfer_entry&&
        !(cc.sources->options&&cc.sources->options->kind==COMPILE_CHAIN);
#else
    return 0;
#endif
}
uint16_t cc_data_base(void) FRONTEND {
#ifdef ZP_STATIC_LAYOUT
    if(cc_layout_active())return cc.real_needed?RUNTIME_FULL_END:RUNTIME_COMPACT_END;
#endif
    return GLOBAL_BASE;
}
uint16_t cc_runtime_address(uint16_t address) FRONTEND {
#ifdef ZP_STATIC_LAYOUT
    if(cc_layout_active()&&!cc.real_needed&&address>=RUNTIME_GAP_LAST&&address<RUNTIME_OWNED_END)
        return address-RUNTIME_GAP_BYTES;
#endif
    return address;
}
void cc_data_op(uint8_t opcode,uint16_t address) FRONTEND {
#ifdef ZP_STATIC_LAYOUT
    if(!cc.sources->unit_mode)cc_link_reference(cc.emitted+1,CL_DATA,0,address-GLOBAL_BASE,CF_WORD16);
#endif
    cc_op16(opcode,address);
}
void cc_runtime_copy_reference(uint16_t site,uint16_t address) FRONTEND {
#ifdef ZP_STATIC_LAYOUT
    if(address>=RUNTIME_GAP_LAST&&address<RUNTIME_OWNED_END) {
        if(cc.sources->unit_mode)cc_units_note(UR_RUNTIME,site,address);
        else cc_link_reference(site,CL_RUNTIME,0,address,CF_WORD16);
    }
#else
    (void)site;(void)address;
#endif
}
void cc_runtime_op(uint8_t opcode,uint16_t address) FRONTEND {
    cc_runtime_copy_reference(cc.emitted+1,address);cc_op16(opcode,address);
}

uint16_t cc_required_stack(void) FRONTEND {
#ifdef ZP_STATIC_LAYOUT
    if(cc.stack_bytes>2048u)return cc.stack_bytes;
#endif
    return 2048u;
}


void cc_runtime_copy_operand(uint16_t destination,uint16_t original) FRONTEND {
#ifdef ZP_STATIC_LAYOUT
    uint8_t i;
    for(i=0;i<sizeof(runtime_copy_operands)/sizeof(runtime_copy_operands[0]);++i)
        if(runtime_copy_operands[i].site==original) {
            if(runtime_copy_operands[i].target)cc_runtime_copy_reference(destination,runtime_copy_operands[i].target);
            return;
        }
    cc_fail(99);
#else
    (void)destination;(void)original;
#endif
}
