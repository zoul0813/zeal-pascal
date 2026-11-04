/* Exercise the synchronous capture/result/cleanup implementation directly. */
#include "../../src/compiler_job.c"
static uint8_t mask_empty(const uint8_t *p,uint16_t n) {while(n--)if(*p++)return 0;return 1;}
static uint8_t host_prepared,host_cancel_failure;
static unsigned host_prepares,host_cancels;
uint8_t prepare_build(const Source *root,uint8_t chain,uint8_t polling) {
    (void)root;(void)chain;(void)polling;
    assert(!host_prepared);host_prepared=1;++host_prepares;return 1;
}
uint8_t cancel_prepared_build(void) {
    if(!host_prepared)return 1;
    ++host_cancels;host_prepared=0;
    if(host_cancel_failure) {platform_error=ERR_FAILURE;return 0;}
    return 1;
}
uint8_t output_interrupt_reason(void) {return 0;}

static void capture_cases(void) {
    static const char *texts[]={"", "begin end.\n", "begin  \nend.  ", " \n  ", "x\ry", "x\032y"};
    Source original,before,captured;uint8_t i;unsigned prior;
    for(i=0;i<sizeof(texts)/sizeof(texts[0]);++i) {
        prior=pages_used();check(source_new(&original,"H:/CAPTURE.PAS"));
        original.length=(uint16_t)strlen(texts[i]);
        for(uint16_t j=0;j<original.length;++j)far_set(original.page,j,texts[i][j]);
        before=original;compiler_allocating=COMPILER_ALLOC_PARENT;
        check(capture_source(&original,&captured));
        check(captured.page==original.page);
        check(!memcmp(&original,&before,sizeof(original)));
        check(compiler_resources_ready());source_close(&original);check(pages_used()==prior);
    }
}
/* Host-only producer oracle: mirror the external reader's entry rules. */
static uint32_t map_number(const Build *result,uint16_t at,uint8_t n) {
    uint32_t value=0;uint8_t i;
    for(i=0;i<n;++i)value|=(uint32_t)far_get(result->map_page,at+i)<<(8*i);
    return value;
}
static void generated_map_cases(void) {
    const char *programs[]={"begin end.",
        "var i:integer;begin for i:=1 to 4 do case i of 1:writeln(i);2,3:writeln(0) end end.",
        "var r:record n:integer end;procedure p;begin with r do n:=1 end;begin p end.",
        "{$I PART.INC}\nbegin writeln(answer) end."};
    HostSource disk[]={{"H:/PART.INC","const answer=42;"}};
    uint16_t at,previous,first,last,file;uint32_t start;Source source;unsigned i;
    disk_sources=disk;disk_source_count=1;
    for(i=0;i<sizeof(programs)/sizeof(programs[0]);++i) {
        load(programs[i]);check(compile(&s,&b,&d));
        at=0;previous=0;
        check(b.map_length%MAP_RECORD_BYTES==0);
        for(;at<b.map_length;at+=MAP_RECORD_BYTES) {
            first=map_number(&b,at,2);last=map_number(&b,at+2,2);file=map_number(&b,at+4,2);
            start=map_number(&b,at+6,2);
            check(first>=previous&&first<last&&last<=b.code_bytes);
            check(file<b.sources.count);
            check(sources_get(&b.sources,file,&source)&&start<=source.length);
            previous=last;
        }
        build_close(&b);check(pages_used()==1);
    }
    disk_sources=0;disk_source_count=0;
}
static void large_job_capacity(void) {
    char text[4096];FILE *file;size_t length;uint8_t held[23],i,expected[32],actual[32];unsigned required,allocation;
    Source before;uint32_t version;CompileFailure failure;
    CompileOptions options={0x8200,0xfbffu,COMPILE_PROGRAM};
    file=fopen("tests/native/chain-call.pas","rb");check(file!=0);
    length=fread(text,1,sizeof(text),file);check(!ferror(file)&&length<sizeof(text));check(!fclose(file));
    check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));before=s;version=page_versions[s.page];
    check(compile_project(&s,&b,&d,0,&options));test_image_hash(&b,expected);build_close(&b);
    for(i=0;i<23;++i){held[i]=page_new(PAGE_RUN);check(held[i]!=255);}
    /* Exactly nine pages remain, including suspended Work,
     * just as in the sixteen-bank IDE. Match the direct compiler's bytes. */
    allocation_calls=0;check(compile_request(&s,&b,&failure,1,&options));required=allocation_calls;
    check(b.valid==2&&b.pages[0]==255&&b.pages[1]==255&&b.pages[2]==255&&b.code_bytes>PAGE_SIZE&&build_check(&s,&b));
    test_image_hash(&b,actual);check(!memcmp(expected,actual,32));
    check(!memcmp(&s,&before,sizeof(s)));build_close(&b);check(cancel_prepared_build());
    /* F uses the same disk backend without preparing/publishing output. */
    compiler_disk_requested=1;check(compile_project(&s,&b,&d,1,&options));compiler_disk_requested=0;
    {uint16_t position;check(b.valid==2&&!host_prepared);
     check(build_locate(&s,&b,map_number(&b,0,2),&position));}
    test_image_hash(&b,actual);check(!memcmp(expected,actual,32));
    build_close(&b);check(pages_used()==24);
    for(allocation=0;allocation<required;++allocation) {
        fail_after=(int)allocation;
        check(!compile_request(&s,&b,&failure,1,&options));fail_after=-1;
        check(pages_used()==24&&!b.valid&&!host_prepared&&!memcmp(&s,&before,sizeof(s)));
        check(mask_empty(compiler_owned,8)&&mask_empty(compiler_parent_owned,8)&&page_versions[s.page]==version);
    }
    cancel_stage=7;cancel_result=1;
    check(!compile_request(&s,&b,&failure,1,&options));cancel_stage=0;
    check(failure.diagnostic.code==COMPILE_INTERRUPTED&&pages_used()==24&&!b.valid&&!host_prepared);
    check(!memcmp(&s,&before,sizeof(s))&&page_versions[s.page]==version);
    check(page_versions[s.page]==version);
    for(i=0;i<23;++i)check(page_drop(held[i]));
    check(pages_used()==1);
}
/* Independent interval counting selects the first read of each of the six
 * worker/parent hashes, without coupling fault injection to implementation. */
