#include "image.h"
uint8_t build_is_chain(const Build *b) SOURCE_BLOCK_CALL {
    uint8_t i;
    if(page_role(b->pages[0])!=PAGE_IMAGE)return 0;
    for(i=0;i<4;++i)if(far_get(b->pages[0],i)!=(uint8_t)"ZCHN"[i])return 0;
    return 1;
}
uint8_t native_header(const uint8_t *h,uint8_t chain,NativeImage *id) SOURCE_BLOCK_CALL {
    return native_decode(h,chain,id);
}
