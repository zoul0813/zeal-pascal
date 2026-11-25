#ifdef __SDCC
#define build_close compiler_build_close
#define build_check compiler_build_check
#define build_ready compiler_build_ready
#define build_locate compiler_build_locate
#define build_location compiler_build_location
#define image_get compiler_image_get
#endif
#include "compiler/internal.h"
#include "compiler/link/link.h"
#include "compiler/link/layout.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "platform/banks/loader.h"
#endif
#include "compiler/disk.h"
#include "compiler/groups/native.h"
#include "program/image.h"

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
#ifdef ZP_UNITS_EXPERIMENT
    /* The prefix is materialized on first mutation, or after unit linking.
     * Preserve template initialization before any emitted patch can see it. */
    if(!index&&!(cc.sources->options&&cc.sources->options->kind==COMPILE_CHAIN)&&
       !cc_runtime_copy(cc.image->pages[index])){cc_fail(99);return 0;}
#endif
    page_initialized(cc.image->pages[index],PAGE_SIZE);return 1;
}
static uint16_t stage_offset(uint16_t offset) {
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
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.sources->unit_mode){cc_fail(249);return;}
#endif
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
    uint16_t count=cc.emitted-CC_CODE_BASE,delta,data=cc_data_base();
    CompileLimits limits;
    if(!compile_limits_at(data,cc.image->globals_bytes,count,cc.sources->options,&limits)) { cc_fail(99);return 0; }
    origin=cc.sources->options&&cc.sources->options->start?cc.sources->options->start:limits.minimum_start;
    delta=origin-CC_CODE_BASE;
    if(!cc_disk_active(cc.image)) {
        if(cc.image->globals_bytes>PAGE_SIZE&&!separate_globals())return 0;
        if(cc.image->globals_bytes&&!cc_image_page(1))return 0;
    }
    #ifdef ZP_OVERLAYS
    if(cc.sources->overlays&&!cc_overlay_place(origin,data))return 0;
    #endif
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
#ifdef ZP_UNITS_EXPERIMENT
    if(cc.links||cc_layout_active()) {
        uint8_t ok,primary;
        if(!code_phase_unit_finish_begin()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
        ok=cc_link_apply(origin,data)&&cc_layout_finish();primary=platform_error;
        if(!code_phase_unit_finish_end()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
        if(!ok){platform_error=primary;return 0;}
    }
#endif
    if(!cc_disk_active(cc.image)) {
    if(cc.image->globals_bytes>PAGE_SIZE&&!separate_globals())return 0;
    if(!cc_layout_active()&&!cc_image_page(1))return 0;
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
    if(cc.error->code)return 0;
    /* Bind the private key dispatch only after final placement is known.
     * Both RAM and saved images own their typeahead state and exact target. */
    if(cc.keyboard_entry)
        cc_word(cc_runtime_address(cc_runtime_private(RP_INPUT_KEY))-0x4000u+1,cc.keyboard_entry+delta);
    for(i=0;i<HOOK_COUNT;++i) if(cc.hook_variables[i])
        cc_word(data+cc.hook_variables[i]-GLOBAL_BASE-0x4000u,cc.hook_defaults[i]+delta);
#ifdef ZP_PROGRAM_PAGES
    if(cc.shared_stack) {
        /* Redirect the existing runtime dispatch, preserving all program data
         * and explicit native addresses in the emitted code. */
        at=cc_runtime_address(cc_runtime_private(RP_PROGRAM_ENTER))-0x4000u;
        cc_put(at,0x21);cc_word(at+1,cc.page_entry+delta);
    }
#endif
    cc_hooks_place(delta);
#ifdef ZP_OVERLAYS
    if(cc.sources->overlays) {
        /* Cleanup entry is the second reserved root JP, following the entry
         * skip and dispatch JP. Root relocation has already placed it. */
        cc_word(RT_FILECLEANUP-0x4000u+1,cc.image->code_origin+6u);
    }
#endif
    if(cc.error->code) return 0;
    cc.image->code_origin=origin; cc.image->code_bytes=count;
    cc.image->length=origin-0x4000u+count;
    if(cc.sources->options&&cc.sources->options->kind==COMPILE_CHAIN)cc_activation_place();
    return !cc.error->code;
}

static uint8_t image_layout(const Build *b) {
    uint8_t i;
    if(b->length<96||b->length>CC_STATIC_LIMIT-0x4000u||
       b->map_length<MAP_RECORD_BYTES||b->map_length>SOURCE_LIMIT||page_role(b->map_page)!=PAGE_MAP)return 0;
    if(b->valid==2)return cc_disk_ready(b)&&b->pages[0]==255&&b->pages[1]==255&&b->pages[2]==255;
    for(i=0;i<3;++i) {
        if((uint16_t)i*PAGE_SIZE<b->length) {
            if(page_role(b->pages[i])!=PAGE_IMAGE)return 0;
        } else if(b->pages[i]!=255&&page_role(b->pages[i])!=PAGE_IMAGE)return 0;
    }
    return b->length>=160||build_is_chain(b);
}
uint8_t build_check(const Source *s,const Build *b) {
    uint8_t i;Source snapshot;
    if(!b->valid||!image_layout(b)||b->revision!=s->revision||!cc_sources_ready(s,b)||
       !sources_get(&b->sources,0,&snapshot)||strcmp(s->path,snapshot.path))return 0;
    for(i=0;i<3;++i)if(b->pages[i]!=255&&b->versions[i]!=page_versions[b->pages[i]])return 0;
    if(b->versions[3]!=page_versions[b->map_page])return 0;
    if(s->page!=snapshot.page)return 0;
    return s->page==255||b->versions[4]==page_versions[s->page];
}
uint8_t build_ready(const Source *s,const Build *b) {return build_check(s,b);}
static uint16_t read16(uint8_t p,uint16_t offset) { return buffer_get(p,offset)|((uint16_t)buffer_get(p,offset+1)<<8); }
uint8_t build_locate(const Source *s,const Build *b,uint16_t pc,uint16_t *position) {
    uint8_t file;uint16_t canonical;
    if(!build_location(s,b,pc,&file,&canonical)||file)return 0;
    *position=source_editor_offset(s,canonical);return 1;
}
#ifdef ZP_UNITS_EXPERIMENT
/* Resolve optional unit text only for navigation. A normal captured input
 * already matches, while unit metadata has no text page of its own. */
static uint8_t map_source_ready(const Sources *sources,uint8_t file,const Source *expected) {
    Source loaded;
    Diagnostic diagnostic;
    uint8_t matches;

    if(sources_matches(sources,file,expected))return 1;
    if(!read_block(&loaded,expected->path,&diagnostic)) {
        sources_release(sources,&loaded);
        return 0;
    }
    matches=sources_matches(sources,file,&loaded);
    if(!sources_release(sources,&loaded))return 0;
    return matches;
}
#endif
uint8_t build_location(const Source *s,const Build *b,uint16_t pc,uint8_t *file,uint16_t *position) {
    uint16_t i, start, end, n;
    Source source;
    if(!build_ready(s,b))return 0;
#ifdef ZP_OVERLAYS
    if(overlay_build_owned&&cc_overlay_locate(b,pc,file,position))return 1;
#endif
    if(pc>=b->code_bytes)return 0;
    for(i=0;i<=b->map_length-MAP_RECORD_BYTES;i+=MAP_RECORD_BYTES) {
        start=read16(b->map_page,i); end=read16(b->map_page,i+2);
        if(pc>=start&&pc<end) {
            start=read16(b->map_page,i+6);
            n=read16(b->map_page,i+4);
            if(n>=b->sources.count||!sources_get(&b->sources,(uint8_t)n,&source)||
               start>source.length)return 0;
#ifdef ZP_UNITS_EXPERIMENT
            if(!map_source_ready(&b->sources,(uint8_t)n,&source))return 0;
#endif
            *file=(uint8_t)n;*position=start;return 1;
        }
    }
    return 0;
}

uint8_t cc_sources_ready(const Source *root,const Build *build) ROUTINES {
    uint8_t id;uint8_t count=build->sources.count;Source source;
    if((root->page!=255&&!page_lifetime(root->page))||!sources_get(&build->sources,0,&source)||strcmp(root->path,source.path))return 0;
    for(id=0;id<count;++id)if(!sources_ready(&build->sources,id))return 0;
    return 1;
}

void cc_storage(uint16_t id,uint16_t width) FRONTEND {
    if((uint32_t)GLOBAL_BASE+cc.image->globals_bytes+width>=CC_STATIC_LIMIT) { cc_fail(98); return; }
    CCS_SET(id,address,GLOBAL_BASE+cc.image->globals_bytes);
    cc.image->globals_bytes+=width;
}
