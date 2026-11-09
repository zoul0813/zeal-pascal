#include "program/image.h"
#include "compiler/disk.h"
uint8_t build_is_chain(const Build *b) SOURCE_BLOCK_CALL {
    uint8_t i;
    if(cc_disk_active(b))return cc_disk_chain(b);
    if(page_role(b->pages[0])!=PAGE_IMAGE)return 0;
    for(i=0;i<4;++i)if(far_get(b->pages[0],i)!=(uint8_t)"ZCHN"[i])return 0;
    return 1;
}
