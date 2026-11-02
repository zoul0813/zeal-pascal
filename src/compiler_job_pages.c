#include "compiler_job.h"

/* Cleanup remains in a retained bank: a failed job must be reclaimable before
 * restoring compiler phases that may need those same physical pages. */
uint8_t compiler_pages_release(uint8_t owned[8],const uint8_t *keep) SOURCE_BLOCK_CALL {
    uint8_t page;uint8_t ok=1;uint8_t error=0;
    compiler_allocating=COMPILER_ALLOC_NONE;
    for(page=0;page<64;++page)if(owned[page>>3]&(1u<<(page&7))) {
        if(keep&&(keep[page>>3]&(1u<<(page&7))))continue;
        if(!page_drop(page)) {ok=0;if(!error)error=platform_error;}
    }
    if(ok&&keep)for(page=0;page<8;++page)owned[page]&=~keep[page];
    if(error)platform_error=error;
    return ok;
}
uint8_t compiler_resources_ready(void) SOURCE_BLOCK_CALL {
    uint8_t workers;uint8_t parent;uint8_t error;
    workers=compiler_pages_release(compiler_owned,0);error=platform_error;
    parent=compiler_pages_release(compiler_parent_owned,0);
    if(!workers)platform_error=error;
    return workers&&parent;
}

uint8_t compiler_page_owned(const uint8_t mask[8],uint8_t page) SOURCE_BLOCK_CALL {
    return page<64&&(mask[page>>3]&(1u<<(page&7)))&&page_lifetime(page);
}
