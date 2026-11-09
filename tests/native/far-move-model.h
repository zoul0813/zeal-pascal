#ifndef ZP_TEST_FAR_MOVE_MODEL_H
#define ZP_TEST_FAR_MOVE_MODEL_H
#include "../../src/common/zp.h"
/* Host oracle stages the complete span, independently of native LDIR/LDDR
 * direction. Existing far_get/set spies still observe every byte/version. */
static uint8_t model_far_move(uint8_t page,uint16_t destination,uint16_t source,uint16_t count) {
    uint8_t bytes[FAR_MOVE_LIMIT];uint16_t i;
    if(!count)return 1;
    if(count>FAR_MOVE_LIMIT||page<32||page>=64||source>=PAGE_SIZE||destination>=PAGE_SIZE||
       count>PAGE_SIZE-source||count>PAGE_SIZE-destination)return 0;
    for(i=0;i<count;++i)bytes[i]=far_get(page,source+i);
    for(i=0;i<count;++i)far_set(page,destination+i,bytes[i]);
    return 1;
}
#endif
