#include "sources_internal.h"

static void version(uint8_t page,uint16_t at,uint32_t value) {
    uint8_t i;
    for(i=0;i<4;++i)far_set(page,at+i,(uint8_t)(value>>(i*8)));
}
uint8_t sources_storage(Sources *set,uint8_t packed) SOURCE_BLOCK_CALL {
    Source root;uint16_t i,base=0,bytes;uint8_t page,id;
    if(!sources_valid(set))return 0;
    if(packed==(set->base!=0)||!set->count)return 1;
    if(!sources_get(set,0,&root))return 0;
    bytes=(uint16_t)set->count*RECORD_BYTES;
    if(packed) {
        if(sources_borrowed(set,0)||root.page==255)return 1;
        if(page_role(root.page)!=PAGE_SNAPSHOT)return 0;
        if(root.length>=PAGE_SIZE-bytes)return 1;
        for(id=0;id<set->count;++id)if(!sources_ready(set,id))return 0;
        page=root.page;base=root.length+1;
    } else {
        page=page_new(PAGE_SOURCES);if(page==255)return 0;
    }
    for(i=0;i<bytes;++i)far_set(page,base+i,far_get(set->page,set->base+i));
    if(packed) {
        if(!page_drop(set->page)) {
            /* Tail writes preserve source bytes. The old catalog still owns
             * the root; refresh its version for a safe failure/retry. */
            version(set->page,OWN_VERSION,page_versions[root.page]);
            set->version=page_versions[set->page];return 0;
        }
    } else {
        /* The root becomes an ordinary separately owned snapshot again.
         * Its last catalog writes are part of its current page version. */
        version(page,OWN_VERSION,page_versions[root.page]);
        page_initialized(root.page,root.length);
    }
    set->page=page;set->base=base;
    set->lifetime=page_lifetime(page);set->version=page_versions[page];
    page_initialized(page,base+bytes);return 1;
}
