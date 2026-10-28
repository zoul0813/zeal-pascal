#include "compiler.h"

static uint8_t flags(void) {
    return cc.absolute_mode|(cc.var_strict<<1)|(cc.range_check<<2)|
        (cc.io_check<<3)|(cc.index_speed<<4)|(cc.input_fresh<<5)|
        (cc.console_control<<6)|(cc.user_interrupt<<7);
}
static void options(uint8_t value,uint8_t limit) {
    cc.absolute_mode=value&1;cc.var_strict=(value>>1)&1;cc.range_check=(value>>2)&1;
    cc.io_check=(value>>3)&1;cc.index_speed=(value>>4)&1;cc.input_fresh=(value>>5)&1;
    cc.user_interrupt=(value>>7)&1;
    cc.console_control=(value>>6)&1;
    cc.with_limit=limit;
}
/* Persistent references also own their insertion context. The same file byte
 * can appear at several include sites; replay must resume the original site,
 * with its options, rather than the last include the scanner happened to read. */
#define REFERENCE_BYTES 10u
SourcePosition cc_source_position(void) ROUTINES {
    return ((SourcePosition)cc.sources->file<<16)|cc.scan;
}
uint8_t cc_source_byte(SourcePosition position) ROUTINES {
    return sources_byte(&cc.image->sources,(uint8_t)(position>>16),(uint16_t)position);
}
SourcePosition cc_reference_position(uint16_t reference) ROUTINES {
    uint8_t i;SourcePosition value=0;
    if(reference>=cc.sources->refs){cc_fail(99);return 0;}
    for(i=0;i<4;++i)value|=(SourcePosition)far_get(cc.sources->refs_page,reference*REFERENCE_BYTES+i)<<(i*8);
    return value;
}
uint16_t cc_source_reference(SourcePosition position) ROUTINES {
    uint16_t id,at;uint8_t i,context[6];
    if(cc.error->code)return 0;
    context[0]=(uint8_t)cc.sources->parent_scan;context[1]=(uint8_t)(cc.sources->parent_scan>>8);
    context[2]=cc.sources->parent_flags;context[3]=cc.sources->parent_limit;
    context[4]=flags();context[5]=cc.with_limit;
    for(id=0;id<cc.sources->refs;++id)if(cc_reference_position(id)==position) {
        for(i=0;i<6;++i)if(far_get(cc.sources->refs_page,id*REFERENCE_BYTES+4+i)!=context[i])break;
        if(i==6)return id;
    }
    if(id==PAGE_SIZE/REFERENCE_BYTES){cc_fail(98);return 0;}
    at=id*REFERENCE_BYTES;
    for(i=0;i<4;++i)far_set(cc.sources->refs_page,at+i,(uint8_t)(position>>(i*8)));
    for(i=0;i<6;++i)far_set(cc.sources->refs_page,at+4+i,context[i]);
    ++cc.sources->refs;return id;
}
void cc_reference_seek(uint16_t reference) ROUTINES {
    SourcePosition position=cc_reference_position(reference);
    uint16_t at=reference*REFERENCE_BYTES;
    if(cc.error->code)return;
    cc.sources->parent_scan=far_get(cc.sources->refs_page,at+4)|((uint16_t)far_get(cc.sources->refs_page,at+5)<<8);
    cc.sources->parent_flags=far_get(cc.sources->refs_page,at+6);
    cc.sources->parent_limit=far_get(cc.sources->refs_page,at+7);
    options(far_get(cc.sources->refs_page,at+8),far_get(cc.sources->refs_page,at+9));
    cc_source_seek(position);
}
void cc_source_seek(SourcePosition position) ROUTINES {
    uint8_t file=(uint8_t)(position>>16);
    if((position>>24)||!sources_get(&cc.image->sources,file,cc.input)||
       (uint16_t)position>cc.input->length){cc_fail(99);return;}
    cc.sources->file=file;cc.scan=(uint16_t)position;
}
void cc_source_save(SourceCursor *cursor) ROUTINES {
    cursor->first=cc.token_start;cursor->last=cc.token_end;
    cursor->scan=cc.scan;cursor->token=cc.token;cursor->file=cc.sources->file;
    cursor->flags=flags();cursor->limit=cc.with_limit;
    cursor->parent_scan=cc.sources->parent_scan;
    cursor->parent_flags=cc.sources->parent_flags;cursor->parent_limit=cc.sources->parent_limit;
}
void cc_source_restore(const SourceCursor *cursor) ROUTINES {
    if(cc.error->code)return; /* Retain the actual failing file and token. */
    cc_source_seek(cursor->first);options(cursor->flags,cursor->limit);
    cc.sources->parent_scan=cursor->parent_scan;
    cc.sources->parent_flags=cursor->parent_flags;cc.sources->parent_limit=cursor->parent_limit;
    cc_next();if(cursor->token==T_STRING&&cc.token=='^')cc_control_literal();
    cc.scan=cursor->scan;cc.token=cursor->token;
    cc.token_start=cursor->first;cc.token_end=cursor->last;
}
uint8_t cc_sources_prepare(const Source *root,const Sources *buffers) ROUTINES {
    const Source *origin=root;
    uint8_t file;
    cc.sources->root=root;cc.sources->buffers=buffers;
    if(buffers) {
        if(!sources_valid(buffers)){cc_fail(98);return 0;}
        for(file=0;file<buffers->count;++file)if(!sources_ready(buffers,file)){cc_fail(98);return 0;}
        if(sources_find(buffers,root->path)<0)origin=0;
    }
    if(!sources_new(&cc.image->sources)){cc_fail(98);return 0;}
    if(!source_snapshot(root,cc.input)){cc_fail(98);return 0;}
    if(sources_take(&cc.image->sources,cc.input,origin)<0) {
        source_close(cc.input);cc_fail(98);return 0;
    }
    cc.sources->refs_page=page_new(PAGE_COMPILER);
    if(cc.sources->refs_page==255){cc_fail(98);return 0;}
    return 1;
}
uint8_t cc_source_resume(void) ROUTINES {
    if(cc.scan!=cc.input->length||!cc.sources->file)return 0;
    /* B is global across includes; other switches and W restore. */
    options((cc.sources->parent_flags&~32u)|(cc.input_fresh<<5),cc.sources->parent_limit);
    cc_source_seek(cc.sources->parent_scan);return 1;
}
static uint8_t current(uint16_t at) {
    return at<cc.input->length?far_get(cc.input->page,at):0;
}
void cc_source_include(uint8_t comment) ROUTINES {
    char name[128],directory[128],path[128];
    Source loaded,buffer;Diagnostic diagnostic;
    uint16_t n=0,trim,after,base,opening;int16_t id,override=-1;
    uint8_t owned=0;
    if(cc.sources->file){cc_fail(93);return;}
    ++cc.scan;
    while(current(cc.scan)==' '||current(cc.scan)=='\t')++cc.scan;
    while(cc.scan<cc.input->length&&current(cc.scan)!='\n'&&
          !(comment=='{'?current(cc.scan)=='}':current(cc.scan)=='*'&&current(cc.scan+1)==')')) {
        if(n==127){cc_fail(90);return;}
        name[n++]=(char)current(cc.scan++);
    }
    trim=n;while(trim&&(name[trim-1]==' '||name[trim-1]=='\t'))--trim;
    name[trim]=0;
    if(!trim){cc_fail(90);return;}
    if(!(comment=='{'?current(cc.scan)=='}':current(cc.scan)=='*'&&current(cc.scan+1)==')')) {
        cc_fail(93);return;
    }
    after=cc.scan+(comment=='{'?1:2);
    strcpy(directory,cc.input->path);base=(uint16_t)strlen(directory);
    while(base&&directory[base-1]!='/')--base;
    directory[base]=0;
    if(!sources_path(name,directory,path)){cc_fail(90);return;}
    id=sources_find(&cc.image->sources,path);
    if(id==0){cc_fail(93);return;}
    if(id<0) {
        if(cc.sources->buffers)override=sources_find(cc.sources->buffers,path);
        if(override>=0) {
            if(!sources_get(cc.sources->buffers,(uint8_t)override,&buffer)||
               !source_snapshot(&buffer,&loaded)){cc_fail(98);return;}
            owned=1;
        } else if(read_block(&loaded,path,&diagnostic))owned=1;
        else {
            cc.error->code=diagnostic.code?diagnostic.code:90;
            cc.error->start=diagnostic.start;cc.error->end=diagnostic.end;
            cc.sources->external_error=1;strcpy(cc.input->path,path);return;
        }
        id=sources_take(&cc.image->sources,&loaded,override>=0?&buffer:0);
        if(id<0){if(owned)source_close(&loaded);cc_fail(98);return;}
        cc_map_source(40+(uint16_t)strlen(path));
        if(cc.error->code)return;
    }
    opening=(uint16_t)cc.token_start-(comment=='{'?1:2);
    while(opening&&current(opening-1)!='\n') {
        --opening;
        if(current(opening)!=' '&&current(opening)!='\t'){cc_fail(93);return;}
    }
    base=after;
    while(base<cc.input->length&&current(base)!='\n') {
        if(current(base)!=' '&&current(base)!='\t'){cc_fail(93);return;}
        ++base;
    }
    cc.sources->parent_scan=after;
    cc.sources->parent_flags=flags();cc.sources->parent_limit=cc.with_limit;
    cc_source_seek((SourcePosition)(uint8_t)id<<16);
}
