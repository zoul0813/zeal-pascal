#include "sources_internal.h"
#include "path.h"
#include "compiler_job_wire.h"

static uint8_t view_record(const Sources *set,uint8_t id,JobSource *record) {
    uint8_t allowed[8],i;uint16_t at=set->base&~SOURCES_WIRE_VIEW;
    if(!sources_valid(set)||id>=set->count)return 0;
    memset(allowed,255,8);
    for(i=0;i<=id;++i)if(!job_read_source(set->page,&at,PAGE_SIZE,allowed,record))return 0;
    return 1;
}
uint8_t sources_view(Sources *set,uint8_t page,uint16_t base,uint8_t count) SOURCE_BLOCK_CALL {
    if(base>=PAGE_SIZE||!count||count>SOURCE_COUNT||page_role(page)!=PAGE_WORKER)return 0;
    memset(set,0,sizeof(*set));set->page=page;set->count=count;set->base=base|SOURCES_WIRE_VIEW;
    set->lifetime=page_lifetime(page);set->version=page_versions[page];return 1;
}

uint8_t sources_canonical(const char *name,char *out) SOURCE_BLOCK_CALL {
    return path_resolve(name,"",out);
}
uint8_t sources_directory(const char *name,const char *directory,char *out) SOURCE_BLOCK_CALL {
    return path_directory(name,directory,out);
}


static uint32_t word32(const Sources *set,uint16_t at) {
    uint8_t i;uint32_t value=0;
    for(i=0;i<4;++i)value|=(uint32_t)far_get(set->page,at+i)<<(i*8);
    return value;
}
/* Callers authenticate the catalog before mutating it. Page numbers alone
 * cannot identify an owner once a block edit has released its old page. */
static uint8_t sources_owned(const Sources *set,uint8_t id,uint8_t page) {
    uint16_t at=set->base+(uint16_t)id*RECORD_BYTES;
    return far_get(set->page,at)==page&&page_lifetime(page)&&
        page_lifetime(page)==word32(set,at+OWN_LIFETIME);
}
static uint8_t sources_release(const Sources *set,uint8_t id) {
    uint8_t page=far_get(set->page,set->base+(uint16_t)id*RECORD_BYTES);
    if(far_get(set->page,set->base+(uint16_t)id*RECORD_BYTES+ORIGIN)==SOURCE_BORROWED)return 1;
    return !sources_owned(set,id,page)||page_drop(page);
}

