#ifdef __SDCC
#define compile_project compiler_compile_project
#endif
#include "compiler.h"

/* Compilation owns transient sources, staging, relocation and sealing until
 * this phase returns. Run may then release its immutable code bank. */
static uint16_t pc(void) {return cc.emitted-CC_CODE_BASE;}

uint8_t compile_project(const Source *s,const Sources *buffers,Build *b,Diagnostic *d,uint8_t interruptible,const CompileOptions *options) {
    uint16_t i,j;
    SourcePosition begin;
    Source active;
    CompilerSources sources;
    memset(&cc,0,sizeof(cc));memset(&sources,0,sizeof(sources));
    sources.refs_page=255;sources.root=s;sources.options=options;
    cc.input=&active;cc.sources=&sources;cc.image=b;cc.error=d;
    cc.workspace=255;cc.stage_page=255;
    memset(d,0,sizeof(*d));memset(b,0,sizeof(*b));
    memset(b->pages,255,3);b->map_page=255;b->sources.page=255;
    cc.polling=interruptible;
    if(!cc_poll())goto failed;
    if(!cc_sources_prepare(s,buffers))goto failed;
    for(i=0;i<3;++i) {
        b->pages[i]=page_new(PAGE_IMAGE);
        if(b->pages[i]==255){d->code=98;goto failed;}
        for(j=0;j<PAGE_SIZE;++j) {
            if(!(j&255)&&!cc_poll())goto failed;
            far_set(b->pages[i],j,0);
        }
        page_initialized(b->pages[i],PAGE_SIZE);
    }
    b->map_page=page_new(PAGE_MAP);
    if(b->map_page==255){d->code=98;goto failed;}
    cc.workspace=page_new(PAGE_COMPILER);
    if(cc.workspace==255){d->code=98;goto failed;}
    for(i=0;i<96;++i)far_set(b->map_page,i,0);
    for(i=0;i<cc_runtime_size();++i) {
        if(!(i&255)&&!cc_poll())goto failed;
        cc_put(i,cc_runtime_get(i));
    }
    cc.absolute_mode=1;cc.var_strict=1;cc.index_speed=1;cc.with_limit=4;
    cc.input_fresh=1;cc.io_check=1;cc.console_control=1;cc.text_type=255;
    cc_new_type(K_INTEGER,2,-32768,32767,0);
    cc_new_type(K_STRING,0,0,0,0);
    cc_new_type(K_BOOLEAN,1,0,1,0);
    cc_new_type(K_CHAR,1,0,255,0);
    cc_new_type(K_INTEGER,1,0,255,0);
    cc_new_type(K_REAL,6,0,0,0);
    cc_system_init();
    cc.file_bytes=40+(uint16_t)strlen(s->path);
    cc_next();
    if(cc_is("program")) {
        cc_next();cc_expect(T_ID,53);
        if(cc.token=='('){cc_next();cc_expect(T_ID,53);while(cc.token==','){cc_next();cc_expect(T_ID,53);}cc_expect(')',5);}
        cc_expect(';',1);
    }
    b->code_origin=CC_CODE_BASE;cc.emitted=CC_CODE_BASE;
    if(options&&options->kind==COMPILE_CHAIN)cc_activation_begin();
    begin=cc_block();j=pc();cc_expect('.',10);
    if(cc.token!=T_EOF)cc_fail(58);
    cc_finish(RT_DONE);cc_mapping(j,pc(),begin,begin);
    cc_transfer_finish();cc_file_stream_finish();cc_hooks_finish();cc_console_finish();cc_file_finish();cc_error_finish();
    cc_unpack_finish();
    if(!options||options->kind!=COMPILE_CHAIN)cc_startup_finish();
    if(d->code||!cc_place_code())goto failed;
    if(cc.stage_page!=255) {
        if(page_drop(cc.stage_page))cc.stage_page=255;
        else {d->code=98;goto failed;}
    }
    if(page_drop(cc.workspace))cc.workspace=255;
    else {d->code=98;goto failed;}
    if(page_drop(sources.refs_page))sources.refs_page=255;
    else {d->code=98;goto failed;}
    if(!cc_seal(s,options))goto failed;
    return 1;
failed:
    if(d->code!=COMPILE_INTERRUPTED)cc_sources_error();
    cc.polling=0;
    if(sources.refs_page!=255&&page_drop(sources.refs_page))sources.refs_page=255;
    if(cc.stage_page!=255&&page_drop(cc.stage_page))cc.stage_page=255;
    if(cc.workspace!=255&&page_drop(cc.workspace))cc.workspace=255;
    build_close(b);return 0;
}

