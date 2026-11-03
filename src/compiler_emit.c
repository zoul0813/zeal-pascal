#include "compiler.h"
#include "compiler_disk.h"

/* Every compiler bank emits through this retained path. Mapping a compiler
 * bank just to write one output byte adds no ownership or correctness value. */
void cc_put(uint16_t p,uint8_t v) {
    if(cc.error->code) return;
    if(cc_disk_active(cc.image)) {
        if(compiler_disk_placed)cc_disk_final_put(p,v);else cc_disk_put(p,v);
        return;
    }
    if(!cc.placing&&p>=CC_CODE_BASE&&p<0xc000u&&cc.primary_stage!=255) {
        far_set(cc.primary_stage,p-CC_CODE_BASE,v);return;
    }
    if(p>=0xc000u) {
        cc_stage_put(p-0xc000u,v);
    } else {
        if(cc.image->pages[p/PAGE_SIZE]==255&&!cc_image_page(p/PAGE_SIZE))return;
        far_set(cc.image->pages[p/PAGE_SIZE],p%PAGE_SIZE,v);
    }
}
uint8_t cc_code_get(uint16_t address) {
    if(cc.error->code) return 0;
    if(address<CC_CODE_BASE||address>=cc.emitted) { cc_fail(99); return 0; }
    if(cc_disk_active(cc.image))return cc_disk_get(address);
    return address>=0xc000u?cc_stage_get(address-0xc000u):
        far_get(cc.primary_stage==255?cc.image->pages[2]:cc.primary_stage,address-CC_CODE_BASE);
}
void cc_word(uint16_t p,uint16_t v) { cc_put(p,(uint8_t)v); cc_put(p+1,(uint8_t)(v>>8)); }



void cc_emit(uint8_t v) {
    if(cc.error->code||cc.suppress) return;
    if(!(cc.emitted&255)&&!cc_poll())return;
    /* The first staging page reuses image page2. A second owned compiler page
     * is acquired only if necessary, then released after checked placement. */
    if(cc.emitted>=CC_STATIC_LIMIT) { cc_fail(99); return; }
    cc_put(cc.emitted++,v);
}
static void emit16(uint16_t v) { cc_emit((uint8_t)v); cc_emit((uint8_t)(v>>8)); }
void cc_op16(uint8_t op,uint16_t value) { cc_emit(op); emit16(value); }
