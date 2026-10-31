#include "source_move.h"

/* Chunk order preserves overlap across the whole range; far_move owns the
 * direction within each checked chunk. This bank stays loaded during Run. */
uint8_t source_move_range(uint8_t page,uint16_t destination,uint16_t source,uint16_t count) SOURCE_BLOCK_CALL {
    uint16_t n,at;uint8_t backward=destination>source;
    if(!count)return 1;
    if(page<32||page>=64||source>=PAGE_SIZE||destination>=PAGE_SIZE||
       count>PAGE_SIZE-source||count>PAGE_SIZE-destination)return 0;
    while(count) {
        n=count>FAR_MOVE_LIMIT?FAR_MOVE_LIMIT:count;count-=n;
        at=backward?count:0;
        if(!far_move(page,destination+at,source+at,n))return 0;
        if(!backward){source+=n;destination+=n;}
    }
    return 1;
}
