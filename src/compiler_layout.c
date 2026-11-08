#include "compiler_layout.h"
#include "compiler_disk.h"
#ifdef ZP_STATIC_LAYOUT
#ifdef __SDCC
#pragma codeseg BANK19
#pragma constseg BANK19
#endif
#define ZP_RUNTIME_RELOCATIONS
#include "runtime-layout.h"
#endif
uint8_t cc_layout_finish(void) ROUTINES {
#ifdef ZP_STATIC_LAYOUT
    uint16_t at,to,i,value,end=cc_data_base(),site;uint8_t kind;
    if(!cc_layout_active())return 1;
    if(!cc.real_needed) {
        for(at=RUNTIME_GAP_LAST;at<RUNTIME_OWNED_END;++at) {
            if(!(at&255u)&&!cc_poll())return 0;
            cc_put(at-RUNTIME_GAP_BYTES-0x4000u,image_get(cc.image,at-0x4000u));
        }
        for(i=0;i<sizeof(runtime_relocations)/sizeof(runtime_relocations[0]);++i) {
            if(!(i&31u)&&!cc_poll())return 0;
            site=cc_runtime_address(runtime_relocations[i].site)-0x4000u;
            value=cc_runtime_address(runtime_relocations[i].target);kind=runtime_relocations[i].kind;
            if(!kind)cc_word(site,value);else cc_put(site,(uint8_t)(kind==2?value>>8:value));
        }
    }
    /* Disk staging keeps data at its logical position and translates at publication. */
    if(!cc_disk_active(cc.image)) {
        cc.placing=1;
        to=end-0x4000u;
        for(at=0;at<cc.image->globals_bytes;++at) {
            if(!(at&255u)&&!cc_poll())return 0;
            cc_put(to+at,image_get(cc.image,PREFIX_BYTES+at));
        }
    }
#endif
    return !cc.error->code;
}
