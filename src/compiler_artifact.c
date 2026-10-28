#include "compiler.h"

const char *compile_error_path(void) SOURCE_BLOCK_CALL { return cc.name; }
const uint8_t *compile_error_hash(void) SOURCE_BLOCK_CALL { return cc.literal_length==32?cc.literal:0; }

static void map8(uint16_t p,uint8_t v) { far_set(cc.image->map_page,p,v); }

static void map16(uint16_t p,uint16_t v) { map8(p,(uint8_t)v); map8(p+1,(uint8_t)(v>>8)); }

static void map32(uint16_t p,uint32_t v) { uint8_t i; for(i=0;i<4;++i) map8(p+i,(uint8_t)(v>>(i*8))); }


void cc_mapping(uint16_t begin,uint16_t end,SourcePosition first,SourcePosition last) BACKEND {
    uint16_t offset=96+cc.file_bytes+cc.entries*16;
    uint16_t file=(uint16_t)(first>>16),start=(uint16_t)first,finish=(uint16_t)last;
    Source source;
    if(begin==end||cc.error->code) return;
    if(offset>PAGE_SIZE-16) { cc_fail(99); return; }
    if(!sources_get(&cc.image->sources,(uint8_t)file,&source)||start>source.length){cc_fail(99);return;}
    if((last>>16)!=file)finish=start<source.length?start+1:start;
    if(finish<start||finish>source.length){cc_fail(99);return;}
    map16(offset,begin);map16(offset+2,end);map16(offset+4,file);map16(offset+6,0);
    map32(offset+8,start);map32(offset+12,finish);++cc.entries;
}
void cc_map_source(uint16_t bytes) BACKEND {
    uint16_t i,first=96+cc.file_bytes,end=first+cc.entries*16;
    if((uint32_t)end+bytes>PAGE_SIZE){cc_fail(99);return;}
    i=end;while(i>first){--i;map8(i+bytes,far_get(cc.image->map_page,i));}
    cc.file_bytes+=bytes;
}

static void chain_image(void) {
    uint16_t i,from=cc.image->code_origin-0x4000u,count=cc.image->code_bytes;
    uint8_t digest[32];
    for(i=0;i<32;++i)digest[i]=image_get(cc.image,100+i);
    /* Destination precedes source, so a forward move is safe even when the
     * retained page vectors overlap. Ordinary globals and prefix are omitted. */
    for(i=0;i<count;++i) {
        if(!(i&255)&&!cc_poll())return;
        cc_put(96+i,image_get(cc.image,from+i));
    }
    for(i=0;i<96;++i)cc_put(i,0);
    for(i=0;i<4;++i)cc_put(i,(uint8_t)"ZCHN"[i]);
    cc_put(4,1);cc_word(6,96);cc_word(8,1);cc_word(10,cc.image->code_origin);
    cc_word(12,count);cc_word(14,GLOBAL_BASE);cc_word(16,cc.image->globals_bytes);
    cc_word(18,cc.sources->options->end);
    for(i=0;i<32;++i)cc_put(52+i,digest[i]);
    cc.image->length=96+count;
}
uint8_t cc_seal(const Source *s,const CompileOptions *options) FRONTEND {
    Build *b=cc.image;Diagnostic *d=cc.error;
    uint16_t i,j,at;uint8_t file,digest[32];Source active;Sha256 hash;
    b->revision=s->revision;sources_hash(&b->sources,0,b->source_hash);
    for(i=0;i<4;++i)map8(i,"ZMAP"[i]);
    map8(4,1);map16(6,96);map16(8,1);map16(10,b->code_origin);map16(12,b->code_bytes);
    map16(14,b->sources.count);map32(16,cc.entries);map32(84,cc.file_bytes);map32(88,(uint32_t)cc.entries*16);
    at=96;
    for(file=0;file<b->sources.count;++file) {
        if(!sources_get(&b->sources,file,&active)||!sources_hash(&b->sources,file,digest)){cc_fail(99);return 0;}
        j=(uint16_t)strlen(active.path);
        map16(at,file);map16(at+2,j);map32(at+4,active.length);
        for(i=0;i<32;++i)map8(at+8+i,digest[i]);
        for(i=0;i<j;++i)map8(at+40+i,(uint8_t)active.path[i]);
        at+=40+j;
    }
    if(at!=96+cc.file_bytes){cc_fail(99);return 0;}
    sha_init(&hash);
    for(i=96;i<at;++i)sha_byte(&hash,far_get(b->map_page,i));
    sha_end(&hash,digest);
    for(i=0;i<32;++i){cc_put(68+i,digest[i]);map8(52+i,digest[i]);}
    b->map_length=at+cc.entries*16;page_initialized(b->map_page,b->map_length);
    cc_hash_map(b,digest);if(d->code)return 0;
    for(i=0;i<32;++i)cc_put(100+i,digest[i]);
    cc_word(10,b->length);cc_word(20,b->code_origin);cc_word(22,b->code_bytes);
    cc_word(24,GLOBAL_BASE);cc_word(26,b->globals_bytes);cc_word(28,0x4000u+b->length);
    cc_word(30,options?options->end:0xfbffu);
    if(options&&options->kind==COMPILE_CHAIN)chain_image();
    if(d->code)return 0;
    cc_hash_image(b,b->id);
    if(d->code||!cc_poll())return 0;
    for(i=0;i<32;++i){cc_put((options&&options->kind==COMPILE_CHAIN?20:36)+i,b->id[i]);map8(20+i,b->id[i]);}
    /* Page2 also served as code staging. Once placed, a short image owns no
     * bytes there; retaining it can prevent a later run with parked includes. */
    for(i=0;i<3;++i)if((uint32_t)i*PAGE_SIZE>=b->length) {
        if(!page_drop(b->pages[i])) { d->code=98;return 0; }
        b->pages[i]=255;
    }
    if(!sources_pack(&b->sources)){d->code=98;return 0;}
    for(i=0;i<3;++i)b->versions[i]=b->pages[i]==255?0:page_versions[b->pages[i]];
    b->versions[3]=page_versions[b->map_page];b->versions[4]=page_versions[s->page];
    cc.polling=0;b->valid=1;return 1;
}
