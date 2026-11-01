/* Exercise the synchronous capture/result/cleanup implementation directly. */
#include "../../src/compiler_job.c"
#include "../../src/sources_internal.h"
static uint8_t mask_empty(const uint8_t *p,uint16_t n) {while(n--)if(*p++)return 0;return 1;}
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

static void capture_cases(void) {
    static const char *texts[]={"", "begin end.\n", "begin  \nend.  ", " \n  ", "x\ry", "x\032y"};
    uint8_t i,borrow,expected[32],actual[32];Source original,before,captured;Sources inputs;unsigned prior;
    for(i=0;i<sizeof(texts)/sizeof(texts[0]);++i)for(borrow=0;borrow<2;++borrow) {
        prior=pages_used();check(source_new(&original,"H:/CAPTURE.PAS"));
        original.length=(uint16_t)strlen(texts[i]);
        for(uint16_t j=0;j<original.length;++j)far_set(original.page,j,texts[i][j]);
        original.revision=17;original.dirty=1;before=original;
        memset(input_pages,0,8);compiler_allocating=COMPILER_ALLOC_PARENT;check(sources_new(&inputs));
        if(i>=4) {
            check(!capture_source(&inputs,&original,borrow)&&!inputs.count);
            check(!memcmp(&original,&before,sizeof(original)));
            check(compiler_resources_ready());source_close(&original);check(pages_used()==prior);continue;
        }
        check(capture_source(&inputs,&original,borrow));
        check(sources_get(&inputs,0,&captured)&&sources_hash(&inputs,0,actual));
        source_hash(&captured,expected);check(!memcmp(actual,expected,32));
        check(captured.revision==17&&!memcmp(&original,&before,sizeof(original)));
        if(borrow&&i<2)check(captured.page==original.page);
        else check(captured.page!=original.page);
        check(!capture_source(&inputs,&original,borrow)); /* duplicate source page */
        check(compiler_resources_ready());source_close(&original);check(pages_used()==prior);
    }
    {
        uint32_t versions[32],lifetimes[32];uint8_t page;
        load("begin end.");
        for(i=0;i<32;++i){versions[i]=page_versions[32+i];lifetimes[i]=page_lifetime(32+i);}
        compiler_allocating=COMPILER_ALLOC_PARENT;check(prepare_inputs(&inputs,&s,0,s.path));
        check(inputs_unchanged(versions,lifetimes));page=s.page;
        far_set(page,0,far_get(page,0));check(!inputs_unchanged(versions,lifetimes));
        versions[page-32]=page_versions[page];check(inputs_unchanged(versions,lifetimes));
        ++lifetimes[page-32];check(!inputs_unchanged(versions,lifetimes));
        check(compiler_resources_ready()&&pages_used()==1);
    }
}
static void capture_boundaries(void) {
    Source original,before,captured;Sources inputs;uint8_t may_borrow,expected[32],actual[32];
    uint16_t length,j;unsigned long reads;char text[16000];
    for(length=127;length<=128;++length) {
        check(source_new(&original,"H:/BOUND.PAS"));original.length=length;
        for(j=0;j<length;++j)far_set(original.page,j,'x');before=original;
        compiler_allocating=COMPILER_ALLOC_PARENT;memset(input_pages,0,8);check(sources_new(&inputs));
        capture_probe_page=original.page;capture_reads=0;capture_probe=1;
        if(length==127)check(capture_source(&inputs,&original,1));
        else check(!capture_source(&inputs,&original,1)&&!inputs.count);
        capture_probe=0;check(!memcmp(&original,&before,sizeof(original)));
        printf("DIRECT capture boundary=%u reads=%lu\n",length,capture_reads);
        check(compiler_resources_ready());source_close(&original);check(pages_used()==1);
    }
    for(may_borrow=0;may_borrow<2;++may_borrow) {
        check(source_new(&original,"H:/FAIL.PAS"));
        check(source_replace(&original,0,0,(const uint8_t*)"x  ",3));before=original;
        check(sources_new(&inputs));compiler_allocating=COMPILER_ALLOC_PARENT;memset(input_pages,0,8);
        fail_after=0;check(!capture_source(&inputs,&original,may_borrow));fail_after=-1;
        check(!inputs.count&&mask_empty(parent_pending,8)&&!memcmp(&original,&before,sizeof(original)));
        check(!memcmp(memory[original.page],"x  ",3));compiler_allocating=COMPILER_ALLOC_NONE;
        check(sources_close(&inputs));source_close(&original);check(pages_used()==1);
    }
    for(j=0;j<sizeof(text);++j)text[j]=j%100==99?'\n':'x';
    for(may_borrow=0;may_borrow<2;++may_borrow) {
        check(source_new(&original,"H:/WIDE.PAS"));check(source_replace(&original,0,0,(const uint8_t*)text,sizeof(text)));
        source_hash(&original,expected);compiler_allocating=COMPILER_ALLOC_PARENT;memset(input_pages,0,8);check(sources_new(&inputs));
        capture_probe_page=original.page;capture_reads=0;capture_probe=1;check(capture_source(&inputs,&original,may_borrow));
        capture_probe=0;reads=capture_reads;
        check(sources_get(&inputs,0,&captured)&&sources_hash(&inputs,0,actual)&&!memcmp(actual,expected,32));
        check(reads==(may_borrow?16000:32000));
        printf("DIRECT capture bytes=16000 borrow=%u reads=%lu\n",may_borrow,reads);
        check(compiler_resources_ready());source_close(&original);check(pages_used()==1);
    }
}
static void retained_source_cases(void) {
    CompileFailure failure;Source before,part,reference,loaded;Sources buffers;uint8_t hash[32],id;unsigned pages,required,a;char path[128];
    HostSource disk[]={{"H:/MAIN.PAS","begin end."},{"H:/PART.INC","const answer=;"}};
    load("begin end.");check(source_replace(&s,s.length,s.length,(const uint8_t*)"  ",2));before=s;
    allocation_calls=0;check(compile_request(&s,0,&b,&failure,0,0,s.path));required=allocation_calls;
    check(!sources_borrowed(&b.sources,0)&&sources_origin(&b.sources,0)==s.page&&build_check(&s,&b));
    check(!memcmp(&s,&before,sizeof(s)));far_set(s.page,0,far_get(s.page,0));check(!build_ready(&s,&b));build_close(&b);check(pages_used()==1);
    for(a=0;a<required;++a) {
        fail_after=(int)a;
        if(compile_request(&s,0,&b,&failure,0,0,s.path))build_close(&b);
        fail_after=-1;check(!b.valid&&pages_used()==1&&!memcmp(&s,&before,sizeof(s)));
        check(mask_empty(compiler_owned,8)&&mask_empty(parent_pending,8));
    }
    load("program Override;\n{$I PART.INC}\nbegin writeln(answer)end.");
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    check(source_new(&part,"H:/PART.INC"));check(source_import(&part,(const uint8_t*)"const answer=42;",16,&d));
    check(source_replace(&part,part.length,part.length,(const uint8_t*)"  ",2));before=part;
    source_hash(&part,hash);check(sources_borrow(&buffers,&part,hash)==1);pages=pages_used();
    check(compile_request(&s,&buffers,&b,&failure,0,0,part.path));id=(uint8_t)sources_find(&b.sources,part.path);
    check(!sources_borrowed(&b.sources,id)&&sources_origin(&b.sources,id)==part.page&&build_check(&s,&b));
    check(!memcmp(&part,&before,sizeof(part)));far_set(part.page,0,far_get(part.page,0));check(!build_ready(&s,&b));
    build_close(&b);check(pages_used()==pages);check(sources_close(&buffers));source_close(&part);check(pages_used()==1);
    disk_sources=disk;disk_source_count=2;strcpy(path,s.path);strcpy(s.path,"H:/MAIN.PAS");
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    memset(&reference,0,sizeof(reference));reference.page=255;strcpy(reference.path,s.path);pages=pages_used();
    check(compile_request(&reference,&buffers,&b,&failure,0,0,s.path));
    check(sources_get(&b.sources,0,&loaded)&&!sources_borrowed(&b.sources,0)&&sources_origin(&b.sources,0)==255);
    far_set(s.page,0,far_get(s.page,0));check(build_ready(&loaded,&b));build_close(&b);check(pages_used()==pages);
    check(sources_close(&buffers));strcpy(s.path,path);
    load("program Error;\n{$I PART.INC}\nbegin end.");check(!compile(&s,&b,&d));
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    memset(&reference,0,sizeof(reference));reference.page=255;strcpy(reference.path,"H:/PART.INC");memset(hash,0,32);
    check(sources_reference(&buffers,&reference,hash)==1);
    check(!compile_request(&s,&buffers,&b,&failure,0,0,s.path));
    check(failure.diagnostic.start==d.start&&failure.diagnostic.code==d.code&&failure.has_hash&&!strcmp(failure.path,reference.path));
    check(sources_close(&buffers)&&pages_used()==1);disk_sources=0;disk_source_count=0;
}
static void direct_input_cases(void) {
    CompileFailure failure;CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};Source before;
    load("begin end.");before=s;
    check(!compile_request(&s,0,&b,&failure,3,0,s.path)&&failure.diagnostic.code==COMPILE_PLATFORM_FAILURE);
    check(!compile_request(&s,0,&b,&failure,0,0,"H:/ABSENT.PAS"));
    check(!compile_request(&s,0,&b,&failure,2,0,s.path));
    options.start=0x7fff;check(!compile_request(&s,0,&b,&failure,0,&options,s.path));
    options.start=0;options.end=0xfc00;check(!compile_request(&s,0,&b,&failure,0,&options,s.path));
    options.end=0xfbff;options.kind=COMPILE_CHAIN;check(!compile_request(&s,0,&b,&failure,0,&options,s.path));
    check(!memcmp(&s,&before,sizeof(s))&&pages_used()==1&&!b.valid);
    check(mask_empty(compiler_owned,8)&&mask_empty(compiler_parent_owned,8));
    {
        Sources inputs;Source captured;uint8_t hash[32];char text[16000];unsigned i;
        for(i=0;i<sizeof(text);++i)text[i]=i%100==99?'\n':'x';
        check(source_replace(&s,0,s.length,(const uint8_t*)text,sizeof(text)));
        compiler_allocating=COMPILER_ALLOC_PARENT;check(prepare_inputs(&inputs,&s,0,s.path));
        check(sources_get(&inputs,0,&captured)&&captured.page==s.page&&captured.length==16000);
        check(sources_hash(&inputs,0,hash));check(compiler_resources_ready()&&pages_used()==1);
    }
    load("");check(!compile(&s,&b,&d));
    check(!compile_request(&s,0,&b,&failure,0,0,s.path));
    check(failure.diagnostic.code==d.code&&failure.has_hash&&!strcmp(failure.path,s.path)&&pages_used()==1);
}
/* Captured identities may replace a root-byte hash only at the synchronous
 * handoff. Probe actual source reads, including a normalized snapshot, and
 * reject stale or mismatched descriptors before parsing. */
