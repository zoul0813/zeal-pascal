#include "compiler/link/link.h"
#include "compiler/link/layout.h"
#ifdef ZP_UNITS_EXPERIMENT
static uint16_t word(const uint8_t *record,uint8_t at) {
    return record[at]|((uint16_t)record[at+1]<<8);
}
uint8_t cc_link_apply(uint16_t code_origin,uint16_t data_origin) ROUTINES {
    uint8_t record[CC_LINK_BYTES],kind,space;uint16_t i,site,offset,value;
    uint32_t address;
    for(i=0;i<cc.links;++i) {
        if(!cc_poll()||!cc_link_read(i,record))return 0;
        site=word(record,0);offset=word(record,4);space=record[6];kind=record[7];
        if(kind>CF_HIGH8||space>CL_RUNTIME||
           (space!=CL_DATA_ADJUST&&word(record,2))||site<CC_CODE_BASE||
           site>=cc.emitted||(kind==CF_WORD16&&site==cc.emitted-1))goto invalid;
        if(space==CL_DATA_ADJUST) {
            if(!word(record,2)||offset||kind==CF_HIGH8)goto invalid;
            value=cc_code_get(site);
            if(kind==CF_WORD16)value|=(uint16_t)cc_code_get(site+1)<<8;
            value=(uint16_t)(value+(uint32_t)word(record,2)*(uint16_t)(data_origin-GLOBAL_BASE));
            if(kind==CF_WORD16)cc_word(site,value);else cc_put(site,(uint8_t)value);
            if(cc.error->code)return 0;
            continue;
        }
        if(space==CL_RUNTIME) {
            if(offset<0x4000u||offset>=GLOBAL_BASE)goto invalid;
            address=cc_runtime_address(offset);
        } else if(space==CL_CODE) {
            if(offset>=cc.emitted-CC_CODE_BASE)goto invalid;
            address=(uint32_t)code_origin+offset;
        } else {
            /* A zero-width final global has the one-past data address. */
            if(offset>cc.image->globals_bytes)goto invalid;
            address=(uint32_t)data_origin+offset;
        }
        if(address>65535UL)goto invalid;
        value=(uint16_t)address;
        if(kind==CF_WORD16)cc_word(site,value);
        else cc_put(site,(uint8_t)(kind==CF_HIGH8?value>>8:value));
        if(cc.error->code)return 0;
    }
    return 1;
invalid:
    cc_fail(99);return 0;
}
#endif
