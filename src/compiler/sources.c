#include "compiler/link/layout.h"
#include "compiler/disk.h"
#include "compiler/groups/native.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "units/internal.h"
#endif
#if defined(__SDCC) && defined(ZP_STATIC_LAYOUT)
/* Unit serialization still reads declaration names and constants after the
 * parser returns. Keep source access in the retained support bank so the
 * otherwise idle Inline/transfer generator can lend its physical page. */
#pragma codeseg BANK11
#pragma constseg BANK11
#endif

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
SourcePosition cc_source_position(void) ROUTINES {
    return ((SourcePosition)cc.sources->file<<16)|cc.scan;
}
uint8_t cc_source_byte(SourcePosition position) ROUTINES {
#ifdef ZP_OVERLAYS
    uint32_t offset;uint16_t length;
#endif
#ifdef ZP_UNITS_EXPERIMENT
    if(position&0x80000000UL)return cc_unit_module_byte((uint16_t)(position>>16)&0x7fffu,(uint16_t)position);
#endif
#ifdef ZP_OVERLAYS
    if(cc.sources->overlays&&sources_spool_offset(&cc.image->sources,(uint8_t)(position>>16),&length,&offset))
        return (uint16_t)position<length?cc_overlay_source_byte(offset+(uint16_t)position):0;
#endif
    return sources_byte(&cc.image->sources,(uint8_t)(position>>16),(uint16_t)position);
}
SourcePosition cc_reference_position(uint16_t reference) ROUTINES {
    uint8_t i,bytes[4];SourcePosition value=0;
    if(reference>=cc.sources->refs){cc_fail(99);return 0;}
    if(!cc_reference_read(reference,bytes,4))return 0;
    for(i=0;i<4;++i)value|=(SourcePosition)bytes[i]<<(i*8);
    return value;
}
uint16_t cc_source_reference(SourcePosition position) ROUTINES {
    uint16_t id;uint8_t i,raw=0;uint8_t context[6],bytes[10];
    if(cc.error->code)return 0;
#ifdef ZP_UNITS_EXPERIMENT
    raw=(position&0x80000000UL)!=0;
#endif
    context[0]=(uint8_t)cc.sources->parent_scan;context[1]=(uint8_t)(cc.sources->parent_scan>>8);
    context[2]=cc.sources->parent_flags;context[3]=cc.sources->parent_limit;
    context[4]=flags();context[5]=cc.with_limit;
    /* A position above every prior key cannot be a duplicate. Replayed
     * positions still search the complete insertion contexts below. */
    id=cc.sources->refs;
    if(!id)cc.sources->highest_reference=0;
    if(id&&!raw&&position<=cc.sources->highest_reference)for(id=0;id<cc.sources->refs;++id) {
        if(!(id&63u)&&!cc_poll())return 0;
        if(!cc_reference_read(id,bytes,10))return 0;
        for(i=0;i<4;++i)
            if(bytes[i]!=(uint8_t)(position>>(i*8)))break;
        if(i!=4)continue;
        for(i=0;i<6;++i)if(bytes[4+i]!=context[i])break;
        if(i==6)return id;
    }
    /* Real constant references reserve the high bit for sign and7FFF for Pi. */
    if(id==REAL_PI){cc_fail(99);return 0;}
    for(i=0;i<4;++i)cc_reference_put(id,i,(uint8_t)(position>>(i*8)));
    for(i=0;i<6;++i)cc_reference_put(id,4+i,context[i]);
    if(cc.error->code)return 0;
    /* Raw names/literals are already cached by unit materialization. They
     * must not raise the ordinary source high-water mark or force scans. */
    if(!raw&&(!id||position>cc.sources->highest_reference))cc.sources->highest_reference=position;
    ++cc.sources->refs;return id;
}
void cc_reference_seek(uint16_t reference) ROUTINES {
    SourcePosition position=0;uint8_t bytes[10],i;
    if(reference>=cc.sources->refs){cc_fail(99);return;}
    if(!cc_reference_read(reference,bytes,10))return;
    for(i=0;i<4;++i)position|=(SourcePosition)bytes[i]<<(i*8);
    cc.sources->parent_scan=bytes[4]|((uint16_t)bytes[5]<<8);
    cc.sources->parent_flags=bytes[6];cc.sources->parent_limit=bytes[7];
    options(bytes[8],bytes[9]);cc_source_seek(position);
}
void cc_source_seek(SourcePosition position) ROUTINES {
    uint8_t file=(uint8_t)(position>>16);
    if((position>>24)||!sources_get(&cc.image->sources,file,cc.input)||
       (uint16_t)position>cc.input->length){cc_fail(99);return;}
    cc.sources->file=file;cc.sources->input_base=sources_offset(&cc.image->sources,file);cc.scan=(uint16_t)position;
}
void cc_source_save(SourceCursor *cursor) ROUTINES {
#ifdef ZP_UNITS_EXPERIMENT
    cursor->unit_qualifier=cc.sources->unit_qualifier;
#endif
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
#ifdef ZP_UNITS_EXPERIMENT
    cc.sources->unit_qualifier=cursor->unit_qualifier;
#endif
}
uint8_t cc_sources_prepare(const Source *root) ROUTINES {
    cc.sources->root=root;*cc.input=*root;
    if(!sources_new(&cc.image->sources)||sources_borrow(&cc.image->sources,root)<0){cc_fail(99);return 0;}
    cc.sources->input_base=0;return 1;
}
#ifdef ZP_OVERLAYS
/* Scanning USES/the first declaration can already have entered an include
 * before overlay mode starts. Capture those views too, then release their
 * shared arena only after every descriptor points into the private spool. */
