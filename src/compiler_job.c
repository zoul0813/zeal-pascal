#ifdef __SDCC
#define compile_request compiler_compile_request
#endif
#include "compiler_job.h"
#include "compiler.h"
#include "image.h"
#include "saved_map.h"
#include "output_transport.h"
#include <zos_errors.h>

/* One synchronous parent registers the exact retained buffers and immutable
 * input handles. Nothing in a worker reply can expand this authority. */
static const uint8_t *registered_job;
static uint8_t *registered_context;
static uint8_t registered_pages[8];
static const uint32_t *registered_versions;
uint8_t *compiler_job_active;
uint8_t compiler_job_memory;

static void bit(uint8_t mask[8],uint8_t page,uint8_t set) {
    if(set)mask[page>>3]|=1u<<(page&7);else mask[page>>3]&=~(1u<<(page&7));
}
static void put(uint8_t page,uint16_t at,const uint8_t *in,uint16_t n) {while(n--)far_set(page,at++,*in++);}
static void handle(uint8_t *p,uint8_t page,uint16_t at) {p[0]=page;job_put16(p+1,at);}
static uint8_t cleanup(const uint8_t *keep) {return compiler_pages_release(compiler_owned,keep);}
static void reply(uint8_t kind,uint8_t ns,uint16_t code) {
    uint8_t *c=registered_context;
    memset(c+26,0,102);c[26]=kind;c[27]=ns;job_put16(c+28,code);job_put16(c+34,65535u);
    memcpy(c+44,registered_job+28,32);
}
static void reply_owned(void) {
    uint8_t p,n=0;uint8_t *c=registered_context;
    memset(c+82,0,32);
    for(p=0;p<64;++p)if(compiler_owned[p>>3]&(1u<<(p&7)))c[82+n++]=p;
    c[81]=n;
}
static uint8_t union_pages(uint8_t allowed[8]) {
    uint8_t i;for(i=0;i<8;++i)allowed[i]=registered_pages[i]|compiler_owned[i];return 1;
}
static uint8_t inputs_unchanged(void) {
    uint8_t page;
    if(!registered_versions)return 0;
    for(page=32;page<64;++page)if(registered_pages[page>>3]&(1u<<(page&7)))
        if(!page_lifetime(page)||page_versions[page]!=registered_versions[page-32])return 0;
    return 1;
}
static uint16_t unused_id(uint8_t page,uint16_t bytes,const uint8_t allowed[8]) {
    JobSource r;uint16_t id=0,at;uint8_t used;
    for(;;) {
        at=0;used=0;
        while(at<bytes) {
            if(!job_read_source(page,&at,bytes,allowed,&r))return 65535u;
            if(r.id==id) {used=1;break;}
        }
        if(!used)return id;
        if(++id==65535u)return id;
    }
}
/* Export canonical positions before the legacy in-process adapter rebases them
 * and before compiler cleanup releases its include catalog. */