/* No stream reader belongs to the synchronous successful result any more.
 * A poisoned reader proves the path does not silently retain any of the six
 * old raw/normalized image/map scans. Digest helper vectors remain elsewhere. */
static void stream_hash_failures(void) {
    Source before;CompileFailure failure;
    load("begin end.");before=s;stream_copy_calls=0;stream_read_fail_after=0;
    check(compile_request(&s,&b,&failure,0,0));
    check(!stream_copy_calls&&b.valid);stream_read_fail_after=-1;
    check(build_check(&s,&b));build_close(&b);check(pages_used()==1&&!memcmp(&s,&before,sizeof(s)));
    check(mask_empty(compiler_owned,8)&&mask_empty(compiler_parent_owned,8)&&compiler_allocating==COMPILER_ALLOC_NONE);
}
/* Worker initialization failures must retain the reply/parent cleanup path. */
static void image_fill_failures(void) {
    static const unsigned failures[]={0,1,32,64,65,96,128};
    Source before;CompileFailure failure;unsigned i;uint8_t hash[32];
    load("program Init;var pad:array[1..193]of byte;n:integer;const k:integer=17;begin n:=k;writeln(n)end.");before=s;
    test_source_hash(&s,hash);
    image_fill_total_calls=0;
    check(compile_request(&s,&b,&failure,0,0));check(image_fill_total_calls==129);
    build_close(&b);check(pages_used()==1&&!memcmp(&s,&before,sizeof(s)));
    for(i=0;i<sizeof(failures)/sizeof(failures[0]);++i) {
        image_fill_total_calls=0;image_fill_fail_after=(int)failures[i];
        check(!compile_request(&s,&b,&failure,0,0));image_fill_fail_after=-1;
        check(image_fill_total_calls==failures[i]+1);
        check(failure.diagnostic.code==99&&!b.valid&&!host_prepared&&pages_used()==1);
        check(!strcmp(failure.path,s.path));
        check(!memcmp(&s,&before,sizeof(s))&&mask_empty(compiler_owned,8)&&mask_empty(compiler_parent_owned,8)&&mask_empty(parent_pending,8));
        check(compiler_allocating==COMPILER_ALLOC_NONE);
    }
}
/* Saved compilation must produce exactly the same executable layout as the
 * in-memory emitter, including code emitted before later global declarations. */