static void captured_identity_cases(void) {
    Sources inputs;Source captured,active,root;Build result;CompilerSources state;
    Diagnostic diagnostic;uint8_t expected[32],actual[32],kind,page;unsigned before;
    for(kind=0;kind<12;++kind) {
        load(kind==1?"begin  \nend.  ":"begin end.\n");before=pages_used();
        compiler_allocating=COMPILER_ALLOC_PARENT;memset(input_pages,0,8);
        check(sources_new(&inputs)&&capture_source(&inputs,&s,kind<2));
        check(sources_get(&inputs,0,&captured)&&sources_hash(&inputs,0,expected));root=captured;
        if(kind==9) {
            /* Disk Main is loaded after its path-only capture. */
            Source reference=captured;reference.page=255;reference.length=0;
            memset(actual,0,32);check(sources_reference(&inputs,&reference,actual)==0);
        }
        if(kind>=10)inputs.count=0; /* Empty sets still need catalogue validation. */
        if(kind==11)inputs.page=255;
        if(kind==2)++root.length;
        if(kind==3)root.page=s.page; /* Snapshot and editor are different pages. */
        if(kind==4)strcpy(root.path,"H:/ABSENT.PAS");
        /* These three pin the existing readiness checks, before digest reuse. */
        if(kind==5)far_set(captured.page,0,far_get(captured.page,0)^1);
        if(kind==6)far_set(inputs.page,0,far_get(inputs.page,0)); /* stale catalogue */
        if(kind==7) {
            check(page_drop(captured.page));page=page_new(PAGE_SNAPSHOT);
            check(page==captured.page); /* same address, different lifetime */
        }
        memset(&cc,0,sizeof(cc));memset(&state,0,sizeof(state));memset(&result,0,sizeof(result));
        memset(&diagnostic,0,sizeof(diagnostic));result.sources.page=255;
        state.borrow_inputs=1;state.refs_page=255;
        cc.sources=&state;cc.input=&active;cc.image=&result;cc.error=&diagnostic;
        compiler_allocating=COMPILER_ALLOC_WORKER;
        capture_probe_page=captured.page;capture_reads=0;capture_probe=1;
        if(kind<2||kind==9)check(cc_sources_prepare(&root,&inputs));
        else check(!cc_sources_prepare(&root,kind==8?0:&inputs)&&diagnostic.code==98);
        capture_probe=0;
        if(kind<2||kind==9) {
            if(kind==9)check(capture_reads==captured.length); /* Fresh disk hash required. */
            else check(!capture_reads); /* Neither borrowed nor normalized root is rescanned. */
            check(sources_hash(&result.sources,0,actual)&&!memcmp(expected,actual,32));
            source_hash(&captured,actual);check(!memcmp(expected,actual,32));
            check(active.page==captured.page&&active.length==captured.length);
        }
        check(compiler_resources_ready()&&pages_used()==before);
    }
}
/* Adoption changes only ownership metadata. Compare its whole record with
 * the legacy hashing transfer, and prove rejection leaves ownership intact. */