static uint8_t failure_table(const Sources *resolved,const char *path,const Diagnostic *d) {
    JobSource r,original;Source source;uint8_t allowed[8],file,page;
    uint16_t at=0,input_at=job_word(registered_job+9),input_end=input_at+job_word(registered_job+12),i;
    int16_t id;uint8_t matched=0;
    page=page_new(PAGE_WORKER);if(page==255)return 0;
    union_pages(allowed);
    reply(2,1,d->code);registered_context[33]=1;
    for(i=0;i<job_word(registered_job+22);++i) {
        if(!job_read_source(registered_job[8],&input_at,input_end,registered_pages,&r))return 0;
        id=resolved?sources_find(resolved,r.source.path):-1;
        if(id>=0) {
            if(!sources_get(resolved,(uint8_t)id,&r.source)||!sources_hash(resolved,(uint8_t)id,r.hash))return 0;
            r.resident=1;
        }
        if(!strcmp(path,r.source.path)) {job_put16(registered_context+34,r.id);matched=1;}
        if(!job_write_source(page,&at,&r))return 0;
    }
    if(resolved)for(file=0;file<resolved->count;++file) {
        if(!sources_get(resolved,file,&source))return 0;
        if(job_find_path(registered_job,source.path,&original))continue;
        memset(&r,0,sizeof(r));r.source=source;r.resident=1;r.id=unused_id(page,at,allowed);
        if(!sources_hash(resolved,file,r.hash)||!job_write_source(page,&at,&r))return 0;
        if(!strcmp(path,r.source.path)) {job_put16(registered_context+34,r.id);matched=1;}
    }
    if(!matched) {
        memset(&r,0,sizeof(r));r.source.page=255;strcpy(r.source.path,path);r.id=unused_id(page,at,allowed);
        if(!job_write_source(page,&at,&r))return 0;
        job_put16(registered_context+34,r.id);
    }
    /* Includes with authenticated content remain alive through adoption. Mark
     * this catalog as a view; the authoritative invocation ledger still owns
     * every newly allocated page, even after the catalog closes. */
    if(resolved)for(file=0;file<resolved->count;++file) {
        if(!sources_get(resolved,file,&r.source)||!sources_hash(resolved,file,r.hash)||
           sources_borrow((Sources*)resolved,&r.source,r.hash)<0)return 0;
    }
    job_put32(registered_context+36,d->start);job_put32(registered_context+40,d->end);
    handle(registered_context+76,page,0);job_put16(registered_context+79,at);page_initialized(page,at);return 1;
}
void compiler_job_failure(void) SOURCE_BLOCK_CALL {
    Source source;const char *path=cc.sources->root->path;const Sources *resolved=0;
    if(!compiler_job_active)return;
    if(sources_valid(&cc.image->sources))resolved=&cc.image->sources;
    if(cc.sources->external_error)path=cc.input->path;
    else if(resolved&&sources_get(resolved,cc.sources->error_file,&source))path=source.path;
    if(!failure_table(resolved,path,cc.error))reply(6,4,platform_error?platform_error:ERR_NO_MORE_MEMORY);
}
static void descriptor(uint8_t page,uint16_t at,uint32_t length,const uint8_t *pages,uint8_t count) {
    uint8_t h[11];memset(h,0,sizeof(h));job_put32(h,length);h[4]=count;memcpy(h+8,pages,count);put(page,at,h,8+count);
}
static uint8_t success_reply(const Build *b) {
    uint8_t page,result[128],record[48],i,n,kind=registered_job[5]==2;
    uint16_t at=0,bytes,descriptors,result_at;char paths[2][128];JobStream stream;
    JobSource root;
    if(!job_find_source(registered_job,job_word(registered_job+14),&root)||!saved_paths_kind(root.source.path,paths,kind))return 0;
    page=page_new(PAGE_WORKER);if(page==255)return 0;
    n=(uint8_t)((b->length+PAGE_SIZE-1)/PAGE_SIZE);
    bytes=96+(uint16_t)strlen(paths[0])+(uint16_t)strlen(paths[1]);descriptors=bytes;result_at=bytes+36;
    for(i=0;i<2;++i) {
        memset(record,0,sizeof(record));record[0]=i?2:kind;job_put16(record+2,(uint16_t)strlen(paths[i]));
        stream.length=i?b->map_length:b->length;stream.count=i?1:n;
        if(i)stream.pages[0]=b->map_page;else memcpy(stream.pages,b->pages,n);
        job_put32(record+8,stream.length);job_hash_stream(&stream,0,0,record+12);
        handle(record+44,page,descriptors+(i?12:0));put(page,at,record,48);at+=48;
        put(page,at,(const uint8_t*)paths[i],(uint16_t)strlen(paths[i]));at+=(uint16_t)strlen(paths[i]);
    }
    descriptor(page,descriptors,b->length,b->pages,n);descriptor(page,descriptors+12,b->map_length,&b->map_page,1);
    descriptor(page,descriptors+24,bytes,&page,1);
    memset(result,0,sizeof(result));memcpy(result,"ZRES",4);result[4]=1;result[5]=kind;job_put16(result+6,128);
    memcpy(result+8,registered_job+28,32);memcpy(result+40,b->id,32);result[72]=n;memcpy(result+73,b->pages,n);
    job_put16(result+76,b->length);handle(result+78,page,descriptors+12);job_put32(result+81,b->map_length);
    job_put16(result+85,b->sources.count);job_put16(result+87,b->code_bytes);job_put16(result+89,b->globals_bytes);
    job_put16(result+91,job_word(registered_job+20)+1u-((b->code_origin+b->code_bytes+1u)&~1u));
    handle(result+93,page,descriptors+24);job_put16(result+96,2);put(page,result_at,result,128);
    reply(1,0,0);handle(registered_context+76,page,result_at);job_put16(registered_context+79,128);
    page_initialized(page,result_at+128);return 1;
}
/* The worker's only argument is its registered context. C Source/Build values
 * below are private decoded state; none is supplied by or returned to the IDE. */
