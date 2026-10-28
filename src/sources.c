#include "sources.h"
#include "path.h"

uint8_t sources_canonical(const char *name,char *out) SOURCE_BLOCK_CALL {
    return path_resolve(name,"",out);
}

/* Private in-memory records, never an installed/source-map ABI. The Source
 * descriptor has the same fixed layout on host and target; counters below
 * are serialized explicitly so host alignment cannot change their offsets. */
typedef char source_layout[sizeof(Source)==140?1:-1];
enum { HASH=140,ORIGIN=172,ORIGIN_LIFETIME=173,ORIGIN_VERSION=177,
       OWN_LIFETIME=181,OWN_VERSION=185,RECORD_BYTES=189,
       SOURCE_COUNT=PAGE_SIZE/RECORD_BYTES };

static uint32_t word32(const Sources *set,uint16_t at) {
    uint8_t i;uint32_t value=0;
    for(i=0;i<4;++i)value|=(uint32_t)far_get(set->page,at+i)<<(i*8);
    return value;
}
static void put32(Sources *set,uint16_t at,uint32_t value) {
    uint8_t i;
    for(i=0;i<4;++i)far_set(set->page,at+i,(uint8_t)(value>>(i*8)));
}
uint8_t sources_valid(const Sources *set) SOURCE_BLOCK_CALL {
    return set->page!=255&&page_role(set->page)==PAGE_SOURCES&&
        set->lifetime==page_lifetime(set->page)&&set->count<=SOURCE_COUNT&&
        set->version==page_versions[set->page];
}
uint8_t sources_new(Sources *set) SOURCE_BLOCK_CALL {
    memset(set,0,sizeof(*set));set->page=page_new(PAGE_SOURCES);
    if(set->page==255)return 0;
    set->lifetime=page_lifetime(set->page);set->version=page_versions[set->page];
    page_initialized(set->page,0);return 1;
}
uint8_t sources_get(const Sources *set,uint8_t id,Source *source) SOURCE_BLOCK_CALL {
    uint16_t i,at=(uint16_t)id*RECORD_BYTES;
    if(!sources_valid(set)||id>=set->count)return 0;
    for(i=0;i<sizeof(*source);++i)((uint8_t *)source)[i]=far_get(set->page,at+i);
    return 1;
}
int16_t sources_find(const Sources *set,const char *path) SOURCE_BLOCK_CALL {
    uint8_t id;uint16_t j,at;
    if(!sources_valid(set))return -1;
    for(id=0;id<set->count;++id) {
        at=(uint16_t)id*RECORD_BYTES+12;
        for(j=0;j<128;++j) {
            uint8_t c=far_get(set->page,at+j);
            if(c!=(uint8_t)path[j])break;
            if(!c)return id;
        }
    }
    return -1;
}
static void forget_record(Sources *set,uint8_t id) {
    uint16_t i,end=(uint16_t)set->count*RECORD_BYTES;
    for(i=(uint16_t)(id+1)*RECORD_BYTES;i<end;++i)
        far_set(set->page,i-RECORD_BYTES,far_get(set->page,i));
    --set->count;
}
int16_t sources_take(Sources *set,const Source *source,const Source *origin) SOURCE_BLOCK_CALL {
    uint16_t at,i;int16_t id,owner=-1;uint8_t prior,hash[32],role=page_role(source->page);
    if(!sources_valid(set)||strlen(source->path)>127||source->length>SOURCE_LIMIT||
       (role!=PAGE_SOURCE&&role!=PAGE_SNAPSHOT)||
       (origin&&!page_lifetime(origin->page)))return -1;
    id=sources_find(set,source->path);
    for(i=0;i<set->count;++i)
        if(far_get(set->page,i*RECORD_BYTES)==source->page)owner=(int16_t)i;
    source_hash(source,hash);
    if(owner>=0) {
        if(id>=0&&id!=owner) {
            /* Save As replaces the destination owner only after its caller
             * resolves any dirty-buffer conflict. Failure preserves both. */
            if(!page_drop(far_get(set->page,(uint16_t)id*RECORD_BYTES)))return -1;
            forget_record(set,(uint8_t)id);
            if(owner>id)--owner;
        }
        id=owner; /* Save as changes this owner's path, never duplicates a page. */
    }
    if(id<0&&set->count==SOURCE_COUNT)return -1;
    if(id>=0) {
        prior=far_get(set->page,(uint16_t)id*RECORD_BYTES);
        if(prior!=source->page&&!page_drop(prior))return -1;
    } else id=set->count++;
    at=(uint16_t)id*RECORD_BYTES;
    for(i=0;i<sizeof(*source);++i)far_set(set->page,at+i,((const uint8_t *)source)[i]);
    for(i=0;i<32;++i)far_set(set->page,at+HASH+i,hash[i]);
    far_set(set->page,at+ORIGIN,origin?origin->page:255);
    put32(set,at+ORIGIN_LIFETIME,origin?page_lifetime(origin->page):0);
    put32(set,at+ORIGIN_VERSION,origin?page_versions[origin->page]:0);
    put32(set,at+OWN_LIFETIME,page_lifetime(source->page));
    put32(set,at+OWN_VERSION,page_versions[source->page]);
    set->version=page_versions[set->page];
    page_initialized(set->page,(uint16_t)set->count*RECORD_BYTES);return id;
}
uint8_t sources_remove(Sources *set,uint8_t id) SOURCE_BLOCK_CALL {
    uint16_t at=(uint16_t)id*RECORD_BYTES;uint8_t page;
    if(!sources_valid(set)||id>=set->count)return 0;
    page=far_get(set->page,at);
    if(!page_drop(page))return 0;
    forget_record(set,id);set->version=page_versions[set->page];
    page_initialized(set->page,(uint16_t)set->count*RECORD_BYTES);return 1;
}
uint8_t sources_close(Sources *set) SOURCE_BLOCK_CALL {
    if(set->page==255)return 1;
    if(!set->page&&!set->count&&!set->lifetime){set->page=255;return 1;}
    if(!sources_valid(set))return 0;
    while(set->count)if(!sources_remove(set,set->count-1))return 0;
    if(!page_drop(set->page))return 0;
    set->page=255;set->lifetime=0;set->version=0;return 1;
}
uint8_t sources_hash(const Sources *set,uint8_t id,uint8_t hash[32]) SOURCE_BLOCK_CALL {
    uint8_t i;uint16_t at=(uint16_t)id*RECORD_BYTES+HASH;
    if(!sources_valid(set)||id>=set->count)return 0;
    for(i=0;i<32;++i)hash[i]=far_get(set->page,at+i);
    return 1;
}
uint8_t sources_ready(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL {
    uint16_t at=(uint16_t)id*RECORD_BYTES;uint8_t page;
    if(!sources_valid(set)||id>=set->count)return 0;
    page=far_get(set->page,at);
    if(!page_lifetime(page)||page_lifetime(page)!=word32(set,at+OWN_LIFETIME)||
       page_versions[page]!=word32(set,at+OWN_VERSION))return 0;
    page=far_get(set->page,at+ORIGIN);
    return page==255||(page_lifetime(page)&&page_lifetime(page)==word32(set,at+ORIGIN_LIFETIME)&&
        page_versions[page]==word32(set,at+ORIGIN_VERSION));
}
uint8_t sources_resident(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL {
    return sources_valid(set)&&id<set->count&&
        far_get(set->page,(uint16_t)id*RECORD_BYTES+ORIGIN)!=255;
}
uint8_t sources_origin(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL {
    return sources_valid(set)&&id<set->count?
        far_get(set->page,(uint16_t)id*RECORD_BYTES+ORIGIN):255;
}
uint8_t sources_byte(const Sources *set,uint8_t id,uint16_t offset) SOURCE_BLOCK_CALL {
    uint16_t at=(uint16_t)id*RECORD_BYTES,length;uint8_t page;
    if(!sources_valid(set)||id>=set->count)return 0;
    page=far_get(set->page,at);length=far_get(set->page,at+2)|((uint16_t)far_get(set->page,at+3)<<8);
    if(offset>=length||length>SOURCE_LIMIT||!page_lifetime(page))return 0;
    return far_get(page,offset);
}
uint8_t sources_path(const char *name,const char *directory,char *out) SOURCE_BLOCK_CALL {
    uint16_t n,base,i;
    if(!path_resolve(name,directory,out))return 0;
    n=(uint16_t)strlen(out);base=n;
    while(base>3&&out[base-1]!='/')--base;
    for(i=base;i<n;++i)if(out[i]=='.')return 1;
    if(n+4>127||n-base+4>15)return 0;
    strcpy(out+n,".PAS");return 1;
}
