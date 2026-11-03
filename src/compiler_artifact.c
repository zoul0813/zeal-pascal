#include "compiler.h"
#include "compiler_disk.h"
#include "transfer_asset.h"

const char *compile_error_path(void) SOURCE_BLOCK_CALL { return cc.name; }
static void map8(uint16_t p,uint8_t v) { buffer_set(cc.image->map_page,p,v); }

static void map16(uint16_t p,uint16_t v) { map8(p,(uint8_t)v); map8(p+1,(uint8_t)(v>>8)); }

/* Private statement locations: begin:u16, end:u16, file:u16,
 * source offset:u16. File names already belong to Build.sources. */
void cc_mapping(uint16_t begin,uint16_t end,SourcePosition first,SourcePosition last) BACKEND {
    uint32_t wide=(uint32_t)cc.entries*MAP_RECORD_BYTES;
    uint16_t offset;uint8_t file=(uint8_t)(first>>16);Source source;
    (void)last;
    if(begin==end||cc.error->code)return;
    if(wide>SOURCE_LIMIT-MAP_RECORD_BYTES||
       !buffer_reserve(cc.image->map_page,(uint16_t)(wide+MAP_RECORD_BYTES))) {cc_fail(99);return;}
    if(!sources_get(&cc.image->sources,file,&source)||(uint16_t)first>source.length){cc_fail(99);return;}
    offset=(uint16_t)wide;
    map16(offset,begin);map16(offset+2,end);map16(offset+4,file);
    map16(offset+6,(uint16_t)first);++cc.entries;
}

static void chain_image(void) {
    uint16_t i,from=cc.image->code_origin-0x4000u,count=cc.image->code_bytes;
    /* Destination precedes source, so a forward move is safe even when the
     * retained page vectors overlap. Ordinary globals and prefix are omitted. */
    if(!cc_disk_active(cc.image))for(i=0;i<count;++i) {
        if(!(i&255)&&!cc_poll())return;
        cc_put(96+i,image_get(cc.image,from+i));
    }
    for(i=0;i<96;++i)cc_put(i,0);
    for(i=0;i<4;++i)cc_put(i,(uint8_t)"ZCHN"[i]);
    cc_put(4,2);cc_word(6,96);cc_word(8,1);cc_word(10,cc.image->code_origin);
    cc_word(12,count);cc_word(14,GLOBAL_BASE);cc_word(16,cc.image->globals_bytes);
    cc_word(18,cc.sources->options->end);
    cc.image->length=96+count;
}
uint8_t cc_seal(const Source *s,const CompileOptions *options) FRONTEND {
    Build *b=cc.image;Diagnostic *d=cc.error;
    uint16_t i,at;
    b->revision=s->revision;
    b->map_length=cc.entries*MAP_RECORD_BYTES;
    buffer_initialized(b->map_page,b->map_length);
    cc_word(10,b->length);cc_word(20,b->code_origin);cc_word(22,b->code_bytes);
    cc_word(24,GLOBAL_BASE);cc_word(26,b->globals_bytes);cc_word(28,0x4000u+b->length);
    cc_word(30,options?options->end:0xfbffu);
    if(options&&options->kind==COMPILE_CHAIN)chain_image();
    else {
        at=0x4000u+b->length-EXECUTE_ENTRY_BYTES;
        cc_word(1,at);cc_word(12,at);
    }
    if(d->code)return 0;
    if(!cc_poll())return 0;
    /* Page2 also served as code staging. Once placed, a short image owns no
     * bytes there; retaining it can prevent a later run with parked includes. */
    for(i=0;i<3;++i)if(b->pages[i]!=255&&(uint32_t)i*PAGE_SIZE>=b->length) {
        if(!page_drop(b->pages[i])) { d->code=99;return 0; }
        b->pages[i]=255;
    }
    for(i=0;i<3;++i)b->versions[i]=b->pages[i]==255?0:page_versions[b->pages[i]];
    b->versions[3]=page_versions[b->map_page];b->versions[4]=page_versions[s->page];
    if(cc_disk_active(b)) {cc_disk_seal(b);if(d->code)return 0;b->valid=2;}else b->valid=1;
    cc.polling=0;return 1;
}
