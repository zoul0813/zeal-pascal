#include "compiler.h"
#include "compiler_disk.h"
#include "transfer_asset.h"

const char *compile_error_path(void) SOURCE_BLOCK_CALL { return cc.name; }
static void map8(uint16_t p,uint8_t v) { buffer_set(cc.image->map_page,p,v); }

static void map16(uint16_t p,uint16_t v) { map8(p,(uint8_t)v); map8(p+1,(uint8_t)(v>>8)); }

static void map32(uint16_t p,uint32_t v) { uint8_t i; for(i=0;i<4;++i) map8(p+i,(uint8_t)(v>>(i*8))); }


void cc_mapping(uint16_t begin,uint16_t end,SourcePosition first,SourcePosition last) BACKEND {
    uint32_t wide=96UL+cc.file_bytes+(uint32_t)cc.entries*16;
    uint16_t offset;
    uint16_t file=(uint16_t)(first>>16),start=(uint16_t)first,finish=(uint16_t)last;
    Source source;
    if(begin==end||cc.error->code) return;
    if(wide>SOURCE_LIMIT-16||!buffer_reserve(cc.image->map_page,(uint16_t)(wide+16))) { cc_fail(99); return; }
    offset=(uint16_t)wide;
    if(!sources_get(&cc.image->sources,(uint8_t)file,&source)||start>source.length){cc_fail(99);return;}
    if((last>>16)!=file)finish=start<source.length?start+1:start;
    if(finish<start||finish>source.length){cc_fail(99);return;}
    map16(offset,begin);map16(offset+2,end);map16(offset+4,file);map16(offset+6,0);
    map32(offset+8,start);map32(offset+12,finish);++cc.entries;
}
void cc_map_source(uint16_t bytes) BACKEND {
    uint32_t wide=96UL+cc.file_bytes+(uint32_t)cc.entries*16+bytes;
    uint16_t i,first=96+cc.file_bytes,end;
    if(wide>SOURCE_LIMIT||!buffer_reserve(cc.image->map_page,(uint16_t)wide)){cc_fail(99);return;}
    end=(uint16_t)(wide-bytes);
    i=end;while(i>first){--i;map8(i+bytes,buffer_get(cc.image->map_page,i));}
    cc.file_bytes+=bytes;
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
    uint16_t i,j,at;uint8_t file;Source active;
    b->revision=s->revision;
    for(i=0;i<4;++i)map8(i,"ZMAP"[i]);
    map8(4,2);map16(6,96);map16(8,1);map16(10,b->code_origin);map16(12,b->code_bytes);
    map16(14,b->sources.count);map32(16,cc.entries);map32(84,cc.file_bytes);map32(88,(uint32_t)cc.entries*16);
    at=96;
    for(file=0;file<b->sources.count;++file) {
        if(!sources_get(&b->sources,file,&active)){cc_fail(99);return 0;}
        j=(uint16_t)strlen(active.path);
        map16(at,file);map16(at+2,j);map32(at+4,active.length);
        for(i=0;i<j;++i)map8(at+8+i,(uint8_t)active.path[i]);
        at+=8+j;
    }
    if(at!=96+cc.file_bytes){cc_fail(99);return 0;}
    b->map_length=at+cc.entries*16;buffer_initialized(b->map_page,b->map_length);
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
    if(!sources_pack(&b->sources)){d->code=99;return 0;}
    for(i=0;i<3;++i)b->versions[i]=b->pages[i]==255?0:page_versions[b->pages[i]];
    b->versions[3]=page_versions[b->map_page];b->versions[4]=page_versions[s->page];
    if(cc_disk_active(b)) {cc_disk_seal(b);if(d->code)return 0;b->valid=2;}else b->valid=1;
    cc.polling=0;return 1;
}
