#include "source_move.h"

/* Preserve memmove order across banks as well as within each bounded chunk. */
uint8_t source_move_range(uint8_t page,uint16_t destination,uint16_t source,uint16_t count) SOURCE_BLOCK_CALL {
    uint16_t n,at,a,z,i,last;uint8_t from,to,backward=destination>source;
    if(!count)return 1;
    if(page<32||page>=64)return 0;
    if((uint32_t)source+count>SOURCE_LIMIT)return 0;
    if((uint32_t)destination+count>SOURCE_LIMIT)return 0;
    last=source+count;--last;
    if(buffer_page(page,last)==255)return 0;
    last=destination+count;--last;
    if(buffer_page(page,last)==255)return 0;
    while(count) {
        n=count>FAR_MOVE_LIMIT?FAR_MOVE_LIMIT:count;count-=n;at=backward?count:0;
        a=source+at;z=destination+at;
        /* The small same-bank fast path retains the original checked mover. */
        from=buffer_page(page,a);to=buffer_page(page,z);
        if(from==255||to==255)return 0;
        if(from==to&&a%PAGE_SIZE+n<=PAGE_SIZE&&z%PAGE_SIZE+n<=PAGE_SIZE) {
            if(!far_move(from,z%PAGE_SIZE,a%PAGE_SIZE,n))return 0;
            if(to!=page)page_versions[page]+=n;
        } else {
            if(backward) {
                i=n;while(i){--i;buffer_set(page,z+i,buffer_get(page,a+i));}
            } else for(i=0;i<n;++i)buffer_set(page,z+i,buffer_get(page,a+i));
        }
        if(!backward){source+=n;destination+=n;}
    }
    return 1;
}
