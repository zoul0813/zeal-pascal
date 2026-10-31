/* White-box corruption tests execute the production decoder/worker/adopter.
 * Including this translation unit keeps test controls out of the target ABI. */
#include "../../src/compiler_job.c"
static uint8_t host_prepared,host_cancel_failure;
static unsigned host_prepares,host_cancels;
uint8_t prepare_build(const Source *root,const Sources *buffers,uint8_t chain,uint8_t polling) {
    (void)root;(void)buffers;(void)chain;(void)polling;
    assert(!host_prepared);host_prepared=1;++host_prepares;return 1;
}
uint8_t cancel_prepared_build(void) {
    if(!host_prepared)return 1;
    ++host_cancels;host_prepared=0;
    if(host_cancel_failure) {platform_error=ERR_FAILURE;return 0;}
    return 1;
}
const char *output_recovery(void) {return host_prepared?"H:/ZP0001":"";}
uint8_t output_interrupt_reason(void) {return 0;}
static void job_register(uint8_t job[128],uint8_t context[128]) {
    static uint32_t versions[32];uint8_t i;
    for(i=0;i<32;++i)versions[i]=page_versions[i+32];
    registered_job=job;registered_context=context;registered_versions=versions;
}
static void job_dispose(void) {
    check(cleanup(0)&&compiler_pages_release(compiler_parent_owned,0));registered_job=0;registered_context=0;compiler_job_active=0;
}
/* Exercise the real parent capture without worker/hash traffic in the read
 * counter. Canonicality and digest results remain checked independently. */
static void capture_one(const uint8_t *text,uint16_t length,uint8_t may_borrow,
                        uint8_t canonical,uint16_t normalized) {
    Source original,before;JobSource record;uint8_t table,allowed[8],expected[32],raw[32],after[32],i;
    uint16_t at=0,end;unsigned pages=pages_used();Sha256 hash;
    check(source_new(&original,"H:/CAPTURE.PAS"));
    for(at=0;at<length;++at)far_set(original.page,at,text[at]);original.length=length;
    original.dirty=1;original.revision=17;page_initialized(original.page,length);before=original;
    sha_init(&hash);for(at=0;at<length;++at)sha_byte(&hash,far_get(original.page,at));sha_end(&hash,raw);
    source_hash(&original,expected);table=page_new(PAGE_WORKER);check(table!=255);bit(parent_pending,table,1);
    memset(parent_borrowed,0,8);at=0;capture_probe_page=original.page;capture_reads=0;capture_probe=1;
    check(snapshot_record(table,&at,&original,7,may_borrow));capture_probe=0;end=at;
    for(i=0;i<8;++i)allowed[i]=parent_pending[i]|parent_borrowed[i];
    at=0;check(job_read_source(table,&at,end,allowed,&record)&&at==end&&record.id==7&&record.resident);
    check(record.source.length==normalized&&!memcmp(record.hash,expected,32));
    check(job_source_hash(&record)==canonical);
    if(may_borrow&&canonical&&length==normalized) {
        check(record.source.page==(length?original.page:255));
        check((parent_borrowed[original.page>>3]&(1u<<(original.page&7)))!=0||!length);
        check(pages_used()==pages+2);
    } else {
        check(record.source.page!=original.page&&job_zero(parent_borrowed,8));
        check(pages_used()==pages+2+(normalized!=0));
    }
    check(!memcmp(&original,&before,sizeof(original)));
    sha_init(&hash);for(at=0;at<length;++at)sha_byte(&hash,far_get(original.page,at));sha_end(&hash,after);
    check(!memcmp(raw,after,32));
    printf("PERF03 capture bytes=%u borrow=%u canonical=%u reads=%lu\n",length,may_borrow,canonical,capture_reads);
    check(compiler_pages_release(parent_pending,0));memset(parent_borrowed,0,8);
    source_close(&original);check(pages_used()==pages);
}
static void capture_cases(void) {
    uint8_t text[16000],i;uint16_t at;Source original,before;uint8_t table;uint16_t end;
    const char *cases[]={"", "begin end.\n", "begin  \nend.  ", " \n  ", "x\ry", "x\032y"};
    const uint16_t normalized[]={0,11,10,1,3,3};
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        capture_one((const uint8_t*)cases[i],(uint16_t)strlen(cases[i]),1,i<4,normalized[i]);
        capture_one((const uint8_t*)cases[i],(uint16_t)strlen(cases[i]),0,i<4,normalized[i]);
    }
    memset(text,'x',128);capture_one(text,127,1,1,127);capture_one(text,128,1,0,128);
    for(at=0;at<sizeof(text);++at)text[at]=at%100==99?'\n':'x';
    capture_one(text,sizeof(text),1,1,sizeof(text));
    capture_one(text,sizeof(text),0,1,sizeof(text));
    /* A residency-zero source has no byte reads or owned snapshot. */
    memset(&original,0,sizeof(original));original.page=255;strcpy(original.path,"H:/DISK.PAS");
    table=page_new(PAGE_WORKER);check(table!=255);end=0;capture_probe=1;capture_reads=0;
    check(snapshot_record(table,&end,&original,1,1));capture_probe=0;check(!capture_reads);check(page_drop(table));
    /* Failed fallback allocation preserves the source and ownership ledger. */
    check(source_new(&original,"H:/FAIL.PAS"));table=page_new(PAGE_WORKER);check(table!=255);end=0;
    fail_after=0;check(!snapshot_record(table,&end,&original,1,0));fail_after=-1;
    check(!end&&job_zero(parent_pending,8)&&job_zero(parent_borrowed,8));check(page_drop(table));source_close(&original);
    /* Failed canonical scan followed by failed fallback must not export the
     * unfinished digest or alter the editor buffer. */
    check(source_new(&original,"H:/FAIL.PAS"));
    check(source_replace(&original,0,0,(const uint8_t*)"x  ",3));before=original;
    table=page_new(PAGE_WORKER);check(table!=255);end=0;
    fail_after=0;check(!snapshot_record(table,&end,&original,1,1));fail_after=-1;
    check(!end&&job_zero(parent_pending,8)&&job_zero(parent_borrowed,8)&&
          !memcmp(&original,&before,sizeof(original))&&!memcmp(memory[original.page],"x  ",3));
    check(page_drop(table));source_close(&original);
}
static void wire_lookup_meter(const Sources *set,const char *label,const char *path,int16_t expected) {
    unsigned allocations=allocation_calls;uint32_t version=page_versions[set->page];int16_t found;
    wire_lookup_page=set->page;wire_lookup_reads=0;wire_lookup_probe=1;
    found=sources_find(set,path);wire_lookup_probe=0;
    check(found==expected&&allocations==allocation_calls&&version==page_versions[set->page]);
    printf("PERF06 %s records=%u reads=%lu\n",label,set->count,wire_lookup_reads);
}
/* Packed wire descriptors can outnumber available resident source pages.
 * These capacity records are deliberately nonresident; each has55bytes. */
