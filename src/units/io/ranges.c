#include "units/io/stream.h"

/* Placement bounds come from the validated fragment plan; writes stay in
 * unpublished staging until the complete source-page hash succeeds. */
uint8_t cc_unit_copy_ranges(uint16_t module,const CompilerFragmentLayout *layout,
                           uint16_t position,const uint8_t *bytes,uint8_t length) SOURCE_BLOCK_CALL {
    uint16_t blob=cc_unit_word(module+UNIT_RECORD_BYTES+10),f,count=cc_unit_module_word(module,blob+6);
    uint16_t address,size;uint8_t status;
    uint32_t source,first,last,end=(uint32_t)position+length;
    for(f=0;f<count;++f) {
        status=cc_fragment_layout_get(layout,cc_unit_word(module+8),f,&address);
        if(status==CF_TARGET)continue;
        if(status!=CF_OK){cc_fail(UNIT_FORMAT);return 0;}
        source=(uint32_t)cc_unit_prefix(module)+cc_unit_module_word(module,blob+16+8*f);
        size=cc_unit_module_word(module,blob+18+8*f);
        if(source+size>cc_unit_word(module+4)||(uint32_t)address+size>CC_CODE_LIMIT) {
            cc_fail(UNIT_FORMAT);return 0;
        }
        first=source>position?source:position;last=source+size;if(last>end)last=end;
        for(;first<last;++first) {
            cc_put(address+(uint16_t)(first-source),bytes[(uint16_t)(first-position)]);
            if(cc.error->code)return 0;
        }
    }
    return 1;
}