static void disk_compiler_cases(void) {
    static const struct {const char *text;uint16_t start;uint8_t wide;} programs[]={
        {"var n:integer;const k:integer=17;procedure first;begin n:=k end;\n"
         "var later:array[1..513]of byte;begin first;later[513]:=42;writeln(n,later[513])end.",0,0},
        {"procedure first;begin end;var a:array[1..20000]of byte;\n"
         "begin first;a[20000]:=7;writeln(a[20000])end.",0,1},
        {"const data:array[1..3]of integer=(17,19,23);procedure first;begin writeln(data[3])end;\n"
         "var later:array[1..193]of byte;begin first;later[193]:=1 end.",0x9000,0}
    };
    static const char fault_program[]=
        "var n:integer;const k:integer=17;procedure first;begin n:=k end;\n"
        "var later:array[1..513]of byte;begin first;later[513]:=42;writeln(n,later[513])end.";
    CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};CompileFailure failure;Source before;
    uint8_t expected[32],actual[32],kind,quirk;unsigned fixture,operation,attempt,total[CD_OPERATIONS],nth;
    uint16_t i,length,origin,globals;uint32_t version;
    /* Placement capacity errors must remain errors without keyboard polling. */
    {
        CompilerSources sources;memset(&sources,0,sizeof(sources));
        load("begin end.");memset(&cc,0,sizeof(cc));memset(&b,0,sizeof(b));memset(&d,0,sizeof(d));
        cc.image=&b;cc.error=&d;cc.sources=&sources;
        check(cc_disk_begin(&b,&s,0));b.globals_bytes=CC_STATIC_LIMIT-0x4000u;
        cc_disk_place(&b,0x8000u,0);check(d.code==99&&!compiler_disk_placed);
        cc_disk_discard(&b);check(!compiler_spool.exists&&!compiler_spool.opened);
        compiler_sdk_reset();
    }
    for(fixture=0;fixture<sizeof(programs)/sizeof(programs[0]);++fixture)for(kind=0;kind<2;++kind) {
        load(programs[fixture].text);before=s;version=page_versions[s.page];
        options.start=programs[fixture].start;options.kind=kind?COMPILE_CHAIN:COMPILE_PROGRAM;
        check(compile_project(&s,&b,&d,0,&options));test_image_hash(&b,expected);
        length=b.length;origin=b.code_origin;globals=b.globals_bytes;
        if(!kind) {
            if(programs[fixture].wide) {
                check(globals==20000);
                for(i=0;i<20000;++i)check(image_get(&b,GLOBAL_BASE-0x4000u+i)==0);
            }
            for(i=GLOBAL_BASE+globals;i<origin;++i)check(image_get(&b,i-0x4000u)==0);
        }
        build_close(&b);
        for(quirk=CD_QUIRK_NONE;quirk<=CD_QUIRK_COMMITTED;++quirk) {
            compiler_sdk_reset();compiler_overwrite_mode=quirk;
            check(compile_request(&s,&b,&failure,kind?2:1,&options));
            check(b.valid==2&&b.pages[0]==255&&b.pages[1]==255&&b.pages[2]==255&&
                  b.length==length&&b.code_origin==origin&&b.globals_bytes==globals&&build_check(&s,&b));
            test_image_hash(&b,actual);check(!memcmp(expected,actual,32));
            if(quirk)check(compiler_overwrite_reports>0&&
                compiler_overwrite_readbacks==compiler_overwrite_reports&&!compiler_overwrite_pending);
            check(!memcmp(&s,&before,sizeof(s))&&page_versions[s.page]==version);
            build_close(&b);check(cancel_prepared_build());
            check(!compiler_spool.exists&&!compiler_spool.opened&&pages_used()==1);
        }
    }
    options.start=0;options.kind=COMPILE_PROGRAM;load(fault_program);before=s;version=page_versions[s.page];
    compiler_sdk_reset();check(compile_request(&s,&b,&failure,1,&options));
    memcpy(total,compiler_sdk_calls,sizeof(total));build_close(&b);check(cancel_prepared_build());
    /* Fail the first SDK boundary and a later transfer boundary. Each failed
     * compile owns its partial scratch and releases it without changing Work. */
    for(operation=CD_STAT;operation<=CD_WRITE;++operation) {
        check(total[operation]>0);
        for(attempt=0;attempt<(total[operation]>2?2u:1u);++attempt) {
            nth=attempt?total[operation]/2:1;
            compiler_sdk_reset();compiler_sdk_fail_kind=(uint8_t)operation;compiler_sdk_fail_at=nth;
            check(!compile_request(&s,&b,&failure,1,&options));
            check(!compiler_sdk_fail_at&&failure.diagnostic.code==COMPILE_PLATFORM_FAILURE&&!b.valid&&!host_prepared);
            check(!compiler_spool.exists&&!compiler_spool.opened&&pages_used()==1&&
                  mask_empty(compiler_owned,8)&&mask_empty(compiler_parent_owned,8));
            check(!memcmp(&s,&before,sizeof(s))&&page_versions[s.page]==version);
            compiler_sdk_reset();check(compile_request(&s,&b,&failure,1,&options));
            build_close(&b);check(cancel_prepared_build());
            check(!compiler_spool.exists&&!compiler_spool.opened&&pages_used()==1);
        }
    }
    /* An undocumented status alone is insufficient: corrupt or truncated
     * sectors must fail. Native pfree clears A on success; those releases must
     * not erase the original write error reported to the caller. */
    for(kind=0;kind<2;++kind)for(quirk=CD_QUIRK_CORRUPT;quirk<=CD_QUIRK_TRUNCATE;++quirk) {
        options.kind=kind?COMPILE_CHAIN:COMPILE_PROGRAM;
        compiler_sdk_reset();compiler_overwrite_mode=quirk;cleared_drop_errors=0;clear_error_on_drop=1;
        check(!compile_request(&s,&b,&failure,kind?2:1,&options));clear_error_on_drop=0;
        check(cleared_drop_errors>0&&compiler_overwrite_reports>0&&!compiler_overwrite_readbacks&&
              compiler_overwrite_reported>=0x40u&&compiler_overwrite_reported<0xc0u&&
              platform_error==compiler_overwrite_reported&&failure.diagnostic.code==COMPILE_PLATFORM_FAILURE);
        check(!b.valid&&!host_prepared&&!compiler_spool.exists&&!compiler_spool.opened&&pages_used()==1&&
              mask_empty(compiler_owned,8)&&mask_empty(compiler_parent_owned,8));
        check(!memcmp(&s,&before,sizeof(s))&&page_versions[s.page]==version);
        compiler_sdk_reset();check(compile_request(&s,&b,&failure,kind?2:1,&options));
        build_close(&b);check(cancel_prepared_build());
        check(!compiler_spool.exists&&!compiler_spool.opened&&pages_used()==1);
    }
    /* A documented SDK failure must remain a failure even with native cleanup
     * clearing the status register. It is never a candidate for readback repair. */
    options.kind=COMPILE_PROGRAM;compiler_sdk_reset();
    compiler_sdk_fail_kind=CD_WRITE;compiler_sdk_fail_at=total[CD_WRITE]/2;cleared_drop_errors=0;clear_error_on_drop=1;
    check(!compile_request(&s,&b,&failure,1,&options));clear_error_on_drop=0;
    check(cleared_drop_errors>0&&!compiler_sdk_fail_at&&failure.diagnostic.code==COMPILE_PLATFORM_FAILURE&&platform_error==ERR_FAILURE&&
          !compiler_overwrite_reports&&!b.valid&&!host_prepared&&!compiler_spool.exists&&!compiler_spool.opened&&pages_used()==1);
    compiler_sdk_reset();
    /* A foreign collision must never become our scratch owner. */
    compiler_spool.exists=1;compiler_spool.length=7;memcpy(compiler_spool.bytes,"foreign",7);
    compiler_sdk_reset();check(!compile_request(&s,&b,&failure,1,&options));
    check(failure.diagnostic.code==COMPILE_PLATFORM_FAILURE&&platform_error==ERR_ALREADY_EXIST&&
          compiler_spool.exists&&!compiler_spool.opened&&compiler_spool.length==7&&
          !memcmp(compiler_spool.bytes,"foreign",7)&&!compiler_sdk_calls[CD_OPEN]&&!compiler_sdk_calls[CD_REMOVE]);
    check(pages_used()==1&&!host_prepared&&!memcmp(&s,&before,sizeof(s)));
    compiler_spool.exists=0;compiler_sdk_reset();check(compile_request(&s,&b,&failure,1,&options));
    /* Failed removal retains only our own pathname. The next compile retries
     * that cleanup before acquiring another scratch file. */
    compiler_sdk_reset();compiler_sdk_fail_kind=CD_REMOVE;compiler_sdk_fail_at=1;build_close(&b);
    check(!compiler_sdk_fail_at&&compiler_spool.exists&&!compiler_spool.opened&&platform_error==ERR_FAILURE);
    check(cancel_prepared_build());compiler_sdk_reset();
    check(compile_request(&s,&b,&failure,1,&options));
    check(compiler_sdk_calls[CD_REMOVE]==1);build_close(&b);check(cancel_prepared_build());
    check(!compiler_spool.exists&&!compiler_spool.opened&&pages_used()==1&&
          !memcmp(&s,&before,sizeof(s))&&page_versions[s.page]==version);
    compiler_sdk_reset();
    /* Fail the post-compile release of a disk-loaded Main, after ownership
     * checks and table cleanup. The dead local Build must not own the fd. */
    {
        static const HostSource root_file[]={{"H:/LOOKUP.PAS","begin writeln(7)end."}};
        Source root;memset(&root,0,sizeof(root));root.page=255;strcpy(root.path,root_file[0].path);
        disk_sources=root_file;disk_source_count=1;fail_final_disk_drop=1;
        check(!compile_request(&root,&b,&failure,1,&options));
        check(!fail_final_disk_drop&&failure.diagnostic.code==COMPILE_PLATFORM_FAILURE&&!b.valid);
        check(!compiler_spool.exists&&!compiler_spool.opened&&pages_used()==1);
        check(compile_request(&root,&b,&failure,1,&options));build_close(&b);check(cancel_prepared_build());
        check(!compiler_spool.exists&&!compiler_spool.opened&&pages_used()==1);
        disk_sources=0;disk_source_count=0;
    }
}

