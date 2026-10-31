#include "harness.h"
#include "compiler_job.h"
#include "library.h"

volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s;Build b;Diagnostic d;
void test_complete(void);
/* Keep this focused target's checks in its test bank; the full corpus harness
 * also contains unrelated resident execution helpers. */
void check(uint8_t ok) {
    ++test_index;
    if(ok)++test_passed;
    else {if(test_failed<64)test_failures[test_failed]=test_index;++test_failed;}
}
uint8_t load(const char *text) {return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d);}
/* PERF10 prerequisite: exercise the real retained primitive before any job
 * allocates worker pages. Source bytes use an independent affine pattern. */
/* Naked test-only entry preserves the actual call1 arguments for the probe.
 * It stays in this job target's BANK1, not every test's shared machine core. */
uint8_t test_far_read_probe(uint8_t page,uint16_t offset,uint8_t *out,uint16_t n) NATIVE __naked {
    __asm
    push ix
    push iy
    ld iy,#0
    add iy,sp
    push iy
    ld l,8(iy)
    ld h,9(iy)
    push hl                      ; copied count
    ld l,6(iy)
    ld h,7(iy)
    push hl                      ; copied destination
    ld ix,#0x1357
    ld iy,#0x2468
    call _far_read               ; A/DE still contain page/offset
    push af
    push ix
    pop hl
    ld de,#0x1357
    or a
    sbc hl,de
    jr nz,probe_read_bad
    push iy
    pop hl
    ld de,#0x2468
    or a
    sbc hl,de
    jr nz,probe_read_bad
    pop af
    jr probe_read_done
probe_read_bad:
    pop af
    ld a,#255
probe_read_done:
    pop iy
    ld sp,iy
    pop iy
    pop ix
    pop hl
    pop bc
    pop bc
    jp (hl)
    __endasm;
}
volatile uint8_t span_benchmark;
uint8_t test_far_read_slow(uint8_t page,uint16_t offset,uint8_t *out,uint16_t n) {
    while(n--)*out++=far_get(page,offset++);
    return 1;
}
static uint8_t span_byte(uint16_t at) {return (uint8_t)(at*73u+19u);}
static void span_case(uint8_t page,uint16_t offset,uint16_t n,uint8_t expected,uint8_t enabled) {
    uint8_t out[259],before[7],after[7],result,good=1;uint16_t i;
    memset(out,0xa5,sizeof(out));
    if(!enabled) {__asm di __endasm;}
    test_machine(before);
    result=test_far_read_probe(page,offset,out+1,n);
    test_machine(after);
    if(!enabled) {__asm ei __endasm;}
    check(result==expected);check(!memcmp(before,after,7));
    check(before[0]==enabled&&before[2]>=0xc0&&out[0]==0xa5&&out[258]==0xa5);
    for(i=0;i<257;++i)if(out[i+1]!=(expected&&i<n?span_byte(offset+i):0xa5))good=0;
    check(good);
}
static void span_checks(void) {
    uint8_t p=page_new(PAGE_SOURCE),mode,before[7],after[7],good;
    volatile uint8_t result; /* keep both machine snapshots at identical SP */
    uint8_t out[64],old_bytes[2],new_bytes[2];uint16_t i;uint32_t version,lifetime;uint8_t owned=pages_used();
    static const struct {uint16_t offset,n;uint8_t ok;} cases[]={
        {0,0,1},{0,1,1},{16383,1,1},{0,64,1},{16320,64,1},
        {123,128,1},{0,255,1},{16128,256,1},{16127,257,0},
        {16384,1,0},{16383,2,0},{65535,1,0},{0,16385,0},{65535,65535,0}};
    static const uint16_t bad_out[]={0,0x3fff,0x7fff,0x8000,0xbfff,0xffff};
    check(p!=255);if(p==255)return;
    for(i=0;i<PAGE_SIZE;++i)far_set(p,i,span_byte(i));
    version=page_versions[p];lifetime=page_lifetime(p);
    for(mode=0;mode<2;++mode) {
        for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
            span_case(p,cases[i].offset,cases[i].n,cases[i].ok,mode);
        span_case(31,0,1,0,mode);span_case(64,0,1,0,mode);span_case(255,0,1,0,mode);
        for(i=0;i<sizeof(bad_out)/sizeof(bad_out[0]);++i) {
            if(!mode) {__asm di __endasm;}
            old_bytes[0]=*(volatile uint8_t*)bad_out[i];
            old_bytes[1]=*(volatile uint8_t*)(uint16_t)(bad_out[i]+1);
            test_machine(before);
            result=test_far_read_probe(p,0,(uint8_t*)bad_out[i],2);
            test_machine(after);
            new_bytes[0]=*(volatile uint8_t*)bad_out[i];
            new_bytes[1]=*(volatile uint8_t*)(uint16_t)(bad_out[i]+1);
            if(!mode) {__asm ei __endasm;}
            check(!result&&!memcmp(before,after,7)&&before[0]==mode&&!memcmp(old_bytes,new_bytes,2));
        }
        /* Zero accepts sentinel arguments without touching any address. */
        if(!mode) {__asm di __endasm;}
        test_machine(before);result=test_far_read_probe(255,65535,0,0);test_machine(after);
        if(!mode) {__asm ei __endasm;}
        check(result==1&&!memcmp(before,after,7)&&before[0]==mode);
    }
    /* Existing owned page1 scratch and parent page3 stack are both valid. */
    memset(console_scratch,0xa5,256);test_machine(before);
    result=test_far_read_probe(p,16130,console_scratch+1,254);test_machine(after);
    check(result==1&&!memcmp(before,after,7)&&console_scratch[0]==0xa5&&console_scratch[255]==0xa5);
    good=1;for(i=0;i<254;++i)if(console_scratch[i+1]!=span_byte(16130+i))good=0;check(good);
    /* Time copy work with IRQ service excluded; both IFF modes were tested above. */
    __asm di __endasm;
    for(mode=0;mode<2;++mode) {
        span_benchmark=1;check(test_far_read_slow(p,16320,out,64));
        good=1;for(i=0;i<64;++i)if(out[i]!=span_byte(16320+i))good=0;check(good);
        memset(out,0,64);span_benchmark=2;check(far_read(p,16320,out,64));
        good=1;for(i=0;i<64;++i)if(out[i]!=span_byte(16320+i))good=0;check(good);
    }
    span_benchmark=0;
    __asm ei __endasm;
    check(page_versions[p]==version&&page_lifetime(p)==lifetime&&pages_used()==owned);
    good=1;for(i=0;i<PAGE_SIZE;++i)if(far_get(p,i)!=span_byte(i))good=0;check(good);
    check(page_drop(p));
}
uint8_t test_far_fill_probe(uint8_t page,uint16_t offset,uint8_t value,uint16_t n) NATIVE __naked {
    __asm
    push ix
    push iy
    ld iy,#0
    add iy,sp
    push iy
    ld l,7(iy)
    ld h,8(iy)
    push hl                      ; copied count
    ld h,6(iy)
    push hl
    inc sp                       ; copied packed value byte
    ld ix,#0x1357
    ld iy,#0x2468
    call _far_fill               ; A/DE still contain page/offset
    push af
    push ix
    pop hl
    ld de,#0x1357
    or a
    sbc hl,de
    jr nz,probe_fill_bad
    push iy
    pop hl
    ld de,#0x2468
    or a
    sbc hl,de
    jr nz,probe_fill_bad
    pop af
    jr probe_fill_done
probe_fill_bad:
    pop af
    ld a,#255
probe_fill_done:
    pop iy
    ld sp,iy
    pop iy
    pop ix
    pop hl
    pop bc
    inc sp
    jp (hl)
    __endasm;
}

