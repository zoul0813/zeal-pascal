#ifdef __SDCC
#define build_close compiler_build_close
#define build_check compiler_build_check
#define build_ready compiler_build_ready
#define build_locate compiler_build_locate
#define build_location compiler_build_location
#define image_get compiler_image_get
#endif
#include "compiler.h"
#include "compiler_disk.h"
#include "image.h"
#include "sources_internal.h"

/* Build ownership, sealing and source lookup share a code bank. Compiler
 * emission reaches these through banked helpers or the resident public API. */
uint8_t image_get(const Build *b,uint16_t p) { return cc_disk_active(b)?cc_disk_image(b,p):far_get(b->pages[p/PAGE_SIZE],p%PAGE_SIZE); }

void build_discard_image(Build *b) SOURCE_BLOCK_CALL {
    uint8_t i;
    cc_disk_discard(b);b->valid=0;
    for(i=0;i<3;++i) if(b->pages[i]!=255&&page_drop(b->pages[i])) b->pages[i]=255;
    if(b->map_page!=255&&buffer_drop(b->map_page)) b->map_page=255;
}
void build_close(Build *b) {
    build_discard_image(b);sources_close(&b->sources);
    if(b==cc.image)cc_tables_end();
}
uint8_t cc_image_page(uint8_t index) FRONTEND {
    uint16_t i;
    if(cc.image->pages[index]!=255)return 1;
    cc.image->pages[index]=page_new(PAGE_IMAGE);
    if(cc.image->pages[index]==255) {cc_fail(99);return 0;}
    for(i=0;i<PAGE_SIZE;i+=256) {
        if(!cc_poll())return 0;
        if(!far_fill(cc.image->pages[index],i,0,256)){cc_fail(99);return 0;}
    }
    page_initialized(cc.image->pages[index],PAGE_SIZE);return 1;
}
static uint16_t stage_offset(uint16_t offset) {
    if(cc.sources->stage_shared)offset+=(uint16_t)cc.image->sources.count*RECORD_BYTES;
    if(offset>=PAGE_SIZE){cc_fail(99);return PAGE_SIZE;}
    return offset;
}
void cc_stage_put(uint16_t offset,uint8_t value) FRONTEND {
    offset=stage_offset(offset);if(cc.error->code)return;
    if(cc.stage_page==255) {
        cc.stage_page=page_new(PAGE_COMPILER);
        if(cc.stage_page==255){cc_fail(99);return;}
    }
    far_set(cc.stage_page,offset,value);
    if(cc.sources->stage_shared)cc.image->sources.version=page_versions[cc.stage_page];
}
uint8_t cc_stage_get(uint16_t offset) FRONTEND {
    offset=stage_offset(offset);
    return cc.error->code?0:far_get(cc.stage_page,offset);
}
/* Once globals cross a bank, their storage must not alias parser staging.
 * Spill code only when necessary, then clear the old staging bank before the
 * first high-global initializer or final placement uses it as data. */