uint8_t cc_sources_spool(void) ROUTINES {
    Sources *set=&cc.image->sources;Source view;uint8_t id;uint32_t offset;
    SourcePosition position=cc_source_position();
    for(id=1;id<set->count;++id) {
        if(!sources_get(set,id,&view)){cc_fail(99);return 0;}
        if(view.page!=255&&(!cc_overlay_source_store(&view,sources_offset(set,id),&offset)||
            sources_spooled(set,&view,offset)!=id)){cc_fail(99);return 0;}
    }
    if(!buffer_drop(set->text_page)){cc_fail(99);return 0;}
    set->text_page=set->text_tail=255;set->text_used=0;
    cc_source_seek(position);return !cc.error->code;
}
#endif
/* Source references are dead after parsing. Only compact file identities and
 * statement positions survive with the build; no include text is retained. */
uint8_t cc_sources_finish(void) ROUTINES {
    if(!cc_references_end()||!sources_finish(&cc.image->sources)){cc_fail(99);return 0;}
    cc.sources->refs=0;
    return 1;
}
uint8_t cc_source_resume(void) ROUTINES {
    if(cc.scan!=cc.input->length||!cc.sources->file)return 0;
    /* B is global across includes; other switches and W restore. */
    options((cc.sources->parent_flags&~32u)|(cc.input_fresh<<5),cc.sources->parent_limit);
    cc_source_seek(cc.sources->parent_scan);return 1;
}
static uint8_t current(uint16_t at) {
    if(cc.input->page==255)return at<cc.input->length?cc_source_byte(((SourcePosition)cc.sources->file<<16)|at):0;
    return at<cc.input->length?buffer_view_get(cc.input->page,cc.sources->input_base,at):0;
}
void cc_source_include(uint8_t comment) ROUTINES {
    char name[128],directory[128],path[128];
    Source loaded;Diagnostic diagnostic;
    uint16_t n=0,trim,after,base,opening;int16_t id;
#ifdef ZP_OVERLAYS
    uint32_t offset;uint16_t spool_length,bank;
#endif
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
#ifdef ZP_UNITS_EXPERIMENT
    id=sources_find_input(&cc.image->sources,path);
#else
    id=sources_find(&cc.image->sources,path);
#endif
    if(id==0){cc_fail(93);return;}
#ifdef ZP_UNITS_EXPERIMENT
    if(id>0&&(!sources_get(&cc.image->sources,(uint8_t)id,&loaded)||
        (loaded.page==255
#ifdef ZP_OVERLAYS
         &&!sources_spool_offset(&cc.image->sources,(uint8_t)id,&spool_length,&offset)
#endif
        ))){cc_fail(93);return;}
#endif
    if(id<0) {
#ifdef ZP_OVERLAYS
        /* Includes are scanned while consuming the prior declaration's last
         * token. Its immutable snapshot can lend the member page to the
         * canonicalizing loader, rather than requiring a second scratch bank. */
        bank=cc.sources->bank;
        if(bank&&!cc_overlay_switch(0))return;
#endif
        if(!read_block(&loaded,path,&diagnostic)) {
            cc.error->code=diagnostic.code==98?99:diagnostic.code?diagnostic.code:90;
            cc.error->start=diagnostic.start;cc.error->end=diagnostic.end;
            cc.sources->external_error=1;strcpy(cc.input->path,path);return;
        }
#ifdef ZP_OVERLAYS
        if(cc.sources->overlays)id=cc_overlay_source_store(&loaded,0,&offset)?
            sources_spooled(&cc.image->sources,&loaded,offset):-1;
        else
#endif
        id=sources_include(&cc.image->sources,&loaded,cc.polling);
        source_close(&loaded);
        if(loaded.page!=255){cc.image->sources.pending=loaded.page;cc_fail(99);return;}
        if(id<-1){cc.interrupted=(uint8_t)(-id-1);cc_fail(255);return;}
        if(id<0){cc_fail(99);return;}
        if(cc.error->code)return;
#ifdef ZP_OVERLAYS
        if(bank&&!cc_overlay_switch(bank))return;
#endif
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
    cc_source_seek((SourcePosition)(TypeId)id<<16);
}

/* Retained UI/diagnostic services must survive any failed parser-bank reload. */
/* Shared job bounds also drive the IDE options annotations. */
uint8_t compile_limits_at(uint16_t data,uint16_t globals,uint16_t code,const CompileOptions *options,CompileLimits *limits) SOURCE_BLOCK_CALL {
    uint32_t start,end,top,needed;
    uint16_t stack=code?cc_required_stack():2048u;
    memset(limits,0,sizeof(*limits));
    if(data<0x40a0u||(uint32_t)data+globals>=CC_STATIC_LIMIT||(options&&options->kind>COMPILE_CHAIN))return 0;
    limits->minimum_start=data+globals;
    start=options&&options->start?options->start:limits->minimum_start;
    end=options?options->end:0xfbffu;
    if(!code)code=1; /* UI platform bounds before a program has been compiled. */
    top=end+1;
    if(top>=(uint32_t)stack+code)limits->maximum_start=(uint16_t)(((top-stack)&~1UL)-code);
    needed=((start+code+1)&~1UL)+stack-1;
    if(needed<0xcfffu)needed=0xcfffu;
    limits->minimum_end=needed>65535u?65535u:(uint16_t)needed;
    return end>=0xcfffu&&end<=0xfbffu&&start>=limits->minimum_start&&
        start<=limits->maximum_start&&end>=needed;
}
uint8_t compile_limits(uint16_t globals,uint16_t code,const CompileOptions *options,CompileLimits *limits) SOURCE_BLOCK_CALL {
    return compile_limits_at(GLOBAL_BASE,globals,code,options,limits);
}

/* Error provenance is captured while invocation-owned compiler inputs live. */
void cc_sources_error(void) ROUTINES {
    Source failed;const Source *original=0;
    cc.literal_length=0;
    if(cc.sources->external_error){strcpy(cc.name,cc.input->path);return;}
    if(!sources_get(&cc.image->sources,cc.sources->error_file,&failed)) {
        strcpy(cc.name,cc.sources->root->path);return;
    }
    strcpy(cc.name,failed.path);
    if(!cc.sources->error_file)original=cc.sources->root;
    if(original) {
        cc.error->start=source_editor_offset(original,cc.error->start);
        cc.error->end=source_editor_offset(original,cc.error->end);
    }
}
