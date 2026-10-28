#ifdef __SDCC
#define build_close compiler_build_close
#define build_check compiler_build_check
#define build_ready compiler_build_ready
#define build_locate compiler_build_locate
#define build_location compiler_build_location
#define image_get compiler_image_get
#endif
#include "compiler.h"

/* Build ownership, sealing and source lookup share a code bank. Compiler
 * emission reaches these through banked helpers or the resident public API. */
uint8_t image_get(const Build *b,uint16_t p) { return far_get(b->pages[p/PAGE_SIZE],p%PAGE_SIZE); }

void build_close(Build *b) {
    uint8_t i;
    b->valid=0;
    for(i=0;i<3;++i) if(b->pages[i]!=255&&page_drop(b->pages[i])) b->pages[i]=255;
    if(b->map_page!=255&&page_drop(b->map_page)) b->map_page=255;
    sources_close(&b->sources);
}
void cc_hash_image(const Build *b,uint8_t out[32]) ROUTINES {
    Sha256 hash; uint16_t i;
    sha_init(&hash);
    for(i=0;i<b->length;++i) sha_byte(&hash,i>=36&&i<68?0:image_get(b,i));
    sha_end(&hash,out);
}
void cc_hash_map(const Build *b,uint8_t out[32]) ROUTINES {
    Sha256 hash; uint16_t i;
    sha_init(&hash);
    for(i=0;i<b->map_length;++i) sha_byte(&hash,i>=20&&i<52?0:far_get(b->map_page,i));
    sha_end(&hash,out);
}
uint8_t build_check(const Source *s,const Build *b) {
    uint8_t hash[32], expected[32], i;
    if(!b->valid||b->revision!=s->revision||!cc_sources_ready(s,b)) return 0;
    source_hash(s,hash); if(memcmp(hash,b->source_hash,32)) return 0;
    cc_hash_image(b,hash); if(memcmp(hash,b->id,32)) return 0;
    cc_hash_map(b,hash);
    for(i=0;i<32;++i) expected[i]=image_get(b,100+i);
    return !memcmp(hash,expected,32);
}
uint8_t build_ready(const Source *s,const Build *b) {
    uint8_t i;
    Source snapshot;
    if(!b->valid||b->revision!=s->revision||!cc_sources_ready(s,b)) return 0;
    if(!sources_get(&b->sources,0,&snapshot))return 0;
    if(s->page!=snapshot.page&&s->page!=sources_origin(&b->sources,0))return build_check(s,b);
    /* Compiler-sealed pages are private and immutable. RAM execution copies
     * them. The single far-store core detects every subsequent modification;
     * changed versions require the full cryptographic check again. */
    for(i=0;i<3;++i) if(b->versions[i]!=page_versions[b->pages[i]]) return build_check(s,b);
    if(b->versions[3]!=page_versions[b->map_page]||b->versions[4]!=page_versions[s->page]) return build_check(s,b);
    return 1;
}
static uint16_t read16(uint8_t p,uint16_t offset) { return far_get(p,offset)|((uint16_t)far_get(p,offset+1)<<8); }
uint8_t build_locate(const Source *s,const Build *b,uint16_t pc,uint16_t *position) {
    uint8_t file;uint16_t canonical;
    if(!build_location(s,b,pc,&file,&canonical)||file)return 0;
    *position=source_editor_offset(s,canonical);return 1;
}
uint8_t build_location(const Source *s,const Build *b,uint16_t pc,uint8_t *file,uint16_t *position) {
    uint16_t i, start, end, n;
    Source source;
    if(!build_ready(s,b)||pc>=b->code_bytes) return 0;
    n=read16(b->map_page,84);
    for(i=96+n;i+16<=b->map_length;i+=16) {
        start=read16(b->map_page,i); end=read16(b->map_page,i+2);
        if(pc>=start&&pc<end) {
            start=read16(b->map_page,i+8);
            n=read16(b->map_page,i+4);
            if(n>=b->sources.count||read16(b->map_page,i+6)||read16(b->map_page,i+10)||
               read16(b->map_page,i+14)||!sources_get(&b->sources,(uint8_t)n,&source)||
               start>source.length||read16(b->map_page,i+12)>source.length)return 0;
            *file=(uint8_t)n;*position=start;return 1;
        }
    }
    return 0;
}

/* Error provenance is captured while invocation-owned compiler inputs live. */
void cc_sources_error(void) ROUTINES {
    Source failed;const Source *original=0;int16_t id;
    cc.literal_length=0;
    if(cc.sources->external_error){strcpy(cc.name,cc.input->path);return;}
    if(!sources_get(&cc.image->sources,cc.sources->error_file,&failed)) {
        strcpy(cc.name,cc.sources->root->path);return;
    }
    strcpy(cc.name,failed.path);
    if(sources_hash(&cc.image->sources,cc.sources->error_file,cc.literal))cc.literal_length=32;
    if(!cc.sources->error_file)original=cc.sources->root;
    else if(cc.sources->buffers) {
        id=sources_find(cc.sources->buffers,failed.path);
        if(id>=0&&sources_get(cc.sources->buffers,(uint8_t)id,&failed))original=&failed;
    }
    if(original) {
        cc.error->start=source_editor_offset(original,cc.error->start);
        cc.error->end=source_editor_offset(original,cc.error->end);
    }
}
uint8_t cc_sources_ready(const Source *root,const Build *build) ROUTINES {
    uint8_t id,hash[32],expected[32];Source source;
    Diagnostic diagnostic;
    if(!page_lifetime(root->page)||!sources_get(&build->sources,0,&source)||strcmp(root->path,source.path))return 0;
    for(id=0;id<build->sources.count;++id) {
        if(!sources_ready(&build->sources,id))return 0;
        if(sources_resident(&build->sources,id))continue;
        if(!sources_get(&build->sources,id,&source))return 0;
        if(!read_block(&source,source.path,&diagnostic))return 0;
        source_hash(&source,hash);if(!page_drop(source.page))return 0;
        if(!sources_hash(&build->sources,id,expected)||memcmp(hash,expected,32))return 0;
    }
    return 1;
}