volatile uint8_t fill_benchmark;
uint8_t test_far_fill_slow(uint8_t page,uint16_t offset,uint8_t value,uint16_t n) {
    while(n--)far_set(page,offset++,value);
    return 1;
}
static void fill_case(uint8_t owned_page,uint8_t page,uint16_t offset,uint16_t n,
                      uint8_t value,uint8_t expected,uint8_t enabled,uint32_t version) {
    uint8_t before[7],after[7],good=1,other;uint16_t i;
    volatile uint8_t result;
    uint32_t versions[64],lifetime=page_lifetime(owned_page);uint8_t owned=pages_used();
    for(i=0;i<PAGE_SIZE;++i)far_set(owned_page,i,span_byte(i));
    page_versions[owned_page]=version;
    for(other=0;other<64;++other)versions[other]=page_versions[other];
    if(!enabled){__asm di __endasm;}
    test_machine(before);result=test_far_fill_probe(page,offset,value,n);test_machine(after);
    if(!enabled){__asm ei __endasm;}
    check(result==expected);check(!memcmp(before,after,7)&&before[0]==enabled&&before[2]>=0xc0);
    check(page_versions[owned_page]==version+(expected?(uint32_t)n:0));
    check(page_lifetime(owned_page)==lifetime&&page_role(owned_page)==PAGE_SOURCE&&pages_used()==owned);
    for(other=0;other<64;++other)if(other!=owned_page&&page_versions[other]!=versions[other])good=0;
    for(i=0;i<PAGE_SIZE;++i)
        if(far_get(owned_page,i)!=(expected&&i>=offset&&i-offset<n?value:span_byte(i)))good=0;
    check(good);
}
static void fill_checks(void) {
    static const struct {uint16_t offset,n;uint8_t ok;} cases[]={
        {0,0,1},{0,1,1},{16383,1,1},{0,64,1},{16320,64,1},
        {123,128,1},{0,255,1},{16128,256,1},{16127,257,0},
        {16384,1,0},{16383,2,0},{65535,1,0},{0,16385,0},{65535,65535,0}};
    static const uint32_t carry[]={0xffUL,0xffffUL,0xffffffUL,0xffffffffUL};
    uint8_t p=page_new(PAGE_SOURCE),mode,good;uint16_t i;
    uint32_t version;
    check(p!=255);if(p==255)return;
    for(mode=0;mode<2;++mode) {
        for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
            fill_case(p,p,cases[i].offset,cases[i].n,i&1?0:255,cases[i].ok,mode,7);
        fill_case(p,31,0,1,0xa5,0,mode,7);fill_case(p,64,0,1,0xa5,0,mode,7);
        fill_case(p,255,0,1,0xa5,0,mode,7);
        fill_case(p,255,65535,0,0xa5,1,mode,7);
        for(i=0;i<sizeof(carry)/sizeof(carry[0]);++i)fill_case(p,p,137,256,0xa5,1,mode,carry[i]);
        fill_case(p,p,137,1,0xa5,1,mode,0xffUL);
        fill_case(p,p,137,1,0xa5,1,mode,0xffffffffUL);
        fill_case(p,p,137,255,0xa5,1,mode,0x1ffUL);
        fill_case(p,p,137,255,0xa5,1,mode,0xffffff01UL);
    }
    version=page_versions[p];check(far_fill(p,0,0,64));check(far_fill(p,0,0,64));
    check(page_versions[p]==version+128);good=1;
    for(i=0;i<64;++i)if(far_get(p,i))good=0;check(good);
    __asm di __endasm;
    for(mode=0;mode<2;++mode) {
        fill_benchmark=1;check(test_far_fill_slow(p,512,0xa5,64));
        fill_benchmark=2;check(far_fill(p,512,0x5a,64));
    }
    fill_benchmark=0;__asm ei __endasm;
    good=1;for(i=512;i<576;++i)if(far_get(p,i)!=0x5a)good=0;check(good);
    check(page_drop(p));
    /* Identical source stores invalidate cached versions without changing
     * text. Catalog self-stores likewise invalidate its authenticated view. */
    check(load("begin end."));check(compile(&s,&b,&d));
    check(build_check(&s,&b));
    check(far_fill(s.page,0,far_get(s.page,0),1));check(!build_check(&s,&b));build_close(&b);
    {
        Sources catalog;uint8_t hash[32];
        check(sources_new(&catalog));source_hash(&s,hash);check(sources_borrow(&catalog,&s,hash)==0);
        check(sources_valid(&catalog)&&sources_ready(&catalog,0));
        check(far_fill(s.page,0,far_get(s.page,0),1));check(!sources_ready(&catalog,0));
        check(far_fill(catalog.page,0,far_get(catalog.page,0),1));check(!sources_valid(&catalog));
        /* Borrowed catalog owns no source page; release its stale handle
         * directly rather than authenticate it as an unchanged catalog. */
        check(page_drop(catalog.page));
    }
    check(pages_used()==code_required+1);
}

