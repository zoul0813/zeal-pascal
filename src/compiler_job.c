#ifdef __SDCC
#define compile_request compiler_compile_request
#endif
#include "compiler_job.h"
#include "compiler.h"
#include "compiler_disk.h"
#include "output_transport.h"
#include <zos_errors.h>

/* The IDE is suspended for this synchronous invocation. Only captured source
 * descriptors and the final Build cross banks; neither needs a wire format. */
CompileFailure *compiler_invocation;
uint8_t compiler_memory_mode;
static uint8_t parent_interrupted;
#define parent_pending compiler_parent_owned
static void bit(uint8_t mask[8],uint8_t page) {mask[page>>3]|=1u<<(page&7);}
static uint8_t parent_poll(void) {parent_interrupted=key_interrupt();return !parent_interrupted;}
static uint8_t source_valid(const Source *s) {
    char canonical[128];
    if(!memchr(s->path,0,128)||!s->path[0]||!sources_canonical(s->path,canonical)||strcmp(s->path,canonical)||
       s->length>SOURCE_LIMIT)return 0;
    return s->page==255||(s->page>=32&&s->page<64&&page_lifetime(s->page)&&
        (page_role(s->page)==PAGE_SOURCE||page_role(s->page)==PAGE_SNAPSHOT));
}
static uint8_t source_canonical(const Source *source) {
    uint8_t c;uint8_t previous=0;uint16_t i,line=0;
    for(i=0;i<source->length;++i) {
        c=buffer_get(source->page,i);
        if(c=='\r'||c==26)return 0;
        if(c=='\n') {if(previous==' ')return 0;line=0;}
        else if(++line>127)return 0;
        previous=c;
    }
    if(previous==' ')return 0;
    return 1;
}

static uint8_t capture_source(const Source *original,Source *captured) {
    *captured=*original;
    if(!source_valid(original))return 0;
    if(original->page==255) {captured->length=0;return 1;}
    if(!source_canonical(original)) {
        if(!source_snapshot(original,captured))return 0;
        captured->revision=original->revision;
        if(!source_canonical(captured))return 0;
    }
    return 1;
}
/* Called before compiler cleanup releases include pages. Retain the actual
 * canonical location directly; rebasing belongs to the IDE inputs. */
void compiler_capture_failure(void) SOURCE_BLOCK_CALL {
    Source source;const char *path=cc.sources->root->path;const Sources *resolved=0;
    CompileFailure *failure=compiler_invocation;
    if(!failure)return;
    failure->diagnostic=*cc.error;
    if(sources_valid(&cc.image->sources))resolved=&cc.image->sources;
    if(cc.sources->external_error)path=cc.input->path;
    else if(resolved&&sources_get(resolved,cc.sources->error_file,&source))path=source.path;
    strcpy(failure->path,path);
}
static void rebase_failure(const Source *root,CompileFailure *failure) {
    if(root->page!=255&&!strcmp(failure->path,root->path)) {
        failure->diagnostic.start=source_editor_offset(root,failure->diagnostic.start);
        failure->diagnostic.end=source_editor_offset(root,failure->diagnostic.end);
    }
}
/* Only the root can come from Work. Includes become disk references once
 * parsing ends; the table retains their IDs for source-error lookup. */