static void wire_lookup_cases(void) {
    Sources view,stale;JobSource record;Source live;uint8_t table,i,j,old,role;uint16_t at,base=17,middle;
    const uint8_t offsets[]={0,2,3,4,8,40,42,44,47};
    const uint8_t values[]={255,2,1,0,1,0,1,0,'/'};
    unsigned pages=pages_used();
    table=page_new(PAGE_WORKER);check(table!=255);memset(&record,0,sizeof(record));record.source.page=255;
    for(i=0;i<2;++i) {
        uint8_t count=i?86:10;
        at=base;
        for(j=0;j<count;++j) {
            record.id=1000+(uint16_t)j*7;snprintf(record.source.path,sizeof(record.source.path),"H:/F%03u.PAS",j);
            check(job_write_source(table,&at,&record));
        }
        check(at==base+(uint16_t)count*55&&sources_view(&view,table,base,count));
        wire_lookup_meter(&view,"first","H:/F000.PAS",0);
        snprintf(record.source.path,sizeof(record.source.path),"H:/F%03u.PAS",count-1);
        wire_lookup_meter(&view,"last",record.source.path,count-1);
        wire_lookup_meter(&view,"missing","H:/missing.pas",-1);
        wire_lookup_meter(&view,"restart","H:/F000.PAS",0);
    }
    /* Rebuild the10-record table for malformed/duplicate middle rows. */
    at=base;
    for(j=0;j<10;++j){record.id=1000+(uint16_t)j*7;snprintf(record.source.path,sizeof(record.source.path),"H:/F%03u.PAS",j);check(job_write_source(table,&at,&record));}
    middle=base+4*55;
    for(i=0;i<sizeof(offsets);++i) {
        uint16_t position=middle+offsets[i];old=far_get(table,position);far_set(table,position,values[i]);
        /* A decoded invalid ID is65535; set both bytes for that mutation. */
        if(!i)far_set(table,middle+1,255);
        check(sources_view(&view,table,base,10));
        check(sources_find(&view,"H:/F000.PAS")==0&&sources_find(&view,"H:/F003.PAS")==3);
        check(sources_find(&view,"H:/F004.PAS")==-1&&sources_find(&view,"H:/F009.PAS")==-1&&sources_find(&view,"H:/missing.pas")==-1);
        if(i==1)wire_lookup_meter(&view,"malformed-middle","H:/F009.PAS",-1);
        far_set(table,position,old);if(!i)far_set(table,middle+1,(uint8_t)((1000+4*7)>>8));
    }
    far_set(table,base+5*55+44+6,'2');check(sources_view(&view,table,base,10));
    check(sources_find(&view,"H:/F002.PAS")==2&&sources_find(&view,"H:/F005.PAS")==-1);
    far_set(table,base+5*55+44+6,'5');check(sources_view(&view,table,base,10));
    check(sources_find(&view,"H:/f000.PAS")==-1);
    stale=view;far_set(table,0,0xa5);wire_lookup_meter(&stale,"stale-version","H:/F000.PAS",-1);
    check(sources_view(&view,table,base,10));stale=view;++stale.lifetime;
    wire_lookup_meter(&stale,"stale-lifetime","H:/F000.PAS",-1);
    /* A record ending exactly atPAGE_SIZE is valid; a truncated next row is not. */
    memset(&record,0,sizeof(record));record.source.page=255;record.id=42;strcpy(record.source.path,"H:/F000.PAS");
    at=PAGE_SIZE-55;check(job_write_source(table,&at,&record)&&at==PAGE_SIZE);
    check(sources_view(&view,table,PAGE_SIZE-55,2));
    check(sources_find(&view,"H:/F000.PAS")==0&&sources_find(&view,"H:/missing.pas")==-1);
    /* Shift a valid header/path prefix right, leaving the final byte outside
     * the page; reject by size before reading the incomplete path. */
    for(j=54;j;--j)far_set(table,PAGE_SIZE-54+j-1,far_get(table,PAGE_SIZE-55+j-1));
    check(sources_view(&view,table,PAGE_SIZE-54,1));check(sources_find(&view,"H:/F000.PAS")==-1);
    /* Resident wire descriptors retain page role/liveness validation. */
    check(source_new(&live,"H:/LIVE.PAS"));check(source_import(&live,(const uint8_t*)"begin end.",10,&d));
    memset(&record,0,sizeof(record));record.id=37;record.resident=1;record.source=live;source_hash(&live,record.hash);
    at=base;check(job_write_source(table,&at,&record)&&sources_view(&view,table,base,1));
    check(sources_find(&view,live.path)==0);role=owned[live.page];owned[live.page]=PAGE_RUN;
    check(sources_find(&view,live.path)==-1);owned[live.page]=role;
    old=far_get(table,at-1);far_set(table,at-1,table);check(sources_view(&view,table,base,1));check(sources_find(&view,live.path)==-1);
    far_set(table,at-1,old);check(sources_view(&view,table,base,1));source_close(&live);
    check(sources_find(&view,record.source.path)==-1);
    stale=view;check(page_drop(table));check(sources_find(&stale,"H:/F000.PAS")==-1);
    table=page_new(PAGE_WORKER);check(table==stale.page);check(sources_find(&stale,"H:/F000.PAS")==-1);
    check(page_drop(table)&&pages_used()==pages);
}
static void wire_mutations(void) {
    uint8_t job[128],context[128],copy[128],old,mask[8];uint16_t i,at;unsigned allocations;JobSource r;
    check(prepare_job(job,context,&s,0,0,0,s.path));job_register(job,context);
    check(job_validate(context,job,registered_pages));allocations=allocation_calls;
    for(i=0;i<128;++i) {
        memcpy(copy,job,128);job[i]^=1;
        check(!job_validate(context,job,registered_pages)&&allocation_calls==allocations);
        memcpy(job,copy,128);
        old=context[i];context[i]^=1;
        check(!job_validate(context,job,registered_pages)&&allocation_calls==allocations);context[i]=old;
    }
    /* Semantic validation still rejects malformed records after their identity
     * has been recomputed. Include both NUL path and oversized path length. */
    {
        const uint8_t offsets[]={2,3,4,8,40,41,42,43,44};
        const uint8_t values[]={2,2,255,0xff,128,1,1,1,0};
        for(i=0;i<sizeof(offsets);++i) {
            at=offsets[i];old=far_get(job[8],at);far_set(job[8],at,values[i]);job_identity(job,job+28);
            check(!job_validate(context,job,registered_pages)&&allocation_calls==allocations);
            far_set(job[8],at,old);job_identity(job,job+28);
        }
    }
    at=0;check(job_read_source(job[8],&at,job_word(job+12),registered_pages,&r));
    old=far_get(r.source.page,0);far_set(r.source.page,0,old^1);
    check(!job_validate(context,job,registered_pages)&&allocation_calls==allocations);far_set(r.source.page,0,old);
    check(job_validate(context,job,registered_pages));
    /* Borrowed pages may be viewed but not released by a running worker. */
    memcpy(mask,compiler_owned,8);compiler_allocating=1;
    check(!page_drop(r.source.page)&&(page_role(r.source.page)==PAGE_SOURCE||page_role(r.source.page)==PAGE_SNAPSHOT)&&!memcmp(mask,compiler_owned,8));
    compiler_allocating=0;
    check(!inputs_unchanged());compiler_allocating=1;execute_job(context);compiler_allocating=0;
    check(context[26]==6&&allocation_calls==allocations);
    job_dispose();check(pages_used()==1);
}
static void result_mutations(void) {
    uint8_t job[128],context[128],saved[128],payload[128],old;uint16_t i,at,files;Build candidate;
    check(prepare_job(job,context,&s,0,0,0,s.path));job_register(job,context);
    compiler_allocating=1;execute_job(context);compiler_allocating=0;
    check(context[26]==1&&validate_context(job,context));
    check(job_validate_success(job,context,&candidate,&files)&&files==1);
    memcpy(saved,context,128);at=job_word(context+77);for(i=0;i<128;++i)payload[i]=far_get(context[76],at+i);
    for(i=0;i<128;++i) {
        context[i]^=1;check(!(validate_context(job,context)&&context[26]==1&&job_validate_success(job,context,&candidate,&files)));memcpy(context,saved,128);
        far_set(context[76],at+i,payload[i]^1);
        check(!job_validate_success(job,context,&candidate,&files));far_set(context[76],at+i,payload[i]);
    }
    check(job_validate_success(job,context,&candidate,&files));
    old=far_get(candidate.map_page,96);far_set(candidate.map_page,96,old^1);
    check(!job_validate_success(job,context,&candidate,&files));far_set(candidate.map_page,96,old);
    old=far_get(candidate.pages[0],160);far_set(candidate.pages[0],160,old^1);
    check(!job_validate_success(job,context,&candidate,&files));far_set(candidate.pages[0],160,old);
    check(job_validate_success(job,context,&candidate,&files));
    job_dispose();check(pages_used()==1);
}
/* This caller supplies only serialized bytes and owned pages. Arbitrary input
 * IDs deliberately differ from the compiler's dense output-map IDs. */