static void execute_job(uint8_t context[128]) {
    Sources buffers;Source root;JobSource r;CompileOptions options;Build built;Diagnostic d;
    uint8_t ok=0,main_loaded=0;
    if(context!=registered_context||!inputs_unchanged()||!job_validate(context,registered_job,registered_pages)) {
        reply(6,4,ERR_INVALID_PARAMETER);reply_owned();return;
    }
    buffers.page=255;memset(&built,0,sizeof(built));memset(built.pages,255,3);built.map_page=built.sources.page=255;
    if(!sources_view(&buffers,registered_job[8],job_word(registered_job+9),(uint8_t)job_word(registered_job+22)))goto platform;
    if(!job_find_source(registered_job,job_word(registered_job+14),&r))goto platform;
    root=r.source;
    if(!r.resident) {
        if(!read_block(&root,r.source.path,&d)) {
            if(!d.code)d.code=90;
            if(!failure_table(0,r.source.path,&d))goto platform;
            goto done;
        }
        main_loaded=1;
    }
    options.start=job_word(registered_job+18);options.end=job_word(registered_job+20);
    options.kind=registered_job[5]==2?COMPILE_CHAIN:COMPILE_PROGRAM;
    compiler_job_memory=registered_job[5]==0;
    compiler_job_active=context;
    ok=compile_project(&root,&buffers,&built,&d,1,&options);
    compiler_job_active=0;
    if(!ok) {
        if(d.code==COMPILE_INTERRUPTED) {reply(5,0,0);context[32]=compile_interrupt_reason();}
        else if(context[26]!=2&&context[26]!=6)goto platform;
        goto done;
    }
    if(!success_reply(&built))goto platform;
    if(!sources_close(&built.sources))goto platform;
    if(main_loaded&&!page_drop(root.page))goto platform;
    goto done;
platform:
    compiler_job_active=0;reply(6,4,platform_error?platform_error:ERR_FAILURE);
done:
    if(!sources_close(&buffers))reply(6,4,platform_error?platform_error:ERR_FAILURE);
    reply_owned();
}

