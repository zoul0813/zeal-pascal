#ifdef __SDCC
#define compile_request compiler_compile_request
#endif
#include "compiler_job.h"
#include "compiler.h"
#include "compiler_disk.h"
#include "output_transport.h"
#include <zos_errors.h>
#include <zos_vfs.h>
#ifdef ZP_UNITS_EXPERIMENT
#include "compiler_units.h"
#include "unit_output.h"
#include "library.h"
#endif

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
static uint8_t capture_source(const Source *original,Source *captured) {
    if(!source_valid(original))return 0;*captured=*original;return 1;
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
    Source source;
    b->revision=root->revision;
    if(root->page==255) {
        if(!sources_get(&b->sources,0,&source))return 0;
        source.page=255;
        if(sources_reference(&b->sources,&source)!=0)return 0;
    }
    b->versions[4]=root->page==255?0:page_versions[root->page];
    return build_check(root,b);
}
#ifdef ZP_UNITS_EXPERIMENT
static uint8_t unit_request(const Source *root,CompileFailure *failure) {
    Source source;Build scratch;Diagnostic diagnostic;
    int8_t fd=-1;uint8_t ok=0,primary,cleanup,reason;
    memset(&scratch,0,sizeof(scratch));memset(scratch.pages,255,3);
    scratch.map_page=scratch.sources.page=255;
    strcpy(failure->path,root->path);output_job_begin(1);
    if(!parent_poll()||!capture_source(root,&source)) {
        if(parent_interrupted){failure->diagnostic.code=COMPILE_INTERRUPTED;failure->interrupted=parent_interrupted;}
        goto done;
    }
    compiler_allocating=COMPILER_ALLOC_WORKER;
    if(source.page==255&&!read_block(&source,root->path,&failure->diagnostic)) {
        if(!failure->diagnostic.code)failure->diagnostic.code=90;
        goto done;
    }
    if(!code_phase_unit_io())goto done;
    fd=unit_output_begin(root);
    if(!code_phase_unit_io_release()||fd<0)goto done;
    ok=compile_unit(&source,&scratch,&diagnostic,fd,1);
    if(!ok) {
        failure->diagnostic=diagnostic;
        if(diagnostic.code==COMPILE_INTERRUPTED)failure->interrupted=compile_interrupt_reason();
        else strcpy(failure->path,compile_error_path());
    }
done:
    if(fd>=0)ok=output_finish(fd,ok);
    primary=platform_error;
    build_close(&scratch);
    cc_units_handoff_cleanup();
    cleanup=compiler_resources_ready();
    if(!cleanup){ok=0;failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;}
    else platform_error=primary;
    primary=platform_error;
    if(code_phase_unit_io()) {
        if(!ok)platform_error=primary;
        if(ok)ok=unit_output_publish(root,failure->path);
        reason=output_interrupt_reason();primary=platform_error;
        output_job_end();
        if(!unit_output_discard()){ok=0;failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;}
        else platform_error=primary;
        if(!code_phase_unit_io_release()){ok=0;failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;}
        if(!ok&&reason&&!platform_error&&
           (!failure->diagnostic.code||failure->diagnostic.code==COMPILE_PLATFORM_FAILURE)) {
            failure->diagnostic.code=COMPILE_INTERRUPTED;failure->interrupted=reason;
        }
    } else {ok=0;failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;}
    output_job_end();
    if(!ok&&!failure->diagnostic.code)failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;
    if(!ok&&failure->diagnostic.code==COMPILE_PLATFORM_FAILURE&&!platform_error)platform_error=ERR_FAILURE;
    return ok;
}
#endif
uint8_t compile_request(const Source *root,Build *b,CompileFailure *failure,
    uint8_t mode,const CompileOptions *options) {
    Source source;Build built;Diagnostic diagnostic;
    CompileOptions effective={0,0xfbffu,COMPILE_PROGRAM};
    uint8_t keep[8];uint8_t i;uint8_t ok=0;uint8_t prepared=0;
    memset(failure,0,sizeof(*failure));memset(b,0,sizeof(*b));memset(b->pages,255,3);b->map_page=b->sources.page=255;
    platform_error=0;parent_interrupted=0;
    if(!compiler_resources_ready()||!cancel_prepared_build())goto platform;
#ifdef ZP_UNITS_EXPERIMENT
    if(mode==COMPILER_UNIT_MODE)return unit_request(root,failure);
#endif
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
#ifdef ZP_UNITS_EXPERIMENT
    cc_units_handoff_cleanup();
#endif
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
    {uint8_t page=built.sources.page;do {bit(keep,page);page=buffer_next(page);}while(page!=255);}
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
    if(!ok)cc_disk_discard(&built);
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
