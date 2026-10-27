#include "compiler.h"

/* A fresh object's file fields have no binding authority. Globals/static
 * locals use lifetime zero; recursive locals use their live frame identity;
 * New uses the live allocation's payload address. A later registry operation
 * must validate that owner and its generation before trusting token bytes.
 * Pointers and VAR aliases do not recursively acquire storage ownership. */
static uint8_t contains(uint8_t type) {
    uint16_t i;
    uint8_t family=cc_family(type);
    if(family==K_FILE) return 1;
    if(family==K_ARRAY) return contains(cc.types[type].element);
    if(family!=K_RECORD) return 0;
    for(i=0;i<cc.symbol_count;++i) {
        if(cc.symbols[i].role==S_FIELD&&cc.symbols[i].owner==type&&contains(cc.symbols[i].type)) return 1;
        if(cc.symbols[i].role==S_VLABEL&&cc.symbols[i].owner==type&&contains(cc.symbols[i].type)) return 1;
    }
    return 0;
}
uint8_t cc_file_fields(uint8_t type) FRONTEND { return contains(type); }
static uint16_t component(uint8_t type) {
    return cc.types[type].capacity==FILE_TEXT?0:cc.types[type].capacity==FILE_UNTYPED?128:
        cc.types[cc.types[type].element].width;
}
static uint16_t array_count(uint8_t type) {
    Type *index=&cc.types[cc.types[type].capacity];
    return (uint16_t)((int32_t)index->high-index->low+1);
}
/* A union has only one physical initial value. Initialize its first declared
 * arm; another arm's file descriptor must be adopted when that arm is used.
 * Never run this fresh-storage walk over an ordinary aggregate value copy. */
static uint8_t first_arm(uint8_t type) {
    uint16_t i;
    for(i=0;i<cc.symbol_count;++i)
        if(cc.symbols[i].role==S_VLABEL&&cc.symbols[i].owner==type) return cc.symbols[i].type;
    return 255;
}
static void static_fields(uint8_t type,uint16_t address) {
    uint16_t i,n,width;
    uint8_t family=cc_family(type),arm;
    if(family==K_FILE) {
        for(i=0;i<16;++i) cc_put(address-0x4000u+i,0);
        cc_put(address-0x4000u,0x5a); cc_put(address-0x4000u+1,0x46);
        cc_put(address-0x4000u+8,cc.types[type].capacity);
        width=component(type); cc_word(address-0x4000u+10,width);
    } else if(family==K_ARRAY&&contains(cc.types[type].element)) {
        n=array_count(type); width=cc.types[cc.types[type].element].width;
        for(i=0;i<n;++i) static_fields(cc.types[type].element,address+i*width);
    } else if(family==K_RECORD) {
        for(i=0;i<cc.symbol_count;++i)
            if(cc.symbols[i].role==S_FIELD&&cc.symbols[i].owner==type&&contains(cc.symbols[i].type))
                static_fields(cc.symbols[i].type,address+cc.symbols[i].address);
        arm=first_arm(type);
        if(arm!=255&&contains(arm)) static_fields(arm,address+cc.symbols[(uint16_t)cc.types[type].low].address);
    }
}
void cc_file_static(uint8_t type,uint16_t address) FRONTEND {
    if(!cc.error->code&&contains(type)) static_fields(type,address);
}
static void init_call(uint8_t type);
static void field_call(uint8_t type,uint16_t offset) {
    if(!contains(type)) return;
    if(offset) {
        cc_emit(0xe5); cc_emit(0xd5); cc_op16(0x11,offset); cc_emit(0x19); cc_emit(0xd1);
    }
    init_call(type);
    if(offset) cc_emit(0xe1);
}
/* These private helpers preserve HL (object address), DE (owner), IX/IY and
 * alternates. AF/BC are scratch. They allocate no pages, paths or handles.
 * Calls share the normal generated-code relocation ledger. Type.high is
 * otherwise unused for these three non-ordinal families. */
static void init_call(uint8_t type) {
    uint16_t skip,i,loop,width;
    uint8_t family=cc_family(type),arm;
    if(!cc.types[type].high) {
        skip=cc_forward(0xc3);
        cc.types[type].high=(int16_t)(cc.emitted);
        if(family==K_FILE) {
            cc_emit(0xe5); cc_emit(0xd5); cc_op16(0x01,16); cc_emit(0xaf); cc_op16(0xcd,RT_FILL);
            cc_emit(0xd1); cc_emit(0xe1); cc_emit(0xe5);
            cc_emit(0x36); cc_emit(0x5a); cc_emit(0x23); cc_emit(0x36); cc_emit(0x46);
            cc_op16(0x01,5); cc_emit(0x09); cc_emit(0x73); cc_emit(0x23); cc_emit(0x72); cc_emit(0x23);
            cc_emit(0x36); cc_emit(cc.types[type].capacity); cc_emit(0x23); cc_emit(0x23);
            width=component(type); cc_emit(0x36); cc_emit((uint8_t)width); cc_emit(0x23);
            cc_emit(0x36); cc_emit((uint8_t)(width>>8)); cc_emit(0xe1);
        } else if(family==K_ARRAY) {
            cc_emit(0xe5); cc_op16(0x01,array_count(type)); loop=cc.emitted;
            cc_emit(0xc5); init_call(cc.types[type].element); cc_emit(0xc1);
            cc_emit(0xd5); cc_op16(0x11,cc.types[cc.types[type].element].width); cc_emit(0x19); cc_emit(0xd1);
            cc_emit(0x0b); cc_emit(0x78); cc_emit(0xb1); cc_address(0xc2,loop); cc_emit(0xe1);
        } else if(family==K_RECORD) {
            for(i=0;i<cc.symbol_count&&!cc.error->code;++i)
                if(cc.symbols[i].role==S_FIELD&&cc.symbols[i].owner==type)
                    field_call(cc.symbols[i].type,cc.symbols[i].address);
            arm=first_arm(type);
            if(arm!=255) field_call(arm,cc.symbols[(uint16_t)cc.types[type].low].address);
        }
        cc_emit(0xc9); cc_patch(skip);
    }
    cc_address(0xcd,(uint16_t)cc.types[type].high);
}
void cc_file_initialize(uint8_t type) FRONTEND {
    if(cc.error->code||cc.suppress||!contains(type)) return;
    /* Parser depth is bounded to24. Each array/record level retains at most
     * eight bytes; the leaf needs six including its fill helper return. */
    cc_reserve(256); init_call(type);
}
