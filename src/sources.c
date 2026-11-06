#include "sources.h"
#include "path.h"

/* A two-byte directory per file, then variable-length descriptors:
 * page:u8, length:u16, text-base:u16, revision:u32, NUL-terminated path.
 * Text pages are borrowed views; the arena below has exactly one owner. */
#define DIRECTORY_BYTES 512u
#ifdef ZP_UNITS_EXPERIMENT
#define RECORD_PATH 14u
#else
#define RECORD_PATH 9u
#endif
static uint16_t word(const Sources *s,uint16_t at) {
    return buffer_get(s->page,at)|((uint16_t)buffer_get(s->page,at+1)<<8);
}
static void put(Sources *s,uint16_t at,uint16_t value) {
    buffer_set(s->page,at,(uint8_t)value);buffer_set(s->page,at+1,(uint8_t)(value>>8));
}
static uint16_t record(const Sources *s,uint8_t id) {return word(s,(uint16_t)id*2u);}
uint8_t sources_canonical(const char *name,char *out) SOURCE_BLOCK_CALL {return path_resolve(name,"",out);}
uint8_t sources_directory(const char *name,const char *directory,char *out) SOURCE_BLOCK_CALL {return path_directory(name,directory,out);}
uint8_t sources_valid(const Sources *s) SOURCE_BLOCK_CALL {
    return s->page!=255&&page_role(s->page)==PAGE_SOURCES&&
        page_lifetime(s->page)==s->lifetime&&s->used>=DIRECTORY_BYTES;
}
uint8_t sources_new(Sources *s) SOURCE_BLOCK_CALL {
    memset(s,0,sizeof(*s));s->page=s->text_page=s->text_tail=s->pending=255;
    s->page=page_new(PAGE_SOURCES);if(s->page==255)return 0;
    buffer_init(s->page);s->lifetime=page_lifetime(s->page);s->used=DIRECTORY_BYTES;return 1;
}
uint8_t sources_get(const Sources *s,uint8_t id,Source *out) SOURCE_BLOCK_CALL {
    uint16_t at;uint8_t i,c;
    if(!sources_valid(s)||id>=s->count)return 0;
    at=record(s,id);memset(out,0,sizeof(*out));out->page=buffer_get(s->page,at);
    out->length=word(s,at+1);
    for(i=0;i<4;++i)out->revision|=(uint32_t)buffer_get(s->page,at+5+i)<<(8*i);
    for(i=0;i<128;++i){c=buffer_get(s->page,at+RECORD_PATH+i);out->path[i]=(char)c;if(!c)return 1;}
    return 0;
}
uint16_t sources_offset(const Sources *s,uint8_t id) SOURCE_BLOCK_CALL {return word(s,record(s,id)+3);}
static int16_t find_source(const Sources *s,const char *path,uint8_t text_only) {
    uint8_t id,i,c;uint16_t at;
    if(!sources_valid(s))return -1;
#ifndef ZP_UNITS_EXPERIMENT
    (void)text_only;
#endif
    for(id=0;id<s->count;++id){
        at=record(s,id);
#ifdef ZP_UNITS_EXPERIMENT
        if(text_only&&buffer_get(s->page,at+9))continue;
#endif
        at+=RECORD_PATH;
        for(i=0;i<128;++i){c=buffer_get(s->page,at+i);if(c!=(uint8_t)path[i])break;if(!c)return id;}}
    return -1;
}
int16_t sources_find(const Sources *s,const char *path) SOURCE_BLOCK_CALL {
    return find_source(s,path,0);
}
#ifdef ZP_UNITS_EXPERIMENT
int16_t sources_find_input(const Sources *s,const char *path) SOURCE_BLOCK_CALL {
    return find_source(s,path,1);
}
#endif
static int16_t descriptor(Sources *s,const Source *source,uint16_t base,uint8_t append) {
    int16_t id;uint16_t at,n;uint8_t i;
    if(!sources_valid(s)||strlen(source->path)>127)return -1;
    id=append?-1:find_source(s,source->path,1);
    if(id<0){
        if(s->count==255)return -1;
        n=RECORD_PATH+(uint16_t)strlen(source->path)+1;
        if((uint32_t)s->used+n>SOURCE_LIMIT||!buffer_reserve(s->page,s->used+n))return -1;
        id=s->count++;at=s->used;s->used+=n;put(s,(uint16_t)id*2u,at);
#ifdef ZP_UNITS_EXPERIMENT
        for(i=9;i<RECORD_PATH;++i)buffer_set(s->page,at+i,0);
#endif
        for(i=0;i<n-RECORD_PATH;++i)buffer_set(s->page,at+RECORD_PATH+i,(uint8_t)source->path[i]);
    }else at=record(s,(uint8_t)id);
    buffer_set(s->page,at,source->page);put(s,at+1,source->length);put(s,at+3,base);
    for(i=0;i<4;++i)buffer_set(s->page,at+5+i,(uint8_t)(source->revision>>(i*8)));
    return id;
}
int16_t sources_borrow(Sources *s,const Source *source) SOURCE_BLOCK_CALL {return descriptor(s,source,0,0);}
int16_t sources_reference(Sources *s,const Source *source) SOURCE_BLOCK_CALL {return descriptor(s,source,0,0);}
int16_t sources_include(Sources *s,const Source *source,uint8_t interruptible) SOURCE_BLOCK_CALL {
    Source view;uint16_t i,base;
    view=*source;
    if(s->text_tail==255){s->text_page=s->text_tail=page_new(PAGE_COMPILER);
        if(s->text_tail==255)return -1;buffer_init(s->text_tail);s->text_used=0;}
    if(s->text_used==PAGE_SIZE){uint8_t next=buffer_append(s->text_tail);
        if(next==255)return -1;s->text_tail=next;s->text_used=0;}
    view.page=s->text_tail;base=s->text_used;
    for(i=0;i<source->length;++i){
        if(interruptible&&!(i&255u)){uint8_t reason=key_interrupt();if(reason)return -(int16_t)(reason+1u);}
        if(s->text_used==PAGE_SIZE){uint8_t next=buffer_append(s->text_tail);if(next==255)return -1;s->text_tail=next;s->text_used=0;}
        far_set(s->text_tail,s->text_used++,buffer_get(source->page,i));
    }
    return descriptor(s,&view,base,0);
}
uint8_t sources_finish(Sources *s) SOURCE_BLOCK_CALL {
    uint8_t i;
    if(!buffer_drop(s->pending))return 0;s->pending=255;
    if(!sources_valid(s))return 0;
    for(i=1;i<s->count;++i)buffer_set(s->page,record(s,i),255);
    if(!buffer_drop(s->text_page))return 0;s->text_page=s->text_tail=255;s->text_used=0;return 1;
}
uint8_t sources_close(Sources *s) SOURCE_BLOCK_CALL {
    if(s->page==255)return 1;
    if(!sources_valid(s)||!buffer_drop(s->pending))return 0;
    s->pending=255;
    if(!buffer_drop(s->text_page))return 0;
    s->text_page=s->text_tail=255;
    if(!buffer_drop(s->page))return 0;s->page=255;return 1;
}
#ifdef ZP_UNITS_EXPERIMENT
uint8_t cc_poll(void);
uint32_t unit_source_fingerprint(const Source *source) SOURCE_BLOCK_CALL {
    uint32_t hash=2166136261UL;uint16_t i;
    for(i=0;i<source->length;++i) {
        if(!(i&63u)&&!cc_poll())return 0;
        hash=(hash^buffer_get(source->page,i))*16777619UL;
    }
    return hash;
}
uint8_t sources_release(const Sources *s,Source *loaded) SOURCE_BLOCK_CALL {
    source_close(loaded);
    if(loaded->page==255)return 1;
    /* Retired pages remain reachable through their existing buffer owner,
     * including when an OS free fails. The const descriptor itself is intact. */
    buffer_retire(s->page,loaded->page,s->used);loaded->page=255;return 0;
}
int16_t sources_unit(Sources *s,const Source *source,uint32_t checksum) SOURCE_BLOCK_CALL {
    int16_t id=descriptor(s,source,0,1);
    uint16_t at;uint8_t i;
    if(id<0)return -1;
    at=record(s,id);buffer_set(s->page,at+9,1);
    for(i=0;i<4;++i)buffer_set(s->page,at+10+i,(uint8_t)(checksum>>(8*i)));
    return id;
}
#endif
/* Build readiness depends on captured metadata. Unit source text is optional. */
uint8_t sources_ready(const Sources *s,uint8_t id) SOURCE_BLOCK_CALL {
    if(!sources_valid(s)||id>=s->count)return 0;
#ifdef ZP_UNITS_EXPERIMENT
    return buffer_get(s->page,record(s,id)+9)<=1;
#else
    return 1;
#endif
}
#ifdef ZP_UNITS_EXPERIMENT
uint8_t sources_matches(const Sources *s,uint8_t id,const Source *source) SOURCE_BLOCK_CALL {
    uint16_t at;uint32_t expected=0;uint8_t i;
    if(!sources_ready(s,id))return 0;
    at=record(s,id);
    if(!buffer_get(s->page,at+9))return 1;
    if(source->page==255||source->length!=word(s,at+1))return 0;
    for(i=0;i<4;++i)expected|=(uint32_t)buffer_get(s->page,at+10+i)<<(8*i);
    return unit_source_fingerprint(source)==expected;
}
#endif
uint8_t sources_byte(const Sources *s,uint8_t id,uint16_t offset) SOURCE_BLOCK_CALL {
    Source source;if(!sources_get(s,id,&source)||offset>=source.length)return 0;
    return buffer_view_get(source.page,sources_offset(s,id),offset);
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
