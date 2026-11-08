#include "compiler_units.h"
#include "unsigned-engine.h"

/* One private code fragment, copied only when used. All its internal words
 * use the ordinary code relocation ledger, including separately compiled units. */
static uint16_t engine(void) {
    uint16_t skip,at,i,target,r=0,owner=cc.sources->unit_owner;
    if(cc.unsigned_entry)return cc.unsigned_entry;
    skip=cc_forward(0xc3);cc_units_owner(0xffffu);at=cc.emitted;cc.unsigned_entry=at;
    for(i=0;i<sizeof(unsigned_code);++i) {
        if(r<sizeof(unsigned_relocations)/sizeof(unsigned_relocations[0])&&i==unsigned_relocations[r]) {
            target=unsigned_code[i];target|=(uint16_t)unsigned_code[i+1]*256u;target+=at;
            cc_relocate(cc.emitted);cc_emit((uint8_t)target);cc_emit((uint8_t)(target>>8));++i;++r;
        } else cc_emit(unsigned_code[i]);
    }
    cc_units_owner(owner);cc_patch(skip);return at;
}
void cc_unsigned_call(uint8_t op) BACKEND {
    uint16_t entry;
    if(cc.suppress||cc.error->code)return;
    entry=engine();cc_reserve(96);cc_emit(0x3e);cc_emit(op);cc_address(0xcd,entry);
}