static uint8_t retain_sources(const Source *root,Build *b) {
    Source source;uint8_t i;uint8_t count=b->sources.count;
    for(i=0;i<count;++i) {
        if(!sources_get(&b->sources,i,&source))return 0;
        if(!i) {
            if(compiler_page_owned(compiler_owned,source.page)||compiler_page_owned(parent_pending,source.page))
                if(!sources_adopt(&b->sources,i,root->page==255?0:root))return 0;
        } else {
            source.page=255;
            if(sources_reference(&b->sources,&source)!=i)return 0;
        }
    }
    b->revision=root->revision;
    if(root->page==255) {
        if(!sources_get(&b->sources,0,&source))return 0;
        root=&source;
    }
    b->versions[4]=page_versions[root->page];
    return build_check(root,b);
}
uint8_t compile_request(const Source *root,Build *b,CompileFailure *failure,
    uint8_t mode,const CompileOptions *options) {
    Source source;Build built;Diagnostic diagnostic;
    CompileOptions effective={0,0xfbffu,COMPILE_PROGRAM};
    uint8_t keep[8];uint8_t i;uint8_t ok=0;uint8_t prepared=0;
    memset(failure,0,sizeof(*failure));memset(b,0,sizeof(*b));memset(b->pages,255,3);b->map_page=b->sources.page=255;
    platform_error=0;parent_interrupted=0;
    if(!compiler_resources_ready()||!cancel_prepared_build())goto platform;
    if(options)effective=*options;
    if(mode>2||(mode==2&&(!options||effective.kind!=COMPILE_CHAIN))||
       (mode!=2&&effective.kind!=COMPILE_PROGRAM)||effective.end<0xcfffu||effective.end>0xfbffu||
       (effective.start&&(effective.start<0x8000u||effective.start>0xf400u))) {platform_error=ERR_INVALID_PARAMETER;goto platform;}
    compiler_allocating=COMPILER_ALLOC_PARENT;
    if(mode) {
        if(!prepare_build(root,mode==2,1)) {
            parent_interrupted=output_interrupt_reason();
            if(parent_interrupted)goto interrupted;
            goto platform;
        }
        prepared=1;
    }
    if(!parent_poll()||!capture_source(root,&source)) {
        if(parent_interrupted)goto interrupted;
        goto platform;
    }
    compiler_allocating=COMPILER_ALLOC_WORKER;
    if(source.page==255) {
        if(!read_block(&source,root->path,&diagnostic)) {
            failure->diagnostic=diagnostic;if(!failure->diagnostic.code)failure->diagnostic.code=90;
            strcpy(failure->path,root->path);goto done;
        }
        source.revision=root->revision;
    }
    compiler_memory_mode=mode==0;compiler_invocation=failure;
    compiler_disk_requested=mode!=0;
    ok=compile_project(&source,&built,&diagnostic,1,&effective);
    compiler_disk_requested=0;
    compiler_invocation=0;
    if(!ok) {
        if(diagnostic.code==COMPILE_INTERRUPTED) {
            failure->diagnostic=diagnostic;failure->interrupted=compile_interrupt_reason();
        } else rebase_failure(root,failure);
        goto done;
    }
    ok=0;
    if(!compiler_result_ready(&built))goto corrupt;
    if(!retain_sources(root,&built)||!cc_tables_end())goto platform;
    memset(keep,0,8);for(i=0;i<3;++i)if(built.pages[i]!=255)bit(keep,built.pages[i]);
    {uint8_t page=built.map_page;do {bit(keep,page);page=buffer_next(page);}while(page!=255);}
    bit(keep,built.sources.page);
    for(i=0;i<built.sources.count;++i) {
        if(!sources_get(&built.sources,i,&source))goto corrupt;
        if(source.page!=255) {
            uint8_t page=source.page;
            do {bit(keep,page);page=buffer_next(page);}while(page!=255);
        }
    }
    /* One ledger transaction releases all scratch before transferring any
     * result ownership. A failed free keeps every surviving handle retryable. */
    for(i=0;i<8;++i) {parent_pending[i]|=compiler_owned[i];compiler_owned[i]=0;}
    if(!compiler_pages_release(parent_pending,keep))goto platform;
    *b=built;cc_disk_adopt(&built,b);ok=1;goto done;
interrupted:
    failure->diagnostic.code=COMPILE_INTERRUPTED;failure->interrupted=parent_interrupted;goto done;
corrupt:
    platform_error=ERR_ENTRY_CORRUPTED;
platform:
    failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;
    if(!platform_error)platform_error=ERR_FAILURE;
done:
    compiler_invocation=0;
    {
        uint8_t primary=platform_error;
        if(!compiler_resources_ready()) {ok=0;failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;}
        else platform_error=primary;
    }
    if(!ok&&prepared) {
        uint8_t primary=platform_error;
        if(!cancel_prepared_build()&&primary)platform_error=primary;
    }
    return ok;
}