uint8_t test_far_move_probe(uint8_t page,uint16_t offset,uint16_t source,uint16_t n) NATIVE __naked {
    __asm
    push ix
    push iy
    ld iy,#0
    add iy,sp
    push iy
    ld l,8(iy)
    ld h,9(iy)
    push hl                      ; copied count
    ld l,6(iy)
    ld h,7(iy)
    push hl                      ; copied source offset
    ld ix,#0x1357
    ld iy,#0x2468
    call _far_move               ; A/DE still contain page/offset
    push af
    push ix
    pop hl
    ld de,#0x1357
    or a
    sbc hl,de
    jr nz,probe_move_bad
    push iy
    pop hl
    ld de,#0x2468
    or a
    sbc hl,de
    jr nz,probe_move_bad
    pop af
    jr probe_move_done
probe_move_bad:
    pop af
    ld a,#255
probe_move_done:
    pop iy
    ld sp,iy
    pop iy
    pop ix
    pop hl
    pop bc
    pop bc
    jp (hl)
    __endasm;
}

volatile uint8_t move_benchmark;
uint8_t test_far_move_slow(uint8_t page,uint16_t to,uint16_t from,uint16_t n) {
    uint16_t i;
    if(to>from)for(i=n;i;--i)far_set(page,to+i-1,far_get(page,from+i-1));
    else for(i=0;i<n;++i)far_set(page,to+i,far_get(page,from+i));
    return 1;
}
static uint8_t move_byte(uint16_t at) {return (uint8_t)(at*73u+(at>>8)*17u+19u);}
static void move_case(uint8_t owned_page,uint8_t page,uint16_t to,uint16_t from,uint16_t n,
                      uint8_t expected,uint8_t enabled,uint32_t version) {
    uint8_t before[7],after[7],good=1,other;uint16_t i;
    volatile uint8_t result;
    uint32_t versions[64],lifetime=page_lifetime(owned_page);uint8_t owned=pages_used();
    for(i=0;i<PAGE_SIZE;++i)far_set(owned_page,i,move_byte(i));
    page_versions[owned_page]=version;
    for(other=0;other<64;++other)versions[other]=page_versions[other];
    if(!enabled){__asm di __endasm;}
    test_machine(before);result=test_far_move_probe(page,to,from,n);test_machine(after);
    if(!enabled){__asm ei __endasm;}
    check(result==expected);check(!memcmp(before,after,7)&&before[0]==enabled&&before[2]>=0xc0);
    check(page_versions[owned_page]==version+(expected?(uint32_t)n:0));
    check(page_lifetime(owned_page)==lifetime&&page_role(owned_page)==PAGE_SOURCE&&pages_used()==owned);
    for(other=0;other<64;++other)if(other!=owned_page&&page_versions[other]!=versions[other])good=0;
    for(i=0;i<PAGE_SIZE;++i)
        if(far_get(owned_page,i)!=(expected&&i>=to&&i-to<n?move_byte(from+i-to):move_byte(i)))good=0;
    check(good);
}
static void move_checks(void) {
    static const struct {uint16_t to,from,n;uint8_t ok;} cases[]={
        {0,0,0,1},{0,1,1,1},{1,0,1,1},{16383,0,1,1},{0,16383,1,1},
        {0,512,64,1},{512,0,64,1},{0,1,64,1},{1,0,64,1},
        {1,0,128,1},{0,1,128,1},{63,0,128,1},{0,63,128,1},
        {0,0,255,1},{1,0,255,1},{0,1,255,1},{16128,0,256,1},{0,16128,256,1},
        {256,0,256,1},{0,256,256,1},{1,0,256,1},{0,1,256,1},
        {16129,16128,255,1},{16128,16129,255,1},{16383,16383,1,1},
        {0,0,257,0},{16384,0,1,0},{0,16384,1,0},{16383,0,2,0},{0,16383,2,0},
        {65535,0,1,0},{0,65535,1,0},{65535,65535,65535,0}};
    uint8_t p=page_new(PAGE_SOURCE),mode,good;uint16_t i;
    check(p!=255);if(p==255)return;
    for(mode=0;mode<2;++mode) {
        for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
            move_case(p,p,cases[i].to,cases[i].from,cases[i].n,cases[i].ok,mode,7);
        move_case(p,31,0,1,1,0,mode,7);move_case(p,64,0,1,1,0,mode,7);
        move_case(p,255,0,1,1,0,mode,7);move_case(p,255,65535,65535,0,1,mode,7);
        move_case(p,p,1,0,1,1,mode,0xffUL);
        move_case(p,p,0,1,255,1,mode,0xffffUL);
        move_case(p,p,1,0,256,1,mode,0xffffffUL);
        move_case(p,p,1,0,64,1,mode,0xffffffffUL);
    }
    __asm di __endasm;
    for(mode=0;mode<2;++mode) {
        move_benchmark=1;check(test_far_move_slow(p,1024,512,64));
        move_benchmark=2;check(far_move(p,2048,512,64));
    }
    move_benchmark=0;__asm ei __endasm;
    good=1;for(i=0;i<64;++i)if(far_get(p,2048+i)!=far_get(p,512+i))good=0;check(good);
    check(page_drop(p));
}