static void put32(Sources *set,uint16_t at,uint32_t value) {
    uint8_t i;
    for(i=0;i<4;++i)far_set(set->page,at+i,(uint8_t)(value>>(i*8)));
}
uint8_t sources_valid(const Sources *set) SOURCE_BLOCK_CALL {
    if(set->base&SOURCES_WIRE_VIEW)return set->page!=255&&page_role(set->page)==PAGE_WORKER&&
        set->lifetime==page_lifetime(set->page)&&set->version==page_versions[set->page]&&
        set->count&&set->count<=SOURCE_COUNT&&(set->base&~SOURCES_WIRE_VIEW)<PAGE_SIZE;
    return set->page!=255&&page_role(set->page)==(set->base?PAGE_SNAPSHOT:PAGE_SOURCES)&&
        set->lifetime==page_lifetime(set->page)&&set->count<=SOURCE_COUNT&&
        set->base<=PAGE_SIZE-(uint16_t)set->count*RECORD_BYTES&&
        (!set->base||set->count)&&
        set->version==page_versions[set->page];
}
uint8_t sources_new(Sources *set) SOURCE_BLOCK_CALL {
    memset(set,0,sizeof(*set));set->page=page_new(PAGE_SOURCES);
    if(set->page==255)return 0;
    set->lifetime=page_lifetime(set->page);set->version=page_versions[set->page];
    page_initialized(set->page,0);return 1;
}
uint8_t sources_get(const Sources *set,uint8_t id,Source *source) SOURCE_BLOCK_CALL {
    uint16_t i,at=set->base+(uint16_t)id*RECORD_BYTES;
    if(set->base&SOURCES_WIRE_VIEW) {JobSource record;if(!view_record(set,id,&record))return 0;*source=record.source;return 1;}
    if(!sources_valid(set)||id>=set->count)return 0;
    for(i=0;i<sizeof(*source);++i)((uint8_t *)source)[i]=far_get(set->page,at+i);
    return 1;
}
int16_t sources_find(const Sources *set,const char *path) SOURCE_BLOCK_CALL {
    uint8_t id;uint16_t j,at;
    if(!sources_valid(set))return -1;
    if(set->base&SOURCES_WIRE_VIEW) {
        JobSource record;
        for(id=0;id<set->count;++id)if(view_record(set,id,&record)&&!strcmp(record.source.path,path))return id;
        return -1;
    }
    for(id=0;id<set->count;++id) {
        at=set->base+(uint16_t)id*RECORD_BYTES+12;
        for(j=0;j<128;++j) {
            uint8_t c=far_get(set->page,at+j);
            if(c!=(uint8_t)path[j])break;
            if(!c)return id;
        }
    }
    return -1;
}
static void forget_record(Sources *set,uint8_t id) {
    uint16_t i,end=set->base+(uint16_t)set->count*RECORD_BYTES;
    for(i=set->base+(uint16_t)(id+1)*RECORD_BYTES;i<end;++i)
        far_set(set->page,i-RECORD_BYTES,far_get(set->page,i));
    --set->count;
}
static int16_t sources_put(Sources *set,const Source *source,const Source *origin,const uint8_t *hash,uint8_t borrowed) {
    uint16_t at,i;int16_t id,owner=-1;uint8_t role=page_role(source->page);
    if(!hash||(set->base&SOURCES_WIRE_VIEW)||!sources_valid(set)||strlen(source->path)>127||source->length>SOURCE_LIMIT||
       (source->page==255?(!hash||(!borrowed&&origin)||(borrowed&&source->length)):
        (role!=PAGE_SOURCE&&role!=PAGE_SNAPSHOT))||
       (origin&&!page_lifetime(origin->page)))return -1;
    id=sources_find(set,source->path);
    for(i=0;i<set->count;++i)
        if(sources_owned(set,(uint8_t)i,source->page))owner=(int16_t)i;
    if(id<0&&owner<0&&set->count==SOURCE_COUNT)return -1;
    /* Keep the packed root alive while its catalog is in its tail. Replacing
     * it, or growing beyond the tail, first separates the catalog. */
    if(set->base&&(id==0||owner==0||
       (id<0&&owner<0&&set->base>PAGE_SIZE-(uint16_t)(set->count+1)*RECORD_BYTES)))
        if(!sources_storage(set,0))return -1;
    if(owner>=0) {
        if(id>=0&&id!=owner) {
            /* Save As replaces the destination owner only after its caller
             * resolves any dirty-buffer conflict. Failure preserves both. */
            if(!sources_release(set,(uint8_t)id))return -1;
            forget_record(set,(uint8_t)id);
            if(owner>id)--owner;
        }
        id=owner; /* Save as changes this owner's path, never duplicates a page. */
    }
    if(id<0&&set->count==SOURCE_COUNT)return -1;
    if(id>=0) {
        if(id!=owner&&!sources_release(set,(uint8_t)id))return -1;
    } else id=set->count++;
    at=set->base+(uint16_t)id*RECORD_BYTES;
    for(i=0;i<sizeof(*source);++i)far_set(set->page,at+i,((const uint8_t *)source)[i]);
    for(i=0;i<32;++i)far_set(set->page,at+HASH+i,hash[i]);
    far_set(set->page,at+ORIGIN,borrowed?SOURCE_BORROWED:origin?origin->page:SOURCE_DISK);
    put32(set,at+ORIGIN_LIFETIME,origin?page_lifetime(origin->page):0);
    put32(set,at+ORIGIN_VERSION,origin?page_versions[origin->page]:0);
    put32(set,at+OWN_LIFETIME,page_lifetime(source->page));
    put32(set,at+OWN_VERSION,source->page==255?0:page_versions[source->page]);
    set->version=page_versions[set->page];
    page_initialized(set->page,set->base+(uint16_t)set->count*RECORD_BYTES);return id;
}
int16_t sources_take(Sources *set,const Source *source,const Source *origin) SOURCE_BLOCK_CALL {
    uint8_t hash[32];
    if(page_role(source->page)!=PAGE_SOURCE&&page_role(source->page)!=PAGE_SNAPSHOT)return -1;
    source_hash(source,hash);return sources_put(set,source,origin,hash,0);
}
int16_t sources_borrow(Sources *set,const Source *source,const uint8_t hash[32]) SOURCE_BLOCK_CALL {
    return sources_put(set,source,0,hash,1);
}
int16_t sources_reference(Sources *set,const Source *source,const uint8_t hash[32]) SOURCE_BLOCK_CALL {
    if(source->page!=255)return -1;
    return sources_put(set,source,0,hash,0);
}
uint8_t sources_borrowed(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL {
    if(set->base&SOURCES_WIRE_VIEW) {JobSource record;return view_record(set,id,&record)&&record.resident;}
    return sources_valid(set)&&id<set->count&&
        far_get(set->page,set->base+(uint16_t)id*RECORD_BYTES+ORIGIN)==SOURCE_BORROWED;
}
uint8_t sources_remove(Sources *set,uint8_t id) SOURCE_BLOCK_CALL {
    if(set->base&SOURCES_WIRE_VIEW)return 0;
    if(!sources_valid(set)||id>=set->count)return 0;
    /* The root owns the packed catalog itself. It must outlive all other
     * records, including when a prior include-page release has failed. */
    if(set->base&&!id&&set->count!=1)return 0;
    if(!sources_release(set,id))return 0;
    if(set->base&&!id) {
        memset(set,0,sizeof(*set));set->page=255;return 1;
    }
    forget_record(set,id);set->version=page_versions[set->page];
    page_initialized(set->page,set->base+(uint16_t)set->count*RECORD_BYTES);return 1;
}
uint8_t sources_close(Sources *set) SOURCE_BLOCK_CALL {
    if(set->page==255)return 1;
    if(set->base&SOURCES_WIRE_VIEW) {memset(set,0,sizeof(*set));set->page=255;return 1;}
    if(!set->page&&!set->count&&!set->lifetime){set->page=255;return 1;}
    if(!sources_valid(set))return 0;
    while(set->count)if(!sources_remove(set,set->count-1))return 0;
    if(set->page==255)return 1;
    if(!page_drop(set->page))return 0;
    set->page=255;set->lifetime=0;set->version=0;return 1;
}
uint8_t sources_pack(Sources *set) SOURCE_BLOCK_CALL {
    return sources_storage(set,1);
}
uint8_t sources_hash(const Sources *set,uint8_t id,uint8_t hash[32]) SOURCE_BLOCK_CALL {
    uint8_t i;uint16_t at=set->base+(uint16_t)id*RECORD_BYTES+HASH;
    if(set->base&SOURCES_WIRE_VIEW) {JobSource record;if(!view_record(set,id,&record))return 0;memcpy(hash,record.hash,32);return 1;}
    if(!sources_valid(set)||id>=set->count)return 0;
    for(i=0;i<32;++i)hash[i]=far_get(set->page,at+i);
    return 1;
}
uint8_t sources_ready(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL {
    uint16_t at=set->base+(uint16_t)id*RECORD_BYTES;uint8_t page;
    if(set->base&SOURCES_WIRE_VIEW)return sources_valid(set)&&id<set->count;
    if(!sources_valid(set)||id>=set->count)return 0;
    page=far_get(set->page,at);
    if(page==255)return !word32(set,at+OWN_LIFETIME)&&!word32(set,at+OWN_VERSION)&&
        (far_get(set->page,at+ORIGIN)==SOURCE_DISK||sources_borrowed(set,id));
    /* A packed root and its records share the whole-page version authenticated
     * by sources_valid. Storing that version inside itself would change it. */
    if(!page_lifetime(page)||page_lifetime(page)!=word32(set,at+OWN_LIFETIME)||
       (!(set->base&&page==set->page)&&page_versions[page]!=word32(set,at+OWN_VERSION)))return 0;
    page=far_get(set->page,at+ORIGIN);
    if(page==SOURCE_BORROWED)return 1;
    return page==255||(page_lifetime(page)&&page_lifetime(page)==word32(set,at+ORIGIN_LIFETIME)&&
        page_versions[page]==word32(set,at+ORIGIN_VERSION));
}

uint8_t sources_resident(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL {
    if(set->base&SOURCES_WIRE_VIEW) {JobSource record;return view_record(set,id,&record)&&record.resident;}
    return sources_valid(set)&&id<set->count&&
        far_get(set->page,set->base+(uint16_t)id*RECORD_BYTES+ORIGIN)!=255;
}
uint8_t sources_origin(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL {
    if(set->base&SOURCES_WIRE_VIEW)return 255;
    uint8_t origin=sources_valid(set)&&id<set->count?
        far_get(set->page,set->base+(uint16_t)id*RECORD_BYTES+ORIGIN):255;
    return origin==SOURCE_BORROWED?255:origin;
}
uint8_t sources_byte(const Sources *set,uint8_t id,uint16_t offset) SOURCE_BLOCK_CALL {
    uint16_t at=set->base+(uint16_t)id*RECORD_BYTES,length;uint8_t page;
    if(set->base&SOURCES_WIRE_VIEW) {JobSource record;return view_record(set,id,&record)&&record.resident&&offset<record.source.length?far_get(record.source.page,offset):0;}
    if(!sources_valid(set)||id>=set->count)return 0;
    page=far_get(set->page,at);length=far_get(set->page,at+2)|((uint16_t)far_get(set->page,at+3)<<8);
    if(offset>=length||length>SOURCE_LIMIT||!sources_owned(set,id,page))return 0;
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