static uint8_t separate_globals(void) {
    uint16_t i,n;
    if(cc.primary_stage!=255)return 1;
    cc.primary_stage=page_new(PAGE_COMPILER);
    if(cc.primary_stage==255){cc_fail(99);return 0;}
    n=cc.emitted>CC_CODE_BASE?cc.emitted-CC_CODE_BASE:0;if(n>PAGE_SIZE)n=PAGE_SIZE;
    for(i=0;i<n;++i) {
        if(!(i&255)&&!cc_poll())return 0;
        far_set(cc.primary_stage,i,far_get(cc.image->pages[2],i));
    }
    for(i=0;i<PAGE_SIZE;i+=256) {
        if(!cc_poll()||!far_fill(cc.image->pages[2],i,0,256))return 0;
    }
    return 1;
}
void cc_global_put(uint16_t p,uint8_t value) FRONTEND {
    if(cc.error->code)return;
    if(cc_disk_active(cc.image)){cc_disk_global_put(p,value);return;}
    if(!cc.placing&&p>=CC_CODE_BASE&&!separate_globals())return;
    if(!cc_image_page(p/PAGE_SIZE))return;
    far_set(cc.image->pages[p/PAGE_SIZE],p%PAGE_SIZE,value);
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
        at=cc_reloc_get(p)|((uint16_t)cc_reloc_get(p+1)<<8);
        byte=(at&0x8000u)!=0;at=CC_CODE_BASE+(at&0x7fffu);
        coefficient=cc_reloc_get(p+2)|((uint16_t)cc_reloc_get(p+3)<<8);
        if(at>=cc.emitted||(!byte&&at>=cc.emitted-1)) { cc_fail(99);return 0; }
        value=cc_code_get(at);
        if(!byte)value|=(uint16_t)cc_code_get(at+1)<<8;
        value+=(uint16_t)((uint32_t)coefficient*delta);
        if(byte)cc_put(at,(uint8_t)value);else cc_word(at,value);
    }
    /* Relocation and routine records are dead before final placement. Reuse
     * their capacity for the middle image page rather than holding both. */
    if(cc.workspace!=255&&!page_drop(cc.workspace)) {cc_fail(99);return 0;}
    cc.workspace=255;
    if(!cc_disk_active(cc.image)) {
    if(cc.image->globals_bytes>PAGE_SIZE&&!separate_globals())return 0;
    if(!cc_image_page(1))return 0;
    cc.placing=1;
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
    }
    cc.image->code_origin=origin;cc.image->code_bytes=count;
    if(cc_disk_active(cc.image))cc_disk_place(cc.image,origin,count);
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

static uint8_t image_layout(const Build *b) {
    uint8_t i;
    if(b->length<96||b->length>CC_STATIC_LIMIT-0x4000u||
       b->map_length<96||b->map_length>SOURCE_LIMIT||page_role(b->map_page)!=PAGE_MAP)return 0;
    if(b->valid==2)return cc_disk_ready(b)&&b->pages[0]==255&&b->pages[1]==255&&b->pages[2]==255;
    for(i=0;i<3;++i) {
        if((uint16_t)i*PAGE_SIZE<b->length) {
            if(page_role(b->pages[i])!=PAGE_IMAGE)return 0;
        } else if(b->pages[i]!=255||b->versions[i])return 0;
    }
    return b->length>=PAGE_SIZE||build_is_chain(b);
}
uint8_t build_check(const Source *s,const Build *b) {
    uint8_t i;Source snapshot;
    if(!b->valid||!image_layout(b)||b->revision!=s->revision||!cc_sources_ready(s,b)||
       !sources_get(&b->sources,0,&snapshot)||strcmp(s->path,snapshot.path))return 0;
    for(i=0;i<3;++i)if(b->pages[i]!=255&&b->versions[i]!=page_versions[b->pages[i]])return 0;
    if(b->versions[3]!=page_versions[b->map_page])return 0;
    if(s->page!=snapshot.page&&s->page!=sources_origin(&b->sources,0))return 0;
    return b->versions[4]==page_versions[s->page];
}
uint8_t build_ready(const Source *s,const Build *b) {return build_check(s,b);}
static uint16_t read16(uint8_t p,uint16_t offset) { return buffer_get(p,offset)|((uint16_t)buffer_get(p,offset+1)<<8); }
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
    uint8_t id;uint8_t count=build->sources.count;Source source;
    if(!page_lifetime(root->page)||!sources_get(&build->sources,0,&source)||strcmp(root->path,source.path))return 0;
    for(id=0;id<count;++id)if(!sources_ready(&build->sources,id))return 0;
    return 1;
}

void cc_storage(uint16_t id,uint16_t width) FRONTEND {
    if((uint32_t)GLOBAL_BASE+cc.image->globals_bytes+width>=CC_STATIC_LIMIT) { cc_fail(98); return; }
    CCS_SET(id,address,GLOBAL_BASE+cc.image->globals_bytes);
    cc.image->globals_bytes+=width;
}