#define parent_pending compiler_parent_owned
static uint8_t parent_borrowed[8];
static uint8_t parent_interrupted;
static uint8_t parent_poll(void) {parent_interrupted=key_interrupt();return !parent_interrupted;}
static uint8_t snapshot_record(uint8_t table,uint16_t *at,const Source *original,uint16_t id,uint8_t may_borrow) {
    JobSource r;
    memset(&r,0,sizeof(r));r.id=id;r.source=*original;
    if(original->page==255)return job_write_source(table,at,&r);
    r.resident=1;
    /* The editor is suspended for the synchronous job. Already-canonical
     * pages can be borrowed directly; edits later invalidate their versions. */
    if(may_borrow&&job_source_digest(original,r.hash)) {
        if(original->length)bit(parent_borrowed,original->page,1);
        return job_write_source(table,at,&r);
    }
    if(!source_snapshot(original,&r.source))return 0;
    bit(parent_pending,r.source.page,1);source_hash(&r.source,r.hash);
    if(!r.source.length) {
        if(!page_drop(r.source.page))return 0;
        bit(parent_pending,r.source.page,0);r.source.page=255;
    }
    return job_write_source(table,at,&r);
}
static uint8_t prepare_job(uint8_t job[128],uint8_t context[128],const Source *root,const Sources *buffers,
                           uint8_t mode,const CompileOptions *options,const char *work_path) {
    uint8_t table,i;uint16_t at=0,count=1,work=65535u,transaction_at;
    uint32_t total=0;Source s;JobSource r;
    parent_interrupted=0;
    memset(parent_borrowed,0,8);
    if(mode>2||(mode==2&&(!options||options->kind!=COMPILE_CHAIN))||
       (mode!=2&&options&&options->kind!=COMPILE_PROGRAM)||
       (buffers&&!sources_valid(buffers))) {platform_error=ERR_INVALID_PARAMETER;return 0;}
    if(!parent_poll())return 0;
    table=page_new(PAGE_WORKER);if(table==255)return 0;bit(parent_pending,table,1);
    if(!snapshot_record(table,&at,root,0,!buffers||sources_find(buffers,root->path)>=0))return 0;
    if(!strcmp(root->path,work_path))work=0;
    if(buffers)for(i=0;i<buffers->count;++i) {
        if(!parent_poll())return 0;
        if(!sources_ready(buffers,i)||!sources_get(buffers,i,&s)) {platform_error=ERR_ENTRY_CORRUPTED;return 0;}
        if(!strcmp(s.path,root->path))continue;
        if(count==JOB_SOURCE_COUNT||!snapshot_record(table,&at,&s,count,1))return 0;
        if(!strcmp(s.path,work_path))work=count;
        ++count;
    }
    if(work==65535u) {platform_error=ERR_INVALID_PARAMETER;return 0;}
    memset(job,0,128);memcpy(job,"ZCMP",4);job[4]=1;job[5]=mode;job_put16(job+6,128);
    handle(job+8,table,0);job_put16(job+12,at);job_put16(job+14,0);job_put16(job+16,work);
    job_put16(job+18,options?options->start:0);job_put16(job+20,options?options->end:0xfbffu);
    job_put16(job+22,count);job_put16(job+60,1);
    for(i=0;i<8;++i)registered_pages[i]=parent_pending[i]|parent_borrowed[i];
    transaction_at=0;
    for(i=0;i<count;++i) {
        uint16_t position=transaction_at;
        if(!job_read_source(table,&position,at,registered_pages,&r))return 0;
        transaction_at=position;if(r.resident)total+=r.source.length;
    }
    job_put32(job+24,total);
    if(mode) {
        const char *path=output_recovery();uint16_t n=(uint16_t)strlen(path);
        if(!n||n>127||at>PAGE_SIZE-n) {platform_error=ERR_INVALID_PATH;return 0;}
        handle(job+62,table,at);job_put16(job+65,n);put(table,at,(const uint8_t*)path,n);at+=n;
    }
    page_initialized(table,at);job_identity(job,job+28);
    memset(context,0,128);memcpy(context,"ZRAM",4);context[4]=context[5]=1;
    job_put16(context+6,128);job_put16(context+22,(uint16_t)(uintptr_t)job);job_put16(context+24,128);job_put16(context+34,65535u);
    return 1;
}
static uint8_t validate_context(const uint8_t *job,const uint8_t *c) {
    uint8_t actual[8],i,p;
    if(memcmp(c,"ZRAM",4)||c[4]!=1||c[5]!=1||job_word(c+6)!=128||!job_zero(c+8,14)||
       job_word(c+22)!=(uint16_t)(uintptr_t)job||job_word(c+24)!=128||memcmp(c+44,job+28,32)||
       !job_zero(c+30,2)||c[81]>32||!job_zero(c+114,14))return 0;
    memset(actual,0,8);
    for(i=0;i<c[81];++i) {
        p=c[82+i];
        if(!job_page(compiler_owned,p)||(actual[p>>3]&(1u<<(p&7))))return 0;
        bit(actual,p,1);
    }
    if(memcmp(actual,compiler_owned,8)||!job_zero(c+82+c[81],32-c[81]))return 0;
    if(c[26]==2) {
        return c[27]==1&&job_word(c+28)&&job_word(c+28)<=100&&!c[32]&&c[33]==1&&
            job_word(c+34)!=65535u&&job_long(c+36)<=job_long(c+40)&&job_long(c+40)<=65535u&&
            job_word(c+79)&&job_handle(c+76,job_word(c+79),compiler_owned);
    }
    if(!job_zero(c+33,1)||job_word(c+34)!=65535u||!job_zero(c+36,8))return 0;
    if(c[26]==1)return !c[27]&&!job_word(c+28)&&!c[32]&&job_word(c+79)==128&&job_handle(c+76,128,compiler_owned);
    if(!job_zero(c+76,5))return 0;
    if(c[26]==5)return !c[27]&&!job_word(c+28)&&(c[32]==1||c[32]==2);
    return c[26]==6&&c[27]==4&&job_word(c+28)&&job_word(c+28)<=255&&!c[32];
}
static uint8_t validate_failure(const uint8_t *job,const uint8_t *c,CompileFailure *failure) {
    uint8_t allowed[8],found=0;JobSource r,prior,input;
    uint16_t at=job_word(c+77),base=at,end=at+job_word(c+79),scan,records=0,j;
    union_pages(allowed);
    if(page_role(c[76])!=PAGE_WORKER)return 0;
    while(at<end) {
        scan=base;
        if(!job_read_source(c[76],&at,end,allowed,&r)||!job_source_hash(&r))return 0;
        for(j=0;j<records;++j)if(!job_read_source(c[76],&scan,end,allowed,&prior)||prior.id==r.id||!strcmp(prior.source.path,r.source.path))return 0;
        if(job_find_source(job,r.id,&input)) {
            if(strcmp(input.source.path,r.source.path)||
               (input.resident&&(!r.resident||input.source.length!=r.source.length||memcmp(input.hash,r.hash,32)||input.source.page!=r.source.page)))return 0;
        } else if(job_find_path(job,r.source.path,&input))return 0;
        if(r.id==job_word(c+34)) {
            if(r.resident&&job_long(c+40)>r.source.length)return 0;
            strcpy(failure->path,r.source.path);memcpy(failure->hash,r.hash,32);failure->has_hash=r.resident;found=1;
        }
        ++records;
    }
    /* Every input ID must survive the failed compile, including unused work
     * overrides; an unrelated table cannot stand in for the job's provenance. */
    scan=job_word(job+9);
    for(j=0;j<job_word(job+22);++j) {
        uint8_t present=0;uint16_t k=base;
        if(!job_read_source(job[8],&scan,job_word(job+9)+job_word(job+12),registered_pages,&input))return 0;
        while(k<end) {if(!job_read_source(c[76],&k,end,allowed,&r))return 0;if(r.id==input.id)present=1;}
        if(!present)return 0;
    }
    return at==end&&found;
}
static uint16_t far_word(uint8_t page,uint16_t at) {return far_get(page,at)|((uint16_t)far_get(page,at+1)<<8);}
static uint8_t adopt_sources(const Source *root,const Sources *buffers,const uint8_t *job,Build *b,uint16_t count) {
    Source s,origin;JobSource input;uint8_t hash[32],i;uint16_t at=96,n;int16_t id;
    if(!sources_new(&b->sources))return 0;
    bit(parent_pending,b->sources.page,0); /* The Build now owns its catalog. */
    /* ZMAP IDs may be in any table order. The owning catalog uses dense IDs. */
    for(i=0;i<count;++i) {
        at=96;
        while(far_word(b->map_page,at)!=i)at+=40+far_word(b->map_page,at+2);
        if(!job_map_source(b,at,&s,hash))return 0;
        if(job_find_path(job,s.path,&input)&&input.resident) {
            const Source *original=0;
            if(!strcmp(s.path,root->path))original=root;
            if(buffers) {
                id=sources_find(buffers,s.path);original=0;
                if(id>=0) {if(!sources_get(buffers,(uint8_t)id,&origin))return 0;original=&origin;}
            }
            n=input.source.length;s=input.source;s.length=n;
            if(s.page==255) {
                if(original&&original->page!=255&&!original->length) {s=*original;bit(parent_borrowed,s.page,1);}
                else {if(!source_new(&s,input.source.path))return 0;bit(parent_pending,s.page,1);}
            }
            s.revision=original?original->revision:!i?root->revision:0;
            if(parent_borrowed[s.page>>3]&(1u<<(s.page&7))) {
                if(sources_borrow(&b->sources,&s,hash)!=i)return 0;
            } else {
                if(sources_take(&b->sources,&s,original)!=i)return 0;
                bit(parent_pending,s.page,0);
            }
        } else if(!i) {
            Diagnostic d;uint8_t digest[32];Source loaded;
            if(!read_block(&loaded,s.path,&d))return 0;
            bit(parent_pending,loaded.page,1);source_hash(&loaded,digest);
            if(loaded.length!=s.length||memcmp(hash,digest,32)) {platform_error=ERR_ENTRY_CORRUPTED;return 0;}
            loaded.revision=root->revision;
            if(sources_take(&b->sources,&loaded,0)!=i)return 0;
            bit(parent_pending,loaded.page,0);
        } else if(sources_reference(&b->sources,&s,hash)!=i)return 0;
        if(!i)memcpy(b->source_hash,hash,32);
    }
    b->revision=root->revision;
    for(i=0;i<3;++i)b->versions[i]=b->pages[i]==255?0:page_versions[b->pages[i]];
    b->versions[3]=page_versions[b->map_page];b->valid=1;
    if(root->page!=255) {b->versions[4]=page_versions[root->page];return build_check(root,b);}
    if(!sources_get(&b->sources,0,&s))return 0;
    b->versions[4]=page_versions[s.page];return build_check(&s,b);
}
uint8_t compile_request(const Source *root,const Sources *buffers,Build *b,CompileFailure *failure,
    uint8_t mode,const CompileOptions *options,const char *work_path) {
    uint8_t job[128],context[128],identity[32],initial[128],keep[8],i,ok=0,prepared=0;
    uint16_t files=0;uint8_t adopted=0;
    uint32_t versions[32];
    memset(failure,0,sizeof(*failure));memset(b,0,sizeof(*b));memset(b->pages,255,3);b->map_page=b->sources.page=255;
    platform_error=0;
    if(!compiler_resources_ready()||!cancel_prepared_build())goto platform;
    compiler_allocating=COMPILER_ALLOC_PARENT;
    if(mode>2) {platform_error=ERR_INVALID_PARAMETER;goto platform;}
    if(mode) {
        if(!prepare_build(root,buffers,mode==2,1)) {
            parent_interrupted=output_interrupt_reason();
            if(parent_interrupted)goto interrupted;
            goto platform;
        }
        prepared=1;
    }
    if(!prepare_job(job,context,root,buffers,mode,options,work_path)) {
        if(parent_interrupted)goto interrupted;
        goto platform;
    }
    registered_job=job;registered_context=context;registered_versions=versions;
    for(i=0;i<32;++i)versions[i]=page_versions[32+i];
    memcpy(identity,job+28,32);memcpy(initial,context,128);
    compiler_allocating=COMPILER_ALLOC_WORKER;execute_job(context);compiler_allocating=COMPILER_ALLOC_NONE;
    if(!inputs_unchanged()||memcmp(identity,job+28,32)||!job_validate(initial,job,registered_pages)||!validate_context(job,context))goto corrupt;
    if(context[26]==2) {
        if(!validate_failure(job,context,failure))goto corrupt;
        failure->diagnostic.code=(uint8_t)job_word(context+28);failure->diagnostic.start=(uint16_t)job_long(context+36);
        failure->diagnostic.end=(uint16_t)job_long(context+40);
        if(root->page!=255&&!strcmp(failure->path,root->path)) {
            failure->diagnostic.start=source_editor_offset(root,failure->diagnostic.start);
            failure->diagnostic.end=source_editor_offset(root,failure->diagnostic.end);
        } else if(buffers) {
            int16_t id=sources_find(buffers,failure->path);Source original;
            if(id>=0&&sources_get(buffers,(uint8_t)id,&original)) {
                failure->diagnostic.start=source_editor_offset(&original,failure->diagnostic.start);
                failure->diagnostic.end=source_editor_offset(&original,failure->diagnostic.end);
            }
        }
        goto done;
    }
    if(context[26]==5) {failure->diagnostic.code=COMPILE_INTERRUPTED;failure->interrupted=context[32];goto done;}
    if(context[26]==6) {platform_error=(uint8_t)job_word(context+28);goto platform;}
    if(!job_validate_success(job,context,b,&files))goto corrupt;
    memset(keep,0,8);for(i=0;i<3;++i)if(b->pages[i]!=255)bit(keep,b->pages[i],1);bit(keep,b->map_page,1);
    if(!cleanup(keep))goto platform;
    adopted=1;
    compiler_allocating=COMPILER_ALLOC_PARENT;
    if(!adopt_sources(root,buffers,job,b,files))goto platform;
    ok=1;goto done;
interrupted:
    failure->diagnostic.code=COMPILE_INTERRUPTED;failure->interrupted=parent_interrupted;goto done;
corrupt:
    platform_error=ERR_ENTRY_CORRUPTED;
platform:
    failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;
    if(!platform_error)platform_error=ERR_FAILURE;
done:
    compiler_job_active=0;registered_job=0;registered_context=0;registered_versions=0;
    {
        uint8_t primary=platform_error;
        if(!compiler_resources_ready()) {ok=0;failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;}
        else platform_error=primary;
    }
    if(!ok) {
        uint8_t error=platform_error;
        /* Before adoption these handles are owned exclusively by the ledger.
         * A malformed reply must never trick build_close into releasing them. */
        if(adopted)build_close(b);
        else {memset(b,0,sizeof(*b));memset(b->pages,255,3);b->map_page=b->sources.page=255;}
        platform_error=error;
        /* Cleanup failure leaves a reported recovery directory. Preserve the
         * actual compile location/cancel reason and any primary OS error. */
        if(prepared) {
            uint8_t primary=platform_error;
            if(!cancel_prepared_build()&&primary)platform_error=primary;
        }
    }
    return ok;
}
