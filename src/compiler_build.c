#ifdef __SDCC
#define build_close compiler_build_close
#define build_check compiler_build_check
#define build_ready compiler_build_ready
#define build_locate compiler_build_locate
#define build_location compiler_build_location
#define image_get compiler_image_get
#endif
#include "compiler.h"
#include "image.h"

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
/* Final placement and relocation belong with build sealing. Keeping them in
 * this bank also leaves lexer/emission room for cancellable scanning. */
uint8_t cc_place_code(void) FRONTEND {
    uint16_t i,p,at,value,coefficient,origin;
    uint8_t byte;
    uint16_t count=cc.emitted-CC_CODE_BASE,delta;
    CompileLimits limits;
    if(!compile_limits(cc.image->globals_bytes,count,cc.sources->options,&limits)) { cc_fail(99);return 0; }
    origin=cc.sources->options&&cc.sources->options->start?cc.sources->options->start:limits.minimum_start;
    delta=origin-CC_CODE_BASE;
    for(i=0;i<cc.relocations;++i) {
        if(!(i&63)&&!cc_poll())return 0;
        p=i*RELOCATION_BYTES;
        at=far_get(cc.workspace,p)|((uint16_t)far_get(cc.workspace,p+1)<<8);
        byte=(at&0x8000u)!=0;at=CC_CODE_BASE+(at&0x7fffu);
        coefficient=far_get(cc.workspace,p+2)|((uint16_t)far_get(cc.workspace,p+3)<<8);
        if(at>=cc.emitted||(!byte&&at>=cc.emitted-1)) { cc_fail(99);return 0; }
        value=cc_code_get(at);
        if(!byte)value|=(uint16_t)cc_code_get(at+1)<<8;
        value+=(uint16_t)((uint32_t)coefficient*delta);
        if(byte)cc_put(at,(uint8_t)value);else cc_word(at,value);
    }
    /* A higher Start may overlap the first staging page in either direction.
     * Reverse only for an upward overlapping move. The second staging page
     * is separate from all final image pages. Clear gaps after staging dies. */
    for(i=0;i<count;++i) {
        if(!(i&255)&&!cc_poll())return 0;
        p=origin>0xc000u?count-1-i:i;
        cc_put(origin-0x4000u+p,cc_code_get(CC_CODE_BASE+p));
    }
    for(i=limits.minimum_start-0x4000u;i<origin-0x4000u;++i) {
        if(!(i&255)&&!cc_poll())return 0;
        cc_put(i,0);
    }
    /* Bind the private key dispatch only after final placement is known.
     * Both RAM and saved images own their typeahead state and exact target. */
    if(cc.keyboard_entry)
        cc_word(cc_runtime_private(RP_INPUT_KEY)-0x4000u+1,cc.keyboard_entry+delta);
    for(i=0;i<HOOK_COUNT;++i) if(cc.hook_variables[i])
        cc_word(cc.hook_variables[i]-0x4000u,cc.hook_defaults[i]+delta);
    cc_hooks_place(delta);
    if(cc.error->code) return 0;
    cc.image->code_origin=origin; cc.image->code_bytes=count;
    cc.image->length=origin-0x4000u+count;
    if(cc.sources->options&&cc.sources->options->kind==COMPILE_CHAIN)cc_activation_place();
    return !cc.error->code;
}

void cc_hash_image(const Build *b,uint8_t out[32]) ROUTINES {
    Sha256 hash; uint16_t i;
    uint8_t zero=build_is_chain(b)?20:36;
    sha_init(&hash);
    for(i=0;i<b->length;++i) {
        if(!(i&255)&&!cc_poll())return;
        sha_byte(&hash,i>=zero&&i<zero+32u?0:image_get(b,i));
    }
    sha_end(&hash,out);
}
void cc_hash_map(const Build *b,uint8_t out[32]) ROUTINES {
    Sha256 hash; uint16_t i;
    sha_init(&hash);
    for(i=0;i<b->map_length;++i) {
        if(!(i&255)&&!cc_poll())return;
        sha_byte(&hash,i>=20&&i<52?0:far_get(b->map_page,i));
    }
    sha_end(&hash,out);
}
static uint8_t image_layout(const Build *b) {
    uint8_t i;
    if(b->length<96||b->length>CC_STATIC_LIMIT-0x4000u||
       b->map_length<96||b->map_length>PAGE_SIZE||page_role(b->map_page)!=PAGE_MAP)return 0;
    for(i=0;i<3;++i) {
        if((uint16_t)i*PAGE_SIZE<b->length) {
            if(page_role(b->pages[i])!=PAGE_IMAGE)return 0;
        } else if(b->pages[i]!=255||b->versions[i])return 0;
    }
    return b->length>=PAGE_SIZE||build_is_chain(b);
}
uint8_t build_check(const Source *s,const Build *b) {
    uint8_t hash[32], expected[32], i;
    if(!b->valid||!image_layout(b)||b->revision!=s->revision||!cc_sources_ready(s,b)) return 0;
    source_hash(s,hash); if(memcmp(hash,b->source_hash,32)) return 0;
    cc_hash_image(b,hash); if(memcmp(hash,b->id,32)||!build_image_identity(b)) return 0;
    cc_hash_map(b,hash);
    for(i=0;i<32;++i) expected[i]=image_get(b,(build_is_chain(b)?52:100)+i);
    return !memcmp(hash,expected,32);
}
uint8_t build_ready(const Source *s,const Build *b) {
    uint8_t i;
    Source snapshot;
    if(!b->valid||!image_layout(b)||b->revision!=s->revision||!cc_sources_ready(s,b)) return 0;
    if(!sources_get(&b->sources,0,&snapshot))return 0;
    if(s->page!=snapshot.page&&s->page!=sources_origin(&b->sources,0))return build_check(s,b);
    /* Compiler-sealed pages are private and immutable. RAM execution copies
     * them. The single far-store core detects every subsequent modification;
     * changed versions require the full cryptographic check again. */
    for(i=0;i<3;++i) if(b->pages[i]!=255&&b->versions[i]!=page_versions[b->pages[i]]) return build_check(s,b);
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
