#include "compiler/groups/late.h"
#include "compiler/groups/native.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "units/internal.h"
#endif

/* Captured callback descriptors and their emitted native frames share one
 * owner. Routine declarations/calls use the banked entry reference below. */
static void indexed(uint8_t opcode,uint8_t offset) { cc_emit(0xdd); cc_emit(opcode); cc_emit(offset); }
static void cursor_store(void) { indexed(0x75,250); indexed(0x74,251); }
static void cursor_load(void) { indexed(0x6e,250); indexed(0x66,251); }

void cc_entry_reference(uint8_t opcode,uint16_t scope) ROUTINES {
    uint16_t entry=cc_rget(scope,R_ENTRY);
#ifdef ZP_UNITS_EXPERIMENT
    if(cc_rget(scope,R_FLAGS)&RF_UNIT_IMPORT){
        cc_units_call(opcode,scope);return;
    }
#endif
#ifdef ZP_UNITS_EXPERIMENT
    if(!(cc_rget(scope,R_FLAGS)&RF_DEFINED))
        cc_rput(scope,R_FLAGS,cc_rget(scope,R_FLAGS)|RF_REFERENCED);
    if(cc_rget(scope,R_FLAGS)&RF_CALLER_IO) {
        if(opcode!=0xcd){cc_fail(UNIT_UNSUPPORTED);return;}
        cc_emit(0x3e);cc_emit(cc.io_check);
    }
#endif
    /* A call made from another Pascal routine can have unbounded aggregate
     * depth. Keep independent windows for nested or unresolved calls; a
     * direct root-to-leaf call uses its checked, recorded stack requirement. */
    if((cc.scope&&!(cc_rget(scope,R_FLAGS)&RF_NATIVE))||!entry)cc_require_full_memory();
    /* Pascal frames use checked stack guards and record their own memory
     * requirements. Opaque external code retains independent windows. */
    if(cc_rget(scope,R_FLAGS)&RF_EXTERNAL) {cc_require_full_memory();cc_op16(opcode,entry);}
    else if(entry) {
#ifdef ZP_OVERLAYS
        if(cc.sources->overlays){cc_overlay_address(opcode,entry);return;}
#endif
        cc_address(opcode,entry);
    }
    else cc_rput(scope,R_CALLS,cc_link(opcode,cc_rget(scope,R_CALLS)));
}
/* A bridge is a real dynamic frame for file-owner traversal. The published
 * first eight bytes keep their ordinary meaning; the remaining private words
 * retain IY, BCDEHL results and the original callback caller's source PC. */
