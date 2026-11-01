#ifdef __SDCC
#define compile_request compiler_compile_request
#endif
#include "compiler_job.h"
#include "compiler.h"
#include "output_transport.h"
#include <zos_errors.h>

/* The IDE is suspended for this synchronous invocation. Only captured source
 * descriptors and the final Build cross banks; neither needs a wire format. */
CompileFailure *compiler_invocation;
uint8_t compiler_memory_mode;
static const Sources *active_inputs;
static uint8_t input_pages[8],parent_interrupted;
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
static uint8_t canonical_digest(const Source *source,uint8_t digest[32]) {
    Sha256 hash;uint8_t c,previous=0;uint16_t i,line=0;
    sha_init(&hash);
    for(i=0;i<source->length;++i) {
        c=far_get(source->page,i);
        if(c=='\r'||c==26)return 0;
        if(c=='\n') {if(previous==' ')return 0;line=0;}
        else if(++line>127)return 0;
        sha_byte(&hash,c);previous=c;
    }
    if(previous==' ')return 0;
    sha_end(&hash,digest);return 1;
}

static uint8_t capture_source(Sources *inputs,const Source *original,uint8_t may_borrow) {
    Source captured;uint8_t hash[32]={0};uint8_t id=inputs->count;
    captured=*original;
    if(!source_valid(original))return 0;
    if(original->page==255) {
        captured.length=0;return sources_reference(inputs,&captured,hash)==id;
    }
    if(input_pages[original->page>>3]&(1u<<(original->page&7)))return 0;
    bit(input_pages,original->page);
    if(!may_borrow||!canonical_digest(original,hash)) {
        if(!source_snapshot(original,&captured))return 0;
        captured.revision=original->revision;
        if(!canonical_digest(&captured,hash))return 0;
    }
    /* Snapshots remain in the invocation ledger until the result takes them.
     * The input catalogue never owns source pages. */
    return sources_borrow(inputs,&captured,hash)==id;
}
static uint8_t prepare_inputs(Sources *inputs,const Source *root,const Sources *buffers,const char *work_path) {
    Source s;uint8_t i,count;
    parent_interrupted=0;memset(input_pages,0,8);
    if(!parent_poll()||!sources_new(inputs))return 0;
    if(!capture_source(inputs,root,!buffers||sources_find(buffers,root->path)>=0))return 0;
    if(buffers) {
        count=buffers->count;
        for(i=0;i<count;++i) {
            if(!parent_poll())return 0;
            if(!sources_ready(buffers,i)||!sources_get(buffers,i,&s)) {platform_error=ERR_ENTRY_CORRUPTED;return 0;}
            if(!strcmp(s.path,root->path))continue;
            if(!capture_source(inputs,&s,1))return 0;
        }
        bit(input_pages,buffers->page);
    }
    if(sources_find(inputs,work_path)<0) {platform_error=ERR_INVALID_PARAMETER;return 0;}
    return 1;
}
static uint8_t inputs_unchanged(const uint32_t versions[32],const uint32_t lifetimes[32]) {
    uint8_t p;
    for(p=32;p<64;++p)if(input_pages[p>>3]&(1u<<(p&7)))
        if(page_versions[p]!=versions[p-32]||page_lifetime(p)!=lifetimes[p-32])return 0;
    return 1;
}
/* Called before compiler cleanup releases include pages. Retain the actual
 * canonical location and digest directly; rebasing belongs to the IDE inputs. */
void compiler_capture_failure(void) SOURCE_BLOCK_CALL {
    Source source;const char *path=cc.sources->root->path;const Sources *resolved=0;int16_t id;
    CompileFailure *failure=compiler_invocation;
    if(!failure)return;
    failure->diagnostic=*cc.error;
    if(sources_valid(&cc.image->sources))resolved=&cc.image->sources;
    if(cc.sources->external_error)path=cc.input->path;
    else if(resolved&&sources_get(resolved,cc.sources->error_file,&source))path=source.path;
    strcpy(failure->path,path);
    id=resolved?sources_find(resolved,path):-1;
    if(id>=0)failure->has_hash=sources_hash(resolved,(uint8_t)id,failure->hash);
    else if(active_inputs&&(id=sources_find(active_inputs,path))>=0&&
            sources_get(active_inputs,(uint8_t)id,&source)&&source.page!=255)
        failure->has_hash=sources_hash(active_inputs,(uint8_t)id,failure->hash);
}
static void rebase_failure(const Source *root,const Sources *buffers,CompileFailure *failure) {
    Source source;const Source *original=0;int16_t id;
    if(root->page!=255&&!strcmp(failure->path,root->path))original=root;
    else if(buffers&&(id=sources_find(buffers,failure->path))>=0&&sources_get(buffers,(uint8_t)id,&source)&&source.page!=255)original=&source;
    if(original) {
        failure->diagnostic.start=source_editor_offset(original,failure->diagnostic.start);
        failure->diagnostic.end=source_editor_offset(original,failure->diagnostic.end);
    }
}
/* Keep the compiler's catalogue and its dense map IDs. Turn captured snapshots
 * into owned results and release disk-only includes without rebuilding records
 * from the map or reloading the already resident root. */