static void hand_constructed_job(const uint8_t expected[32]) {
    const char *text="program Hello;begin writeln('Hello, Zeal!')end.";
    const char *bad="begin wrong end.";
    const char *path="H:/SLICE.PAS",*unused="H:/UNUSED.PAS";
    uint8_t job[128],context[128],prefix[44],hash[32],table,page,old;uint16_t at=0,n,i,files=0;Sha256 sha;Build candidate;CompileFailure failure;JobSource record,borrowed;
    table=page_new(PAGE_WORKER);page=page_new(PAGE_SNAPSHOT);bit(parent_pending,table,1);bit(parent_pending,page,1);
    n=(uint16_t)strlen(text);put(page,0,(const uint8_t*)text,n);sha_init(&sha);
    for(i=0;i<n;++i)sha_byte(&sha,(uint8_t)text[i]);sha_end(&sha,hash);
    memset(prefix,0,sizeof(prefix));job_put16(prefix,37);prefix[2]=prefix[3]=1;
    job_put32(prefix+4,n);memcpy(prefix+8,hash,32);job_put16(prefix+40,(uint16_t)strlen(path));
    put(table,at,prefix,44);at+=44;put(table,at,(const uint8_t*)path,(uint16_t)strlen(path));at+=(uint16_t)strlen(path);far_set(table,at++,page);
    memset(prefix,0,sizeof(prefix));job_put16(prefix,9);job_put32(prefix+4,0xffffffffUL);job_put16(prefix+40,(uint16_t)strlen(unused));
    put(table,at,prefix,44);at+=44;put(table,at,(const uint8_t*)unused,(uint16_t)strlen(unused));at+=(uint16_t)strlen(unused);
    memset(job,0,128);memcpy(job,"ZCMP",4);job[4]=1;job_put16(job+6,128);handle(job+8,table,0);
    job_put16(job+12,at);job_put16(job+14,37);job_put16(job+16,9);job_put16(job+20,0xfbffu);job_put16(job+22,2);
    job_put32(job+24,n);job_put16(job+60,1);job_identity(job,job+28);
    memset(context,0,128);memcpy(context,"ZRAM",4);context[4]=context[5]=1;job_put16(context+6,128);
    job_put16(context+22,(uint16_t)(uintptr_t)job);job_put16(context+24,128);job_put16(context+34,65535u);
    memcpy(registered_pages,parent_pending,8);job_register(job,context);
    check(job_validate(context,job,registered_pages));compiler_allocating=1;execute_job(context);compiler_allocating=0;
    check(context[26]==1&&validate_context(job,context)&&job_validate_success(job,context,&candidate,&files));
    check(files==1&&!memcmp(candidate.id,expected,32));check(cleanup(0));
    n=(uint16_t)strlen(bad);put(page,0,(const uint8_t*)bad,n);sha_init(&sha);
    for(i=0;i<n;++i)sha_byte(&sha,(uint8_t)bad[i]);sha_end(&sha,hash);
    job_put32(prefix,n);put(table,4,prefix,4);put(table,8,hash,32);job_put32(job+24,n);job_identity(job,job+28);
    memset(context+26,0,102);job_put16(context+34,65535u);job_register(job,context);
    compiler_allocating=1;execute_job(context);compiler_allocating=0;
    check(context[26]==2&&job_word(context+34)==37&&validate_context(job,context));
    memset(&failure,0,sizeof(failure));check(validate_failure(job,context,&failure)&&!strcmp(failure.path,path)&&failure.has_hash);
    at=job_word(context+77);old=far_get(context[76],at);far_set(context[76],at,old^1);
    check(!validate_failure(job,context,&failure));far_set(context[76],at,old);
    /* A2 allows resolved records to borrow immutable job pages. Reusing an
     * authenticated byte stream under another file ID does not allocate it
     * or transfer its ownership to the worker. */
    check(job_find_source(job,37,&borrowed)&&job_find_source(job,9,&record));
    at=job_word(context+79);record=borrowed;record.id=12;strcpy(record.source.path,"H:/INCLUDE.INC");
    check(job_write_source(context[76],&at,&record));job_put16(context+79,at);job_put16(context+34,12);
    check(validate_failure(job,context,&failure)&&!strcmp(failure.path,"H:/INCLUDE.INC"));
    at=0;check(job_write_source(context[76],&at,&borrowed));
    check(job_find_source(job,9,&record));record.resident=1;record.source.page=borrowed.source.page;
    record.source.length=borrowed.source.length;memcpy(record.hash,borrowed.hash,32);
    check(job_write_source(context[76],&at,&record));job_put16(context+79,at);job_put16(context+34,9);
    check(validate_failure(job,context,&failure)&&!strcmp(failure.path,unused));
    job_dispose();check(pages_used()==1);
}
static void job_capacity(void) {
    uint8_t reserved[23],i,hash[32],before;Sources buffers;Source part,extra[7],prior[7];CompileFailure failure;char name[32];
    HostSource disk[]={{"H:/MAIN.PAS","begin writeln('DISK')end."},{"H:/PART.INC","const answer=42;"}};
    load("program Include;\n{$I PART.INC}\nbegin writeln(answer)end.");
    for(i=0;i<23;++i) {reserved[i]=page_new(PAGE_RUN);check(reserved[i]!=255);}
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    check(source_new(&part,"H:/PART.INC"));check(source_import(&part,(const uint8_t*)"const answer=42;",16,&d));
    source_hash(&part,hash);check(sources_borrow(&buffers,&part,hash)==1);before=pages_used();
    check(compile_request(&s,&buffers,&b,&failure,0,0,part.path));
    check(b.valid&&sources_borrowed(&b.sources,0)&&sources_borrowed(&b.sources,1));build_close(&b);check(pages_used()==before);
    check(source_replace(&part,13,15,0,0));check(sources_borrow(&buffers,&part,hash)==1);
    check(!compile_request(&s,&buffers,&b,&failure,0,0,part.path));
    check(failure.diagnostic.code<101&&failure.has_hash&&!strcmp(failure.path,part.path)&&pages_used()==before);
    check(sources_close(&buffers));source_close(&part);
    /* Disk-only Main is a residency0 input, not a temporary editor buffer. */
    disk_sources=disk;disk_source_count=2;
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);before=pages_used();
    check(compile_request(&s,&buffers,&b,&failure,0,0,s.path)&&b.sources.count==2);
    build_close(&b);check(pages_used()==before);check(sources_close(&buffers));
    memset(&part,0,sizeof(part));part.page=255;strcpy(part.path,disk[0].path);
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);before=pages_used();
    check(compile_request(&part,&buffers,&b,&failure,0,0,s.path));
    check(b.valid&&sources_get(&b.sources,0,&part)&&part.page!=255&&!sources_borrowed(&b.sources,0));
    check(build_check(&part,&b));build_close(&b);check(pages_used()==before);check(sources_close(&buffers));
    disk_sources=0;disk_source_count=0;
    /* Finite native capacity includes parked buffers that need canonical
     * copies. Even the maximum resident catalog must fail without losing
     * edits, borrowed pages or transient invocation allocations. */
    load("begin end.");check(sources_new(&buffers));source_hash(&s,hash);
    check(sources_borrow(&buffers,&s,hash)==0);
    for(i=0;i<7;++i) {
        snprintf(name,sizeof(name),"H:/UNUSED%u.PAS",i);
        check(source_new(&extra[i],name));check(source_replace(&extra[i],0,0,(const uint8_t*)"unused ",7));
        source_hash(&extra[i],hash);check(sources_borrow(&buffers,&extra[i],hash)==i+1);prior[i]=extra[i];
        if(i==1||i==6) {
            before=pages_used();check(!compile_request(&s,&buffers,&b,&failure,0,0,s.path));
            check((failure.diagnostic.code==98||failure.diagnostic.code==COMPILE_PLATFORM_FAILURE)&&pages_used()==before);
            check(!memcmp(extra,prior,(i+1)*sizeof(Source))&&job_zero(compiler_owned,8)&&job_zero(compiler_parent_owned,8));
        }
    }
    check(sources_close(&buffers));for(i=0;i<7;++i)source_close(&extra[i]);
    for(i=0;i<23;++i)check(page_drop(reserved[i]));check(pages_used()==1);
}
static void large_job_capacity(void) {
    char text[4096];FILE *file;size_t length;uint8_t held[23],i,expected[32],hash[32];unsigned required,allocation;
    Sources buffers;Source before;CompileFailure failure;
    CompileOptions options={0x8040,0xfbffu,COMPILE_PROGRAM};
    file=fopen("tests/native/chain-call.pas","rb");check(file!=0);
    length=fread(text,1,sizeof(text),file);check(!ferror(file)&&length<sizeof(text));check(!fclose(file));
    check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));before=s;
    check(compile_project(&s,0,&b,&d,0,&options));memcpy(expected,b.id,32);build_close(&b);
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    for(i=0;i<23;++i){held[i]=page_new(PAGE_RUN);check(held[i]!=255);}
    /* Exactly nine pages remain, including suspended Work and its catalog,
     * just as in the sixteen-bank IDE. Match the direct compiler's bytes. */
    allocation_calls=0;check(compile_request(&s,&buffers,&b,&failure,1,&options,s.path));required=allocation_calls;
    check(b.valid&&b.code_bytes>PAGE_SIZE&&!memcmp(b.id,expected,32)&&build_check(&s,&b));
    check(!memcmp(&s,&before,sizeof(s)));build_close(&b);check(cancel_prepared_build());
    for(allocation=0;allocation<required;++allocation) {
        fail_after=(int)allocation;
        check(!compile_request(&s,&buffers,&b,&failure,1,&options,s.path));fail_after=-1;
        check(pages_used()==25&&!b.valid&&!host_prepared&&!memcmp(&s,&before,sizeof(s)));
        check(job_zero(compiler_owned,8)&&job_zero(compiler_parent_owned,8)&&sources_ready(&buffers,0));
    }
    cancel_stage=7;cancel_result=1;
    check(!compile_request(&s,&buffers,&b,&failure,1,&options,s.path));cancel_stage=0;
    check(failure.diagnostic.code==COMPILE_INTERRUPTED&&pages_used()==25&&!b.valid&&!host_prepared);
    check(!memcmp(&s,&before,sizeof(s))&&sources_ready(&buffers,0));
    check(sources_ready(&buffers,0)&&sources_close(&buffers));
    for(i=0;i<23;++i)check(page_drop(held[i]));
    check(pages_used()==1);
}
/* Independent interval counting selects the first read of each of the six
 * worker/parent hashes, without coupling fault injection to implementation. */
