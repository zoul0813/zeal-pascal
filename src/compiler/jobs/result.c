#include "compiler/jobs/job.h"
#include "compiler/disk.h"

/* This is an ownership handoff from our synchronous compiler, not an external
 * artifact reader. The emitter owns header/map correctness. Check only the
 * bounded page handles that the caller will adopt; disk readers validate files
 * independently at their actual trust boundary. */
uint8_t compiler_result_ready(const Build *b) SOURCE_BLOCK_CALL {
    uint8_t i;uint8_t j;uint8_t count;
    if(!b->valid||b->length<96||b->length>48128u||
       b->map_length<MAP_RECORD_BYTES||b->map_length>SOURCE_LIMIT||
       !sources_valid(&b->sources)||b->sources.text_page!=255||!b->sources.count||
       !compiler_page_owned(compiler_owned,b->sources.page)||
       !compiler_page_owned(compiler_owned,b->map_page)||page_role(b->map_page)!=PAGE_MAP)return 0;
    if(b->valid==2)return cc_disk_ready(b)&&b->pages[0]==255&&b->pages[1]==255&&b->pages[2]==255;
    count=(uint8_t)((b->length+PAGE_SIZE-1u)/PAGE_SIZE);
    for(i=0;i<3;++i) {
        if(i>=count) {if(b->pages[i]!=255)return 0;continue;}
        if(!compiler_page_owned(compiler_owned,b->pages[i])||
           page_role(b->pages[i])!=PAGE_IMAGE)return 0;
        for(j=0;j<i;++j)if(b->pages[j]==b->pages[i])return 0;
    }
    return 1;
}