static uint8_t retain_sources(const Source *root,const Sources *buffers,const Sources *inputs,Build *b) {
    Source source,original;uint8_t hash[32],i,count=b->sources.count;int16_t id;
    for(i=0;i<count;++i) {
        if(!sources_get(&b->sources,i,&source))return 0;
        /* prepare_inputs captures resident bytes as borrowed records and
         * disk-only names as references. Result readiness binds source zero;
         * no descriptor copy is needed just to classify a captured input. */
        id=i?sources_find(inputs,source.path):-1;
        if(!i||(id>=0&&sources_borrowed(inputs,(uint8_t)id))) {
            if(compiler_page_owned(compiler_owned,source.page)||compiler_page_owned(parent_pending,source.page)) {
                const Source *origin=0;
                if(!i) {if(root->page!=255)origin=root;}
                else if(buffers&&(id=sources_find(buffers,source.path))>=0&&sources_get(buffers,(uint8_t)id,&original)&&original.page!=255)origin=&original;
                if(!sources_adopt(&b->sources,i,origin))return 0;
            }
        } else {
            if(!sources_hash(&b->sources,i,hash))return 0;
            source.page=255;
            if(sources_reference(&b->sources,&source,hash)!=i)return 0;
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
uint8_t compile_request(const Source *root,const Sources *buffers,Build *b,CompileFailure *failure,
    uint8_t mode,const CompileOptions *options,const char *work_path) {
    Sources inputs;Source source;Build built;Diagnostic diagnostic;
    CompileOptions effective={0,0xfbffu,COMPILE_PROGRAM};
    uint32_t versions[32],lifetimes[32];uint8_t keep[8],i,ok=0,prepared=0;
    memset(failure,0,sizeof(*failure));memset(b,0,sizeof(*b));memset(b->pages,255,3);b->map_page=b->sources.page=255;
    inputs.page=255;platform_error=0;parent_interrupted=0;
    if(!compiler_resources_ready()||!cancel_prepared_build())goto platform;
    if(options)effective=*options;
    if(mode>2||(mode==2&&(!options||effective.kind!=COMPILE_CHAIN))||
       (mode!=2&&effective.kind!=COMPILE_PROGRAM)||effective.end<0xcfffu||effective.end>0xfbffu||
       (effective.start&&(effective.start<0x8000u||effective.start>0xf400u))||
       (buffers&&!sources_valid(buffers))) {platform_error=ERR_INVALID_PARAMETER;goto platform;}
    for(i=0;i<32;++i) {versions[i]=page_versions[32+i];lifetimes[i]=page_lifetime(32+i);}
    compiler_allocating=COMPILER_ALLOC_PARENT;
    if(mode) {
        if(!prepare_build(root,buffers,mode==2,1)) {
            parent_interrupted=output_interrupt_reason();
            if(parent_interrupted)goto interrupted;
            goto platform;
        }
        prepared=1;
    }
    if(!prepare_inputs(&inputs,root,buffers,work_path)) {
        if(parent_interrupted)goto interrupted;
        goto platform;
    }
    if(!sources_get(&inputs,0,&source))goto corrupt;
    compiler_allocating=COMPILER_ALLOC_WORKER;
    if(source.page==255) {
        if(!read_block(&source,root->path,&diagnostic)) {
            failure->diagnostic=diagnostic;if(!failure->diagnostic.code)failure->diagnostic.code=90;
            strcpy(failure->path,root->path);goto done;
        }
        source.revision=root->revision;
    }
    active_inputs=&inputs;compiler_memory_mode=mode==0;compiler_invocation=failure;
    ok=compile_project(&source,&inputs,&built,&diagnostic,1,&effective);
    compiler_invocation=0;active_inputs=0;
    if(!inputs_unchanged(versions,lifetimes)) {ok=0;goto corrupt;}
    if(!ok) {
        if(diagnostic.code==COMPILE_INTERRUPTED) {
            failure->diagnostic=diagnostic;failure->interrupted=compile_interrupt_reason();
        } else rebase_failure(root,buffers,failure);
        goto done;
    }
    ok=0;
    if(!compiler_result_ready(&inputs,&effective,mode,&built))goto corrupt;
    if(!retain_sources(root,buffers,&inputs,&built))goto platform;
    memset(keep,0,8);for(i=0;i<3;++i)if(built.pages[i]!=255)bit(keep,built.pages[i]);
    bit(keep,built.map_page);bit(keep,built.sources.page);
    for(i=0;i<built.sources.count;++i) {
        if(!sources_get(&built.sources,i,&source))goto corrupt;
        if(source.page!=255)bit(keep,source.page);
    }
    /* One ledger transaction releases all scratch before transferring any
     * result ownership. A failed free keeps every surviving handle retryable. */
    for(i=0;i<8;++i) {parent_pending[i]|=compiler_owned[i];compiler_owned[i]=0;}
    if(!compiler_pages_release(parent_pending,keep))goto platform;
    *b=built;ok=1;goto done;
interrupted:
    failure->diagnostic.code=COMPILE_INTERRUPTED;failure->interrupted=parent_interrupted;goto done;
corrupt:
    platform_error=ERR_ENTRY_CORRUPTED;
platform:
    failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;
    if(!platform_error)platform_error=ERR_FAILURE;
done:
    compiler_invocation=0;active_inputs=0;
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