static void compiler_job_cases(void) {
    CompileFailure failure;Source before,include;uint8_t expected[32],actual[32],prior;
    unsigned allocation,prepares=host_prepares,cancels=host_cancels;uint8_t stage,reason;
    HostSource disk[]={ {"H:/BAD.INC","const answer=;\n"},{"H:/GOOD.INC","const answer=42;\n"} };
    load("program Hello;begin writeln('Hello, Zeal!')end.");s.cursor=11;s.previous=4;s.dirty=1;before=s;
    check(compile(&s,&b,&d));test_image_hash(&b,expected);build_close(&b);
    check(compile_request(&s,&b,&failure,0,0));
    check(b.valid&&!memcmp(&s,&before,sizeof(s))&&!host_prepared);
    test_image_hash(&b,actual);check(!memcmp(actual,expected,32));
    check(build_check(&s,&b)&&sources_get(&b.sources,0,&include)&&include.page==s.page);build_close(&b);check(pages_used()==1);
    generated_map_cases();
    stream_hash_failures();image_fill_failures();
    check(host_prepares==prepares&&host_cancels==cancels);
    strcpy(s.path,"H:/bad//name.pas");
    check(!compile_request(&s,&b,&failure,0,0)&&failure.diagnostic.code==COMPILE_PLATFORM_FAILURE);
    check(platform_error==ERR_FAILURE&&pages_used()==1);strcpy(s.path,"H:/SLICE.PAS");
    /* Canonical failure positions must rebase to the dirty editor, not to a
     * worker-global filename or an offset into its trimmed input. */
    load("program Bad;    \nvar answer:integer;   \nbegin answer:=;end.");before=s;
    check(!compile(&s,&b,&d));prior=d.code;allocation=d.start;
    check(!compile_request(&s,&b,&failure,0,0));
    check(failure.diagnostic.code==prior&&failure.diagnostic.start==allocation&&!strcmp(failure.path,s.path));
    check(!b.valid&&pages_used()==1&&!memcmp(&s,&before,sizeof(s)));
    load("overlay procedure One;begin end;begin One end.");
    check(!compile_request(&s,&b,&failure,0,0)&&failure.diagnostic.code==77&&pages_used()==1);
    {CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};uint8_t mode;
     for(mode=1;mode<=2;++mode) {
        options.kind=mode==2?COMPILE_CHAIN:COMPILE_PROGRAM;
        check(!compile_request(&s,&b,&failure,mode,&options)&&failure.diagnostic.code==12);
        check(pages_used()==1&&!host_prepared&&!compiler_spool.exists&&!compiler_spool.opened);
     }
     prepares=host_prepares;cancels=host_cancels;}
    disk_sources=disk;disk_source_count=2;
    load("program Include;\n{$I BAD.INC}\nbegin end.");before=s;
    check(!compile_request(&s,&b,&failure,0,0));
    check(failure.diagnostic.code&&failure.diagnostic.code<101&&!strcmp(failure.path,"H:/BAD.INC"));
    check(pages_used()==1&&!b.valid&&!memcmp(&s,&before,sizeof(s)));
    load("program Missing;\n{$I MISSING.INC}\nbegin end.");
    check(!compile_request(&s,&b,&failure,0,0));
    check(failure.diagnostic.code==90&&!strcmp(failure.path,"H:/MISSING.INC")&&pages_used()==1);
    load("program Include;\n{$I GOOD.INC}\nbegin writeln(answer)end.");
    check(compile_request(&s,&b,&failure,0,0));
    check(b.sources.count==2&&sources_get(&b.sources,1,&include)&&include.page==255);
    check(build_check(&s,&b));build_close(&b);check(pages_used()==1);
    disk_sources=0;disk_source_count=0;
    load("program Cancel;var n:integer;begin n:=7;writeln(n)end.");before=s;
    for(stage=1;stage<=4;++stage)for(reason=1;reason<=2;++reason) {
        cancel_stage=stage;cancel_result=reason;poll_calls=0;
        check(!compile_request(&s,&b,&failure,0,0));
        check(failure.diagnostic.code==COMPILE_INTERRUPTED&&failure.interrupted==reason&&pages_used()==1&&!b.valid);
        check(!memcmp(&before,&s,sizeof(s)));cancel_stage=0;
    }
    /* Fail each allocation across parent preparation, compiler, reply export
     * and adoption. All live ownership returns to the original source. */
    for(allocation=0;allocation<24;++allocation) {
        fail_after=(int)allocation;
        if(compile_request(&s,&b,&failure,0,0))build_close(&b);
        else check(failure.diagnostic.code==COMPILE_PLATFORM_FAILURE||failure.diagnostic.code==99);
        fail_after=-1;check(!b.valid&&pages_used()==1&&mask_empty(compiler_owned,8)&&mask_empty(parent_pending,8)&&compiler_allocating==COMPILER_ALLOC_NONE);
        check(!memcmp(&before,&s,sizeof(s)));
    }
    /* Retain failed releases explicitly; a later invocation retries them. */
    load("begin wrong end.");
    prior=32;while(page_role(prior))++prior;fail_drop=prior;
    check(!compile_request(&s,&b,&failure,0,0)&&failure.diagnostic.code==COMPILE_PLATFORM_FAILURE);
    check(page_role(prior)&&(!mask_empty(parent_pending,8)||!mask_empty(compiler_owned,8)));fail_drop=-1;
    load("begin end.");
    check(compile_request(&s,&b,&failure,0,0));build_close(&b);check(pages_used()==1);
    {
        CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};
        check(compile_request(&s,&b,&failure,1,&options)&&host_prepared);
        check(host_prepares==prepares+1);build_close(&b);check(cancel_prepared_build());
        load("begin wrong end.");
        check(!compile_request(&s,&b,&failure,1,&options)&&!host_prepared);
        check(host_prepares==prepares+2&&host_cancels==cancels+2&&pages_used()==1);
        check(!compile(&s,&b,&d));prior=d.code;
        host_cancel_failure=1;
        check(!compile_request(&s,&b,&failure,1,&options));
        check(failure.diagnostic.code==prior&&failure.diagnostic.start==d.start&&!strcmp(failure.path,s.path)&&!host_prepared&&pages_used()==1);
        host_cancel_failure=0;
        load("begin end.");check(compile_request(&s,&b,&failure,1,&options));
        build_close(&b);check(cancel_prepared_build()&&pages_used()==1);
    }
    disk_compiler_cases();
}