static unsigned stream_copy_count(uint16_t length,uint16_t zero,uint16_t bytes) {
    unsigned count=0;uint32_t i,start,end,page_end;
    for(i=0;i<length;i=page_end) {
        page_end=i+PAGE_SIZE;if(page_end>length)page_end=length;
        end=page_end<zero?page_end:zero;
        if(end>i)count+=(end-i+63)/64;
        start=i>(uint32_t)zero+bytes?i:(uint32_t)zero+bytes;
        if(page_end>start)count+=(page_end-start+63)/64;
    }
    return count;
}
static void stream_hash_failures(void) {
    Source before;CompileFailure failure;unsigned spans[6],at=0,k,total=0;
    load("begin end.");before=s;check(compile(&s,&b,&d));
    spans[0]=spans[4]=stream_copy_count(b.length,0,0);
    spans[1]=spans[5]=stream_copy_count(b.map_length,0,0);
    spans[2]=stream_copy_count(b.length,36,32);
    spans[3]=stream_copy_count(b.map_length,20,32);build_close(&b);
    for(k=0;k<6;++k)total+=spans[k];
    stream_copy_calls=0;stream_copy_trace=1;
    check(compile_request(&s,0,&b,&failure,0,0,s.path));stream_copy_trace=0;
    check(stream_copy_calls==total);build_close(&b);check(pages_used()==1);
    for(k=0;k<6;++k) {
        check(spans[k]>0&&stream_copy_offsets[at]==0);
        stream_copy_calls=0;stream_read_fail_after=(int)at;
        check(!compile_request(&s,0,&b,&failure,0,0,s.path));
        stream_read_fail_after=-1;
        check(stream_copy_calls==at+1&&stream_failed_page==stream_copy_pages[at]&&
              stream_failed_offset==stream_copy_offsets[at]);
        check(failure.diagnostic.code==COMPILE_PLATFORM_FAILURE&&!b.valid&&!host_prepared);
        check(pages_used()==1&&!memcmp(&s,&before,sizeof(s)));
        check(job_zero(compiler_owned,8)&&job_zero(compiler_parent_owned,8)&&
              job_zero(parent_pending,8)&&compiler_allocating==COMPILER_ALLOC_NONE);
        at+=spans[k];
    }
}
/* Worker initialization failures must retain the reply/parent cleanup path. */
static void image_fill_failures(void) {
    static const unsigned failures[]={0,1,32,64,65,96,128};
    Source before;CompileFailure failure;unsigned i;uint8_t hash[32];
    load("program Init;var pad:array[1..193]of byte;n:integer;const k:integer=17;begin n:=k;writeln(n)end.");before=s;
    source_hash(&s,hash);
    image_fill_total_calls=0;
    check(compile_request(&s,0,&b,&failure,0,0,s.path));check(image_fill_total_calls==129);
    build_close(&b);check(pages_used()==1&&!memcmp(&s,&before,sizeof(s)));
    for(i=0;i<sizeof(failures)/sizeof(failures[0]);++i) {
        image_fill_total_calls=0;image_fill_fail_after=(int)failures[i];
        check(!compile_request(&s,0,&b,&failure,0,0,s.path));image_fill_fail_after=-1;
        check(image_fill_total_calls==failures[i]+1);
        check(failure.diagnostic.code==98&&!b.valid&&!host_prepared&&pages_used()==1);
        check(failure.has_hash&&!strcmp(failure.path,s.path)&&!memcmp(failure.hash,hash,32));
        check(!memcmp(&s,&before,sizeof(s))&&job_zero(compiler_owned,8)&&job_zero(compiler_parent_owned,8)&&job_zero(parent_pending,8));
        check(compiler_allocating==COMPILER_ALLOC_NONE);
    }
}
static void compiler_job_cases(void) {
    CompileFailure failure;Source before,include;Sources buffers;uint8_t hash[32],expected[32],prior;
    unsigned pages,allocation,prepares=host_prepares,cancels=host_cancels;uint8_t stage,reason;
    HostSource disk[]={ {"H:/BAD.INC","const answer=;\n"},{"H:/GOOD.INC","const answer=42;\n"} };
    load("program Hello;begin writeln('Hello, Zeal!')end.");s.cursor=11;s.previous=4;s.dirty=1;before=s;
    check(compile(&s,&b,&d));memcpy(expected,b.id,32);build_close(&b);
    check(compile_request(&s,0,&b,&failure,0,0,s.path));
    check(b.valid&&!memcmp(expected,b.id,32)&&!memcmp(&s,&before,sizeof(s))&&!host_prepared);
    check(build_check(&s,&b)&&sources_resident(&b.sources,0));build_close(&b);check(pages_used()==1);
    hand_constructed_job(expected);wire_mutations();result_mutations();job_capacity();
    stream_hash_failures();image_fill_failures();
    check(host_prepares==prepares&&host_cancels==cancels);
    strcpy(s.path,"H:/bad//name.pas");
    check(!compile_request(&s,0,&b,&failure,0,0,s.path)&&failure.diagnostic.code==COMPILE_PLATFORM_FAILURE);
    check(platform_error==ERR_FAILURE&&pages_used()==1);strcpy(s.path,"H:/SLICE.PAS");
    /* Canonical failure positions must rebase to the dirty editor, not to a
     * worker-global filename or an offset into its trimmed input. */
    load("program Bad;    \nvar answer:integer;   \nbegin answer:=;end.");before=s;
    check(!compile(&s,&b,&d));prior=d.code;allocation=d.start;
    check(!compile_request(&s,0,&b,&failure,0,0,s.path));
    check(failure.diagnostic.code==prior&&failure.diagnostic.start==allocation&&!strcmp(failure.path,s.path)&&failure.has_hash);
    check(!b.valid&&pages_used()==1&&!memcmp(&s,&before,sizeof(s)));
    load("overlay procedure One;begin end;begin One end.");
    check(!compile_request(&s,0,&b,&failure,0,0,s.path)&&failure.diagnostic.code==77&&pages_used()==1);
    disk_sources=disk;disk_source_count=2;
    load("program Include;\n{$I BAD.INC}\nbegin end.");before=s;
    check(!compile_request(&s,0,&b,&failure,0,0,s.path));
    check(failure.diagnostic.code&&failure.diagnostic.code<101&&!strcmp(failure.path,"H:/BAD.INC")&&failure.has_hash);
    check(pages_used()==1&&!b.valid&&!memcmp(&s,&before,sizeof(s)));
    load("program Missing;\n{$I MISSING.INC}\nbegin end.");
    check(!compile_request(&s,0,&b,&failure,0,0,s.path));
    check(failure.diagnostic.code==90&&!strcmp(failure.path,"H:/MISSING.INC")&&!failure.has_hash&&pages_used()==1);
    /* A good disk include is replaced by the unsaved Work input; its error
     * path/hash and input ID survive the failure table. */
    load("program Override;\n{$I GOOD.INC}\nbegin writeln(answer)end.");before=s;
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    check(source_new(&include,"H:/GOOD.INC"));check(source_import(&include,(const uint8_t*)disk[0].text,14,&d));
    source_hash(&include,hash);check(sources_borrow(&buffers,&include,hash)==1);pages=pages_used();
    check(!compile_request(&s,&buffers,&b,&failure,0,0,include.path));
    check(!strcmp(failure.path,include.path)&&failure.has_hash&&!memcmp(failure.hash,hash,32)&&pages_used()==pages);
    check(sources_close(&buffers));source_close(&include);check(pages_used()==1);
    load("program Include;\n{$I GOOD.INC}\nbegin writeln(answer)end.");
    check(compile_request(&s,0,&b,&failure,0,0,s.path));
    check(b.sources.count==2&&sources_get(&b.sources,1,&include)&&include.page==255&&!sources_resident(&b.sources,1));
    check(build_check(&s,&b));build_close(&b);check(pages_used()==1);
    disk_sources=0;disk_source_count=0;
    load("program Cancel;var n:integer;begin n:=7;writeln(n)end.");before=s;
    for(stage=1;stage<=4;++stage)for(reason=1;reason<=2;++reason) {
        cancel_stage=stage;cancel_result=reason;poll_calls=0;
        check(!compile_request(&s,0,&b,&failure,0,0,s.path));
        check(failure.diagnostic.code==COMPILE_INTERRUPTED&&failure.interrupted==reason&&pages_used()==1&&!b.valid);
        check(!memcmp(&before,&s,sizeof(s)));cancel_stage=0;
    }
    /* Fail each allocation across parent preparation, compiler, reply export
     * and adoption. All live ownership returns to the original source. */
    for(allocation=0;allocation<24;++allocation) {
        fail_after=(int)allocation;
        if(compile_request(&s,0,&b,&failure,0,0,s.path))build_close(&b);
        else check(failure.diagnostic.code==COMPILE_PLATFORM_FAILURE||failure.diagnostic.code==98);
        fail_after=-1;check(!b.valid&&pages_used()==1&&job_zero(compiler_owned,8)&&job_zero(parent_pending,8)&&compiler_allocating==COMPILER_ALLOC_NONE);
        check(!memcmp(&before,&s,sizeof(s)));
    }
    /* Retain failed releases explicitly; a later invocation retries them. */
    prior=32;while(page_role(prior))++prior;fail_drop=prior;
    check(!compile_request(&s,0,&b,&failure,0,0,s.path)&&failure.diagnostic.code==COMPILE_PLATFORM_FAILURE);
    check(page_role(prior)&&!job_zero(parent_pending,8));fail_drop=-1;
    check(compile_request(&s,0,&b,&failure,0,0,s.path));build_close(&b);check(pages_used()==1);
    {
        CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};
        check(compile_request(&s,0,&b,&failure,1,&options,s.path)&&host_prepared);
        check(host_prepares==prepares+1);build_close(&b);check(cancel_prepared_build());
        load("begin wrong end.");
        check(!compile_request(&s,0,&b,&failure,1,&options,s.path)&&!host_prepared);
        check(host_prepares==prepares+2&&host_cancels==cancels+2&&pages_used()==1);
        check(!compile(&s,&b,&d));prior=d.code;
        host_cancel_failure=1;
        check(!compile_request(&s,0,&b,&failure,1,&options,s.path));
        check(failure.diagnostic.code==prior&&failure.diagnostic.start==d.start&&!strcmp(failure.path,s.path)&&failure.has_hash&&!host_prepared&&pages_used()==1);
        host_cancel_failure=0;
        load("begin end.");check(compile_request(&s,0,&b,&failure,1,&options,s.path));
        build_close(&b);check(cancel_prepared_build()&&pages_used()==1);
    }
}