#define BRIDGE_PRIVATE_BYTES 18u
static void bridge_guard(uint16_t bytes) {
    uint16_t at=cc.emitted;
    /* Only A/flags are scratch here. Preserve operand registers and the actual
     * caller PC instead of attributing a bridge overflow to an earlier Addr. */
    cc_op16(0x3a,RT_PC); cc_address(0x32,at+17);
    cc_op16(0x3a,RT_PC+1); cc_address(0x32,at+18);
    cc_reserve(bytes);
}
static void bridge_slot(int16_t offset) {
    cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xe1);
    cc_op16(0x11,(uint16_t)offset); cc_emit(0x19);
}
static uint16_t argument_bytes(uint16_t id) {
    TypeId type=CCS(id,type);
    if(CCS(id,role)==S_REFERENCE) return 2;
    if(cc_family(type)==K_REAL||cc_family(type)==K_LONGWORD)return CCT(type,width);
    if(cc_family(type)==K_SET) return 32;
    return 2; /* scalars, pointers and aggregate source addresses */
}
static void bridge_body(uint16_t scope) {
    uint16_t count=cc_rget(scope,R_COUNT),first=cc_rget(scope,R_PARAMETERS),i,width;
    uint16_t routine=cc_rget(scope,R_SYMBOL);
    uint8_t string_result=CCS(routine,role)==S_FUNCTION&&cc_family(CCS(routine,type))==K_STRING;
    width=BRIDGE_PRIVATE_BYTES+count*2;
    bridge_guard(width+4); cc_emit(0xeb); /* DE owns the descriptor */
    cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xdd); cc_op16(0x21,0); cc_emit(0xdd); cc_emit(0x39);
    cc_op16(0x21,(uint16_t)(0u-width)); cc_emit(0x39); cc_emit(0xf9);
    indexed(0x75,248); indexed(0x74,249);
    cc_emit(0xeb); cc_op16(0x01,10); cc_emit(0x09);
    cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56);
    indexed(0x73,254); indexed(0x72,255);
    cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xe1); indexed(0x75,252); indexed(0x74,253);
    cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe1); indexed(0x75,246); indexed(0x74,247);
    cc_op16(0x2a,RT_PC); indexed(0x75,238); indexed(0x74,239);
    bridge_slot(4); cc_emit(0xe5); cc_emit(0xfd); cc_emit(0xe1); /* IY scans original actuals */
    for(i=count;i&&!cc.error->code;--i) {
        bridge_slot((int16_t)(0u-BRIDGE_PRIVATE_BYTES-i*2));
        cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xd1);
        cc_emit(0x73); cc_emit(0x23); cc_emit(0x72);
        if(CCS(first+i-1,role)==S_VALUE&&cc_family(CCS(first+i-1,type))==K_STRING) {
            cc_emit(0xfd); cc_emit(0x5e); cc_emit(0); cc_emit(0x16); cc_emit(0); cc_emit(0x13);
        } else cc_op16(0x11,argument_bytes(first+i-1));
        cc_emit(0xfd); cc_emit(0x19);
    }
    cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe1); cursor_store();
    if(string_result) {
        width=CCT(CCS(routine,type),width);
        bridge_guard(width); cc_op16(0x21,(uint16_t)(0u-width)); cc_emit(0x39); cc_emit(0xf9);
    }
    indexed(0x6e,254); indexed(0x66,255); bridge_guard(2); cc_emit(0xe5);
    for(i=0;i<count&&!cc.error->code;++i) {
        bridge_guard(2); bridge_slot((int16_t)(0u-BRIDGE_PRIVATE_BYTES-(i+1u)*2));
        cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
        if(CCS(first+i,role)==S_VALUE&&cc_family(CCS(first+i,type))==K_STRING) {
            bridge_guard(257); cc_op16(0xcd,RT_PUSHSTRING);
        } else {
            width=argument_bytes(first+i); bridge_guard(width+2);
            cc_emit(0xeb); cc_op16(0x21,(uint16_t)(0u-width)); cc_emit(0x39); cc_emit(0xf9); cc_emit(0xeb);
            cc_op16(0x01,width); cc_op16(0xcd,RT_MOVE);
        }
    }
    bridge_guard(2); cc_entry_reference(0xcd,scope);
    indexed(0x75,244); indexed(0x74,245); /* result HL */
    indexed(0x73,242); indexed(0x72,243); /* result DE */
    indexed(0x71,240); indexed(0x70,241); /* result BC */
    indexed(0x6e,238); indexed(0x66,239); cc_op16(0x22,RT_PC);
    if(string_result) {
        cc_op16(0x21,0); cc_emit(0x39); indexed(0x5e,250); indexed(0x56,251);
        cc_emit(0x0e); cc_emit(CCT(CCS(routine,type),capacity));
        bridge_guard(2); cc_op16(0xcd,RT_COPY);
        cursor_load(); indexed(0x75,244); indexed(0x74,245);
    }
    /* Build a return packet below our own frame, then memmove it into retired
     * original actual slots. Original arguments survive until the callee has
     * finished. IY holds the destination while the old frame is overwritten. */
    bridge_guard(14);
    indexed(0x6e,2); indexed(0x66,3); cc_emit(0xe5);
    indexed(0x6e,0); indexed(0x66,1); cc_emit(0xe5);
    indexed(0x6e,246); indexed(0x66,247); cc_emit(0xe5);
    indexed(0x6e,240); indexed(0x66,241); cc_emit(0xe5);
    indexed(0x6e,242); indexed(0x66,243); cc_emit(0xe5);
    indexed(0x6e,244); indexed(0x66,245); cc_emit(0xe5);
    cursor_load(); cc_op16(0x11,(uint16_t)-12); cc_emit(0x19);
    cc_emit(0xe5); cc_emit(0xfd); cc_emit(0xe1); cc_emit(0xeb);
    cc_op16(0x21,0); cc_emit(0x39); cc_op16(0x01,12); cc_op16(0xcd,RT_MOVE);
    cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe1); cc_emit(0xf9);
    cc_emit(0xe1); cc_emit(0xd1); cc_emit(0xc1);
    cc_emit(0xfd); cc_emit(0xe1); cc_emit(0xdd); cc_emit(0xe1); cc_emit(0xc9);
}
static uint16_t bridge_descriptor(uint16_t scope,uint8_t dynamic) {
    uint16_t skip=cc_forward(0xc3),entry=cc.emitted,descriptor,i;
    bridge_body(scope); descriptor=cc.emitted;
    cc_emit('Z'); cc_emit('T'); cc_emit('H'); cc_emit('K'); cc_emit(1); cc_emit(dynamic);
    cc_emit(16); cc_emit(0);
    cc_relocate(cc.emitted); cc_emit((uint8_t)entry); cc_emit((uint8_t)(entry>>8));
    for(i=0;i<6;++i) cc_emit(0); /* lexical frame, lifetime, reserved */
    if(dynamic) cc_op16(0x21,0); else cc_address(0x21,descriptor);
    cc_address(0xc3,entry); cc_patch(skip);
    return descriptor;
}
void cc_routine_address(uint16_t id) ROUTINES {
    uint16_t scope=CCS(id,role)==S_RESULT?CCS(id,owner):CCS(id,initial_type);
    uint16_t parent=cc_rget(scope,R_PARENT);uint8_t dynamic;
    uint16_t descriptor,slot,locals;
    if(!parent) { cc_entry_reference(0x21,scope); return; }
    if(cc.suppress||cc.error->code) return;
    /* Captured callbacks need resident descriptors and a live owner. */
    if(cc_root_member()) {
#ifdef ZP_OVERLAYS
        cc_fail(UNIT_UNSUPPORTED);
#else
        cc_fail(99);
#endif
        return;
    }
    dynamic=cc_frame_required(parent);
    descriptor=cc_rget(scope,R_DESCRIPTOR);
    if(!descriptor) {
        descriptor=bridge_descriptor(scope,dynamic); cc_rput(scope,R_DESCRIPTOR,descriptor);
    }
    if(!dynamic) { cc_address(0x21,descriptor+16); return; }
    slot=cc_rget(scope,R_THUNK);
    if(!slot) {
        locals=cc_rget(parent,R_LOCALS);
        if((uint32_t)locals+22>0x7ff0u) { cc_fail(98); return; }
        locals+=22; cc_rput(parent,R_LOCALS,locals);
        slot=(uint16_t)(0u-FRAME_PRIVATE_BYTES-locals); cc_rput(scope,R_THUNK,slot);
    }
    cc_frame(parent); cc_reserve(6); cc_emit(0xe5); /* live declaring frame */
    cc_op16(0x11,slot); cc_emit(0x19); cc_emit(0xe5); cc_emit(0xeb);
    cc_address(0x21,descriptor); cc_op16(0x01,22); cc_call(RT_MOVE);
    cc_emit(0xe1); cc_emit(0xd1); cc_emit(0xe5); /* descriptor, owner, saved descriptor */
    cc_op16(0x01,10); cc_emit(0x09);
    cc_emit(0x73); cc_emit(0x23); cc_emit(0x72); cc_emit(0x23);
    cc_emit(0x73); cc_emit(0x23); cc_emit(0x72); /* lifetime is the live frame's ID */
    cc_op16(0x01,4); cc_emit(0x09); cc_emit(0xd1);
    cc_emit(0x73); cc_emit(0x23); cc_emit(0x72); /* LD HL,descriptor operand */
    cc_emit(0xeb); cc_op16(0x01,16); cc_emit(0x09);
}
