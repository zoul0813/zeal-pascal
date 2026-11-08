#include "compiler_layout.h"
#include "compiler.h"
#include "compiler_disk.h"

/* Code-only transfers reuse the prefix. Each program therefore owns the
 * executable bindings it installs there, including restoring unused native
 * entries after a previous program installed adapters. This straight-line
 * entry makes no call, stack allocation or ordinary-global initialization. */
void cc_activation_begin(void) FRONTEND {
    uint8_t i;
    for(i=0;i<CC_ACTIVATION_BYTES;++i)cc_emit(0);
    /* Chain starts a new program session without running prefix start. */
    cc_emit(0xaf);cc_runtime_op(0x32,cc_runtime_private(RP_CRT_ACTIVE));
}
static uint8_t byte(uint16_t address) {
    uint16_t p=address-0x4000u;
    return image_get(cc.image,p);
}
static void put(uint16_t *at,uint8_t value) {
    uint16_t p=(*at)++;
    cc_put(p,value);
}
static void word(uint16_t *at,uint16_t value) {
    put(at,(uint8_t)value);put(at,(uint8_t)(value>>8));
}
static void store_byte(uint16_t *at,uint16_t target,uint8_t value) {
    put(at,0x3e);put(at,value);put(at,0x32);word(at,target);
}
static void store_word(uint16_t *at,uint16_t target,uint16_t value) {
    put(at,0x21);word(at,value);put(at,0x22);word(at,target);
}
void cc_activation_place(void) FRONTEND {
    uint16_t at=cc.image->code_origin-0x4000u,end=at+CHAIN_ENTRY_BYTES,site,target;
    uint8_t i,first,lo,hi;
    for(i=0;i<5;++i) {
        if(!cc_poll())return;
        site=i==0?cc_runtime_private(RP_INPUT_KEY):i==1?
            cc_runtime_get(RT_CHAR-0x4000u+1)|((uint16_t)cc_runtime_get(RT_CHAR-0x4000u+2)<<8):
            cc_runtime_private(i==2?RP_INPUT_ECHO:i==3?RP_INPUT_OPEN:RP_NORMAL_ERROR);
        if(site<0x4000u||site>GLOBAL_BASE-3) {cc_fail(99);return;}
        target=i==0?cc.keyboard_entry:i==1?cc.hook_character:
            i==2?cc.hook_echo:i==3?cc.hook_open:cc.error_entry;
        /* CHN owns its adapters, but borrows the caller's runtime. Restore
         * unused entries from the immutable library, not its empty prefix. */
        if(target) {target+=cc.image->code_origin-CC_CODE_BASE;
            first=0xc3;lo=(uint8_t)target;hi=(uint8_t)(target>>8);}
        else {first=cc_runtime_get(site-0x4000u);
            lo=cc_runtime_get(site-0x4000u+1);hi=cc_runtime_get(site-0x4000u+2);}
        store_byte(&at,site,first);
        store_word(&at,site+1,lo|((uint16_t)hi<<8));
    }
    store_byte(&at,RT_CBREAK,byte(RT_CBREAK));
    for(i=0;i<HOOK_COUNT;++i)if(cc.hook_variables[i]) {
        site=cc.hook_variables[i];
        store_word(&at,site,byte(site)|((uint16_t)byte(site+1)<<8));
    }
    /* The fixed entry size keeps declaration/code relocation independent of
     * which predefined callback variables this program happens to reference. */
    if(at>end-3) {cc_fail(99);return;}
    while(at<end-3)put(&at,0);
    put(&at,0xc3);word(&at,cc.image->code_origin+CC_ACTIVATION_BYTES);
    for(i=0;i<CHAIN_PROFILE_BYTES;++i)put(&at,cc_runtime_profile_get(i));
}