static void source_adoption_cases(void) {
    Sources set,before;Source snapshot,origin,reference;uint8_t hash[32],record[RECORD_BYTES],kind,page;
    uint32_t version,source_version;unsigned pages,allocations;const Source *from;
    for(kind=0;kind<2;++kind) {
        load("begin  \nend.  ");pages=pages_used();
        check(source_snapshot(&s,&snapshot)&&sources_new(&set));source_hash(&snapshot,hash);
        check(sources_borrow(&set,&snapshot,hash)==0);from=kind?&s:0;
        source_version=page_versions[snapshot.page];allocations=allocation_calls;
        capture_probe_page=snapshot.page;capture_reads=0;capture_probe=1;
        check(sources_adopt(&set,0,from));capture_probe=0;
        check(!capture_reads&&allocation_calls==allocations&&page_versions[snapshot.page]==source_version);
        check(!sources_borrowed(&set,0)&&sources_ready(&set,0));
        check(sources_origin(&set,0)==(kind?s.page:255));
        memcpy(record,memory[set.page],sizeof(record));
        check(sources_take(&set,&snapshot,from)==0&&!memcmp(record,memory[set.page],sizeof(record)));
        check(sources_adopt(&set,0,from)&&!memcmp(record,memory[set.page],sizeof(record)));
        fail_drop=snapshot.page;check(!sources_close(&set)&&set.count==1&&sources_ready(&set,0));
        fail_drop=-1;check(sources_close(&set)&&pages_used()==pages);
    }
    for(kind=0;kind<9;++kind) {
        load("begin end.\n");pages=pages_used();
        check(source_snapshot(&s,&snapshot)&&sources_new(&set));source_hash(&snapshot,hash);
        check(sources_borrow(&set,&snapshot,hash)==0);origin=s;
        if(kind==0)far_set(snapshot.page,0,far_get(snapshot.page,0));
        if(kind==1)far_set(set.page,0,far_get(set.page,0));
        if(kind==2){check(page_drop(snapshot.page));page=page_new(PAGE_SNAPSHOT);check(page==snapshot.page);}
        if(kind==3)owned[snapshot.page]=PAGE_RUN;
        if(kind==4)origin.page=255;
        if(kind==6){reference=snapshot;reference.page=255;reference.length=0;check(sources_reference(&set,&reference,hash)==0);}
        if(kind==7){check(sources_take(&set,&snapshot,&s)==0&&sources_pack(&set)&&set.base);}
        if(kind==8){check(sources_adopt(&set,0,&s));far_set(s.page,0,far_get(s.page,0));}
        before=set;version=page_versions[set.page];allocations=allocation_calls;
        check(!sources_adopt(&set,kind==5?1:0,&origin));
        check(!memcmp(&set,&before,sizeof(set))&&page_versions[set.page]==version&&allocation_calls==allocations);
        if(kind==3)owned[snapshot.page]=PAGE_SNAPSHOT;
        set.version=page_versions[set.page];check(sources_close(&set));
        if(kind!=7&&kind!=8)source_close(&snapshot);
        check(pages_used()==pages);
    }
}
static void direct_result_cases(void) {
    Sources inputs;Source captured;Build result,candidate;CompileFailure failure;
    CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};uint8_t old,catalog;uint32_t lifetime;unsigned allocations;
    load("begin end.");compiler_allocating=COMPILER_ALLOC_PARENT;
    check(prepare_inputs(&inputs,&s,0,s.path)&&sources_get(&inputs,0,&captured));
    compiler_allocating=COMPILER_ALLOC_WORKER;compiler_invocation=&failure;active_inputs=&inputs;compiler_memory_mode=1;
    check(compile_project(&captured,&inputs,&result,&d,1,&options));compiler_invocation=0;active_inputs=0;
    check(compiler_result_ready(&inputs,&options,0,&result));candidate=result;
    candidate.valid=0;check(!compiler_result_ready(&inputs,&options,0,&candidate));candidate=result;
    candidate.length=95;check(!compiler_result_ready(&inputs,&options,0,&candidate));candidate=result;
    candidate.map_length=PAGE_SIZE+1u;check(!compiler_result_ready(&inputs,&options,0,&candidate));candidate=result;
    candidate.pages[0]=s.page;check(!compiler_result_ready(&inputs,&options,0,&candidate));candidate=result;
    candidate.pages[1]=candidate.pages[0];check(!compiler_result_ready(&inputs,&options,0,&candidate));candidate=result;
    candidate.map_page=candidate.pages[0];check(!compiler_result_ready(&inputs,&options,0,&candidate));candidate=result;
    old=far_get(candidate.map_page,96);far_set(candidate.map_page,96,old^1);
    check(!compiler_result_ready(&inputs,&options,0,&candidate));candidate.versions[3]=page_versions[candidate.map_page];
    check(!compiler_result_ready(&inputs,&options,0,&candidate));far_set(candidate.map_page,96,old);
    candidate.versions[3]=page_versions[candidate.map_page];
    old=far_get(candidate.pages[0],36);far_set(candidate.pages[0],36,old^1);
    candidate.versions[0]=page_versions[candidate.pages[0]];check(!compiler_result_ready(&inputs,&options,0,&candidate));
    far_set(candidate.pages[0],36,old);candidate.versions[0]=page_versions[candidate.pages[0]];
    check(compiler_result_ready(&inputs,&options,0,&candidate));
    catalog=candidate.sources.page;lifetime=page_lifetime(catalog);allocations=allocation_calls;
    check(retain_sources(&s,0,&inputs,&candidate));
    check(candidate.sources.page==catalog&&page_lifetime(catalog)==lifetime&&allocation_calls==allocations);
    old=far_get(candidate.pages[0],160);far_set(candidate.pages[0],160,old^1);
    candidate.versions[0]=page_versions[candidate.pages[0]];
    check(compiler_result_ready(&inputs,&options,0,&candidate));
    check(!retain_sources(&s,0,&inputs,&candidate));
    check(compiler_resources_ready()&&pages_used()==1);
}
/* Entry bytes are sealed by the compiler, not independently re-decoded by
 * the synchronous handoff. Even forged refreshed versions must not bypass
 * the complete image/map digest check before adoption. Exercise every entry
 * field, including bounds, file IDs, reserved flags and source positions. */
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
    uint16_t at,previous,first,last,file;uint32_t start,end;Source source;unsigned i;
    disk_sources=disk;disk_source_count=1;
    for(i=0;i<sizeof(programs)/sizeof(programs[0]);++i) {
        load(programs[i]);check(compile(&s,&b,&d));
        at=96+map_number(&b,84,4);previous=0;
        check(at+16*map_number(&b,16,4)==b.map_length);
        for(;at<b.map_length;at+=16) {
            first=map_number(&b,at,2);last=map_number(&b,at+2,2);file=map_number(&b,at+4,2);
            start=map_number(&b,at+8,4);end=map_number(&b,at+12,4);
            check(first>=previous&&first<last&&last<=b.code_bytes);
            check(file<b.sources.count&&!map_number(&b,at+6,2));
            check(sources_get(&b.sources,file,&source)&&start<=end&&end<=source.length);
            previous=last;
        }
        build_close(&b);check(pages_used()==1);
    }
    disk_sources=0;disk_source_count=0;
}
static void direct_map_integrity_cases(void) {
    Sources inputs,buffers;Source captured,part;uint8_t hash[32];Build result;CompileFailure failure;
    CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};uint8_t mode,i,old;
    uint16_t entries,entry_byte,record,positions[4];unsigned before;
    HostSource disk[]={{"H:/PART.INC","const answer=42;"}};
    disk_sources=disk;disk_source_count=1;
    check(source_new(&part,"H:/PART.INC"));check(source_import(&part,(const uint8_t*)disk[0].text,16,&d));
    check(sources_new(&buffers));source_hash(&part,hash);check(sources_borrow(&buffers,&part,hash)==0);
    for(mode=0;mode<3;++mode) {
        load("{$I PART.INC}\nbegin writeln(answer);writeln(1) end.");before=pages_used();options.kind=mode==2?COMPILE_CHAIN:COMPILE_PROGRAM;
        compiler_allocating=COMPILER_ALLOC_PARENT;
        check(prepare_inputs(&inputs,&s,&buffers,s.path)&&sources_get(&inputs,0,&captured));
        compiler_allocating=COMPILER_ALLOC_WORKER;compiler_invocation=&failure;active_inputs=&inputs;compiler_memory_mode=mode==0;
        check(compile_project(&captured,&inputs,&result,&d,1,&options));compiler_invocation=0;active_inputs=0;
        check(compiler_result_ready(&inputs,&options,mode,&result));
        entries=96u+far_get(result.map_page,84)+((uint16_t)far_get(result.map_page,85)<<8);
        check(entries+32u<=result.map_length&&result.sources.count==2);
        record=96+40+map_number(&result,98,2);
        positions[0]=record;positions[1]=record+40;positions[2]=record+4;positions[3]=record+8;
        for(i=0;i<4;++i) {
            old=far_get(result.map_page,positions[i]);far_set(result.map_page,positions[i],old^1);
            check(!compiler_result_ready(&inputs,&options,mode,&result));
            result.versions[3]=page_versions[result.map_page];
            check(!compiler_result_ready(&inputs,&options,mode,&result));
            far_set(result.map_page,positions[i],old);result.versions[3]=page_versions[result.map_page];
        }
        /* Test every byte of both the first entry and the map's final entry. */
        for(i=0;i<32;++i) {
            entry_byte=(i<16?entries:result.map_length-16)+(i%16);
            old=far_get(result.map_page,entry_byte);far_set(result.map_page,entry_byte,old^1);
            check(!compiler_result_ready(&inputs,&options,mode,&result));
            result.versions[3]=page_versions[result.map_page];
            /* Pins the private handoff boundary, not a security property. */
            check(compiler_result_ready(&inputs,&options,mode,&result));
            check(!retain_sources(&s,&buffers,&inputs,&result));
            far_set(result.map_page,entry_byte,old);result.versions[3]=page_versions[result.map_page];
        }
        /* CHAIN has no manifest field in its image header: the retained map
         * digest must still authenticate the manifest hash stored in ZMAP. */
        if(mode==2) {
            old=far_get(result.map_page,52);far_set(result.map_page,52,old^1);
            result.versions[3]=page_versions[result.map_page];
            /* Pins the private handoff boundary, not a security property. */
            check(compiler_result_ready(&inputs,&options,mode,&result));
            check(!retain_sources(&s,&buffers,&inputs,&result));
            far_set(result.map_page,52,old);result.versions[3]=page_versions[result.map_page];
        }
        check(compiler_result_ready(&inputs,&options,mode,&result)&&retain_sources(&s,&buffers,&inputs,&result));
        check(compiler_resources_ready()&&pages_used()==before);
    }
    check(sources_close(&buffers));source_close(&part);
    disk_sources=0;disk_source_count=0;
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
            check(!memcmp(extra,prior,(i+1)*sizeof(Source))&&mask_empty(compiler_owned,8)&&mask_empty(compiler_parent_owned,8));
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
        check(mask_empty(compiler_owned,8)&&mask_empty(compiler_parent_owned,8)&&sources_ready(&buffers,0));
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
/* No stream reader belongs to the synchronous successful result any more.
 * A poisoned reader proves the path does not silently retain any of the six
 * old raw/normalized image/map scans. Digest helper vectors remain elsewhere. */