volatile uint8_t tail_probe;
static uint8_t tail_byte(uint16_t at) {return at<16000?(at%100==99?'\n':'a'):0xa5;}
static void tail_case(uint8_t shrink,uint8_t enabled) {
    Source before,wanted;uint8_t text[257],cpu_before[7],cpu_after[7],good=1,c;
    uint16_t i,z=shrink?8513:8001,n=shrink?0:257;
    uint32_t version,lifetime=page_lifetime(s.page);uint8_t owned=pages_used();
    volatile uint8_t result;
    for(i=0;i<PAGE_SIZE;++i)far_set(s.page,i,tail_byte(i));
    for(i=0;i<257;++i)text[i]=i&1?'\n':'x';
    s.length=16000;s.cursor=123;s.previous=12000;s.revision=23;s.dirty=0;
    page_initialized(s.page,16000);before=s;wanted=before;
    wanted.length=16000-(z-8000)+n;wanted.cursor=8000+n;
    wanted.previous=12000-(z-8000)+n;wanted.dirty=1;++wanted.revision;
    version=page_versions[s.page];
    if(!enabled){__asm di __endasm;}
    test_machine(cpu_before);tail_probe=(shrink?2:1)+2*enabled;
    result=source_replace(&s,8000,z,text,n);tail_probe=0;test_machine(cpu_after);
    if(!enabled){__asm ei __endasm;}
    check(result==1);check(!memcmp(cpu_before,cpu_after,7)&&cpu_before[0]==enabled);
    check(!memcmp(&s,&wanted,sizeof(s)));
    check(page_versions[s.page]==version+16000-z+n);
    check(page_lifetime(s.page)==lifetime&&page_role(s.page)==PAGE_SOURCE&&pages_used()==owned);
    for(i=0;i<PAGE_SIZE;++i) {
        c=i<wanted.length?(i<8000?tail_byte(i):i<8000+n?text[i-8000]:tail_byte(i-n+z-8000)):tail_byte(i);
        if(far_get(s.page,i)!=c)good=0;
    }
    check(good);
}
static void tail_checks(void) {
    tail_case(0,0);tail_case(1,0);tail_case(0,1);tail_case(1,1);
}
void test_span_done(void) {__asm nop __endasm;}
static CompileFailure failure;
static uint8_t expected[32];
static uint8_t request(uint8_t mode,const CompileOptions *options) {
    return compile_request(&s,0,&b,&failure,mode,options,s.path);
}
static void closed(void) {build_close(&b);check(pages_used()==code_required+1);}
int app_main(void) {
    Source before,item;Sources buffers;CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};
    uint8_t hash[32],status=255,owned;int8_t fd;uint16_t pc,position;
    if(!console_open())return 1;
    check(source_new(&s,"H:/JOB.PAS"));
    test_stage=79;span_checks();fill_checks();move_checks();tail_checks();test_span_done();
    fd=open("H:/JOBS.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);if(fd>=0)check(!swap(fd,DEV_STDOUT));
    test_stage=80;
    check(load("program Hello;begin writeln('JOB-OK')end."));s.cursor=8;s.previous=3;s.dirty=1;before=s;
    check(compile(&s,&b,&d));memcpy(expected,b.id,32);closed();
    compiler_allocating=COMPILER_ALLOC_WORKER;owned=page_new(PAGE_COMPILER);compiler_allocating=COMPILER_ALLOC_NONE;
    check(owned!=255);check(code_phase_park());check(request(0,0));
    check(job_zero(compiler_owned,8)&&job_zero(compiler_parent_owned,8));check(b.valid&&!memcmp(b.id,expected,32)&&!memcmp(&s,&before,sizeof(s)));
    check(!*output_recovery()&&build_check(&s,&b));owned=pages_used();
    check(run_build(&s,&b)&&ram_result[26]==1);check(pages_used()==owned);
    check(code_phase_restore());closed();
    test_stage=81;
    check(request(1,&options));check(*output_recovery());check(save_build(&s,&b,0));
    check(!*output_recovery()&&!memcmp(&s,&before,sizeof(s)));
    check(run_saved(&s,&b,&status)&&status==0&&b.valid);check(code_phase_restore());closed();
    options.kind=COMPILE_CHAIN;check(request(2,&options));check(build_is_chain(&b)&&*output_recovery());
    check(save_build(&s,&b,0)&&!*output_recovery());closed();options.kind=COMPILE_PROGRAM;
    test_stage=82;
    check(load("program Include;\n{$I PART.INC}\nbegin writeln(answer)end."));before=s;
    check(request(1,&options));check(b.sources.count==2&&sources_get(&b.sources,1,&item)&&item.page==255);
    check(save_build(&s,&b,0)&&!*output_recovery()&&!strcmp(output_path(),"H:/JOB.COM"));
    check(build_check(&s,&b)&&!memcmp(&s,&before,sizeof(s)));
    pc=96+far_get(b.map_page,84)+((uint16_t)far_get(b.map_page,85)<<8);
    pc=far_get(b.map_page,pc)|((uint16_t)far_get(b.map_page,pc+1)<<8);
    check(build_locate(&s,&b,pc,&position));closed();
    test_stage=83;
    check(load("program Bad;   \nvar n:integer;   \nbegin n:=;end."));before=s;
    check(!compile(&s,&b,&d));position=d.start;
    check(!request(1,&options)&&failure.diagnostic.code==d.code&&failure.diagnostic.start==position);
    check(!strcmp(failure.path,s.path)&&failure.has_hash&&!*output_recovery()&&pages_used()==code_required+1&&!memcmp(&s,&before,sizeof(s)));
    check(load("program Include;\n{$I BAD.INC}\nbegin end."));
    check(!request(0,0)&&failure.diagnostic.code<101&&!strcmp(failure.path,"H:/BAD.INC")&&failure.has_hash&&pages_used()==code_required+1);
    check(load("program Missing;\n{$I MISSING.INC}\nbegin end."));
    check(!request(1,&options)&&failure.diagnostic.code==90&&!strcmp(failure.path,"H:/MISSING.INC")&&!failure.has_hash&&!*output_recovery()&&pages_used()==code_required+1);
    test_stage=84;
    check(load("program Override;\n{$I PART.INC}\nbegin writeln(answer)end."));
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    check(source_new(&item,"H:/PART.INC"));check(source_import(&item,(const uint8_t*)"const answer=;",14,&d));
    source_hash(&item,hash);check(sources_borrow(&buffers,&item,hash)==1);owned=pages_used();
    check(!compile_request(&s,&buffers,&b,&failure,0,0,item.path));
    check(failure.diagnostic.code<101&&!strcmp(failure.path,item.path)&&failure.has_hash&&!memcmp(failure.hash,hash,32)&&pages_used()==owned);
    check(sources_close(&buffers));source_close(&item);check(pages_used()==code_required+1);
    test_stage=85;
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    memset(&item,0,sizeof(item));item.page=255;strcpy(item.path,"H:/DISK.PAS");owned=pages_used();
    check(compile_request(&item,&buffers,&b,&failure,0,0,s.path));
    check(b.valid&&sources_get(&b.sources,0,&item)&&item.page!=255&&!sources_borrowed(&b.sources,0));
    check(build_check(&item,&b));build_close(&b);check(pages_used()==owned);check(sources_close(&buffers));
    test_stage=86;
    check(load_source(&s,"H:/LARGE.PAS",&d));before=s;
    check(sources_new(&buffers));source_hash(&s,hash);check(sources_borrow(&buffers,&s,hash)==0);
    options.start=0x8040;owned=pages_used();
    check(compile_request(&s,&buffers,&b,&failure,1,&options,s.path));
    check(b.valid&&b.code_bytes>PAGE_SIZE&&build_check(&s,&b));
    check(!memcmp(&s,&before,sizeof(s))&&sources_ready(&buffers,0));
    check(save_build(&s,&b,0)&&!*output_recovery());build_close(&b);check(pages_used()==owned);
    check(sources_close(&buffers));
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
