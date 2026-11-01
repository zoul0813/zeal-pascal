#include "compiler_job.h"
#include "image.h"
#include "saved_map.h"
#include "sources_internal.h"

static uint16_t word(const uint8_t *p) {return p[0]|((uint16_t)p[1]<<8);}
static uint32_t longword(const uint8_t *p) {return (uint32_t)word(p)|((uint32_t)word(p+2)<<16);}
static uint8_t zero(const uint8_t *p,uint16_t n) {while(n--)if(*p++)return 0;return 1;}

static void take(uint8_t page,uint16_t at,uint8_t *out,uint16_t n) {while(n--)*out++=far_get(page,at++);}
static uint16_t far_word(uint8_t page,uint16_t at) {return far_get(page,at)|((uint16_t)far_get(page,at+1)<<8);}
static uint32_t far_long(uint8_t page,uint16_t at) {return (uint32_t)far_word(page,at)|((uint32_t)far_word(page,at+2)<<16);}
static uint8_t map_source(const Build *b,uint16_t at,Source *s,uint8_t hash[32]) {
    char canonical[128];uint16_t n=far_word(b->map_page,at+2);
    if(!n||n>127||(uint32_t)at+40+n>b->map_length||far_long(b->map_page,at+4)>SOURCE_LIMIT)return 0;
    memset(s,0,sizeof(*s));s->page=255;s->length=(uint16_t)far_long(b->map_page,at+4);
    take(b->map_page,at+8,hash,32);take(b->map_page,at+40,(uint8_t*)s->path,n);s->path[n]=0;
    return strlen(s->path)==n&&sources_canonical(s->path,canonical)&&!strcmp(s->path,canonical);
}
static uint8_t validate_sources(const Build *b,const NativeImage *image,const Sources *inputs,uint16_t count) {
    uint8_t h[96],digest[32],hash[32];uint16_t at=96,i,n,file;
    uint32_t table,entries;Source s,prior,input;int16_t id;
    take(b->map_page,0,h,96);table=longword(h+84);entries=longword(h+16);
    if(memcmp(h,"ZMAP",4)||h[4]!=1||h[5]||word(h+6)!=96||word(h+8)!=1||
       word(h+10)!=image->origin||word(h+12)!=image->bytes||!count||count>SOURCE_COUNT||word(h+14)!=count||
       memcmp(h+20,image->id,32)||(!image->chain&&memcmp(h+52,image->manifest,32))||
       !zero(h+92,4)||table>b->map_length-96u||entries>image->bytes||longword(h+88)!=entries*16||
       96UL+table+entries*16!=b->map_length)return 0;
    /* cc_seal writes dense source IDs in catalogue order. Bind every record
     * to that catalogue; the final build_check authenticates the whole map,
     * including this manifest and the PC entries, before result publication. */
    for(i=0;i<count;++i) {
        if((uint32_t)at+40>96+table)return 0;
        file=far_word(b->map_page,at);n=far_word(b->map_page,at+2);
        if(file!=i||!map_source(b,at,&s,hash)||(uint32_t)at+40+n>96+table)return 0;
        if(!file) {
            if(!sources_get(inputs,0,&input)||strcmp(s.path,input.path))return 0;
        }
        id=sources_find(inputs,s.path);
        if(id>=0) {
            if(!sources_get(inputs,(uint8_t)id,&input))return 0;
            if(input.page!=255&&(!sources_hash(inputs,(uint8_t)id,digest)||
               s.length!=input.length||memcmp(hash,digest,32)))return 0;
        }
        if(sources_find(&b->sources,s.path)!=file||
           !sources_ready(&b->sources,(uint8_t)file)||!sources_get(&b->sources,(uint8_t)file,&prior)||
           strcmp(prior.path,s.path)||prior.length!=s.length||
           !sources_hash(&b->sources,(uint8_t)file,digest)||memcmp(hash,digest,32))return 0;
        if(prior.page==255||(!compiler_page_owned(compiler_owned,prior.page)&&
           !compiler_page_owned(compiler_parent_owned,prior.page)&&
           (id<0||prior.page!=input.page||!sources_borrowed(&b->sources,(uint8_t)file))))return 0;
        at+=40+n;
    }
    return at==96+table;
}
/* The producer and consumer share one synchronous invocation. Bounds,
 * ownership and sealed versions protect the direct result before any page is
 * adopted. Disk/source freshness and full Build identity remain checked by
 * retain_sources/build_check; external image/map readers are unchanged.
 * This check alone does not authorize publication: immutable versions protect
 * the handoff, and build_check must still verify the sealed image/map digests. */
uint8_t compiler_result_ready(const Sources *inputs,const CompileOptions *options,uint8_t mode,const Build *b) SOURCE_BLOCK_CALL {
    uint8_t i,j,count,h[160],kind=mode==2,files=b->sources.count;NativeImage identity;Source root;char paths[2][128];
    if(!sources_valid(&b->sources)||b->sources.base||!compiler_page_owned(compiler_owned,b->sources.page))return 0;
    if(!b->valid||!files||files>SOURCE_COUNT||b->length<96||b->length>48128u||
       b->map_length<96||b->map_length>PAGE_SIZE||b->map_page>=64||
       !compiler_page_owned(compiler_owned,b->map_page)||page_role(b->map_page)!=PAGE_MAP||
       b->versions[3]!=page_versions[b->map_page])return 0;
    count=(uint8_t)((b->length+PAGE_SIZE-1u)/PAGE_SIZE);
    for(i=0;i<3;++i) {
        if(i>=count) {if(b->pages[i]!=255||b->versions[i])return 0;continue;}
        if(b->pages[i]>=64||b->pages[i]==b->map_page||!compiler_page_owned(compiler_owned,b->pages[i])||
           page_role(b->pages[i])!=PAGE_IMAGE||b->versions[i]!=page_versions[b->pages[i]])return 0;
        for(j=0;j<i;++j)if(b->pages[j]==b->pages[i])return 0;
    }
    take(b->pages[0],0,h,kind?96:160);
    if((!kind&&word(h+18))||native_header(h,kind,&identity)||identity.length!=b->length||
       memcmp(identity.id,b->id,32)||identity.origin!=b->code_origin||identity.bytes!=b->code_bytes||
       word(h+(kind?16:26))!=b->globals_bytes||word(h+(kind?18:30))!=options->end||
       (options->start&&identity.origin!=options->start))return 0;
    if(!sources_get(inputs,0,&root)||!saved_paths_kind(root.path,paths,kind))return 0;
    if(mode&&(sources_find(inputs,paths[0])>=0||sources_find(inputs,paths[1])>=0))return 0;
    return validate_sources(b,&identity,inputs,files);
}