static void stream_hash_failures(void) {
    Source before;CompileFailure failure;
    load("begin end.");before=s;stream_copy_calls=0;stream_read_fail_after=0;
    check(compile_request(&s,0,&b,&failure,0,0,s.path));
    check(!stream_copy_calls&&b.valid);stream_read_fail_after=-1;
    check(build_check(&s,&b));build_close(&b);check(pages_used()==1&&!memcmp(&s,&before,sizeof(s)));
    check(mask_empty(compiler_owned,8)&&mask_empty(compiler_parent_owned,8)&&compiler_allocating==COMPILER_ALLOC_NONE);
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
        check(!memcmp(&s,&before,sizeof(s))&&mask_empty(compiler_owned,8)&&mask_empty(compiler_parent_owned,8)&&mask_empty(parent_pending,8));
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
    capture_boundaries();retained_source_cases();direct_input_cases();captured_identity_cases();source_adoption_cases();direct_result_cases();generated_map_cases();direct_map_integrity_cases();job_capacity();
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
        fail_after=-1;check(!b.valid&&pages_used()==1&&mask_empty(compiler_owned,8)&&mask_empty(parent_pending,8)&&compiler_allocating==COMPILER_ALLOC_NONE);
        check(!memcmp(&before,&s,sizeof(s)));
    }
    /* Retain failed releases explicitly; a later invocation retries them. */
    prior=32;while(page_role(prior))++prior;fail_drop=prior;
    check(!compile_request(&s,0,&b,&failure,0,0,s.path)&&failure.diagnostic.code==COMPILE_PLATFORM_FAILURE);
    check(page_role(prior)&&!mask_empty(parent_pending,8));fail_drop=-1;
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
