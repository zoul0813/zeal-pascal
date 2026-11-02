#include "hash-evidence.h"
/* Execute product publication against a deterministic VFS. Faults are injected
 * at API boundaries, independently of the implementation's transaction stages. */
#include "zp.h"
#include "output_transport.h"
#include "compiler_members.h"
#include "compiler_groups.h"
#include "library.h"
#include "file_runtime.h"
#include <zos_vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define exit zos_exit
#include <zos_sys.h>
#undef exit

#define FILES 40
#define BYTES 200000
static struct { char path[128]; uint8_t bytes[BYTES],dir; uint32_t length; } files[FILES];
static struct { int file; uint32_t pos; uint8_t flags,used; } fds[12];
static uint8_t memory[64][PAGE_SIZE],roles[64];
volatile uint32_t page_versions[64];
uint8_t console_scratch[256],platform_error;
static unsigned assertions,scenarios,events,polls,fail_at,cancel_at;
static uint8_t shorts,cancel_reason=1,rollback_fault,bad_io;
static unsigned writes,reads,fail_write,fail_read;
static unsigned case_kind,case_old;
static int drop_failure=-1;
static uint32_t lifetimes[64];
uint8_t code_pages[CODE_BANK_LIMIT],code_owned,boot_error;
uint16_t code_sizes[CODE_BANK_LIMIT];
const uint8_t code_required=15,code_version[4]={0};
static int phase_free_failure=-1;
static uint8_t allocation_failure,exec_allocation_failure,exec_corrupt_map,exec_corrupt_image,exec_error,exec_finished,exec_corrupt_phase;
static unsigned exec_calls,cache_checks,cache_closes;
static Source source,before;
static Build build;
Compiler cc;
/* This component fixture does not scan Pascal or enable compiler polling.
 * Emission/word/chain operations below use the real shared front end. */
uint8_t cc_poll(void) {return 1;}
static Diagnostic late_diagnostic;
static CompilerSources late_sources;
/* Mutate an owner at an actual streaming SDK boundary, after its initial
 * preflight. This models invalidation during writes, verification or seeks. */
static CompilerLate *stream_late;
static uint8_t stream_page,stream_seal;
static unsigned stream_mutate_at;
static void stream_mutate(void) {
    if(!stream_mutate_at||events!=stream_mutate_at)return;
    stream_mutate_at=0;
    if(stream_seal)stream_late->sealed=1;
    else ++page_versions[stream_page];
}
static const char *target[2];
static uint8_t old[2][401],fresh[401];
static unsigned old_length[2]={273,401};

static void check_at(int ok,unsigned line) {
    ++assertions;
    if(!ok) {fprintf(stderr,"output check %u line %u case %u/%u events %u fail %u poll %u cancel %u error %u\n",assertions,line,case_kind,case_old,events,fail_at,polls,cancel_at,platform_error);abort();}
}
#define check(x) check_at((x),__LINE__)
static int find(const char *path) {int i;for(i=0;i<FILES;++i)if(!strcmp(path,files[i].path))return i;return -1;}
static int make(const char *path,uint8_t dir) {
    int i=find(path);if(i>=0)return i;
    for(i=0;i<FILES;++i)if(!files[i].path[0]) {strcpy(files[i].path,path);files[i].dir=dir;files[i].length=0;return i;}
    abort();
}
static void seed(const char *path,const uint8_t *data,unsigned n) {int i=make(path,0);check(n<BYTES);memcpy(files[i].bytes,data,n);files[i].length=n;}
static int fault(void) {return ++events==fail_at;}
static unsigned opened(void) {unsigned i,n=0;for(i=2;i<12;++i)n+=fds[i].used;return n;}
static unsigned owned(void) {unsigned i,n=0;for(i=0;i<16;++i)n+=roles[i]!=0;return n;}
uint8_t page_new(uint8_t role) {uint8_t i;if(allocation_failure)return 255;for(i=0;i<16;++i)if(!roles[i]){roles[i]=role;++lifetimes[i];memset(memory[i],0xa5,PAGE_SIZE);return i;}return 255;}
uint8_t page_drop(uint8_t p) {assert(p<16&&roles[p]);if(p==drop_failure){platform_error=ERR_FAILURE;return 0;}roles[p]=0;return 1;}
uint8_t page_role(uint8_t p) {return p<64?roles[p]:0;}
uint32_t page_lifetime(uint8_t p) {return page_role(p)?lifetimes[p]:0;}
uint8_t far_get(uint8_t p,uint16_t at) {assert(p<64&&roles[p]&&at<PAGE_SIZE);return memory[p][at];}
void far_set(uint8_t p,uint16_t at,uint8_t b) {assert(p<64&&roles[p]&&at<PAGE_SIZE);memory[p][at]=b;++page_versions[p];}
#include "far-move-model.h"
uint8_t far_move(uint8_t p,uint16_t to,uint16_t from,uint16_t n) {return model_far_move(p,to,from,n);}
void page_initialized(uint8_t p,uint16_t n) {assert(page_role(p)&&n<=PAGE_SIZE);}
zos_err_t palloc(uint8_t *page) {
    uint8_t i;if(fault()||allocation_failure)return ERR_NO_MORE_MEMORY;
    for(i=32;i<64;++i)if(!roles[i]) {roles[i]=128;++lifetimes[i];*page=i;return 0;}
    return ERR_NO_MORE_MEMORY;
}
zos_err_t pfree(uint8_t page) {
    assert(page>=32&&page<64&&roles[page]==128);
    if(fault()||page==phase_free_failure)return ERR_FAILURE;
    roles[page]=0;return 0;
}
uint8_t key_interrupt(void) {platform_error=0;return ++polls==cancel_at?cancel_reason:0;}
uint8_t build_ready(const Source *s,const Build *b) {return s->page==source.page&&b==&build;}
/* Publication/member fixtures preallocate their image pages. Compiler page
 * materialization is exercised by the shared front-end/native suites. */
uint8_t cc_image_page(uint8_t index) {(void)index;assert(0);return 0;}
/* Model RAM version/ownership authority; restored content is not hashed. */
uint8_t build_check(const Source *s,const Build *b) {
    unsigned i;++cache_checks;
    if(!build_ready(s,b)||!b->valid||page_role(b->map_page)!=PAGE_MAP)return 0;
    for(i=0;i<3;++i)if((uint32_t)i*PAGE_SIZE<b->length&&page_role(b->pages[i])!=PAGE_IMAGE)return 0;
    return 1;
}
void build_close(Build *b) {
    unsigned i;++cache_closes;b->valid=0;
    for(i=0;i<3;++i)if(b->pages[i]!=255&&page_drop(b->pages[i]))b->pages[i]=255;
    if(b->map_page!=255&&page_drop(b->map_page))b->map_page=255;
    b->sources.page=255;
}
zos_err_t curdir(char *path) {strcpy(path,"T:/");return 0;}
zos_err_t exec(zos_exec_mode_t mode,const char *path,const char *args[],uint8_t *status) {
    (void)args;++exec_calls;
    check(mode==EXEC_PRESERVE_PROGRAM&&!strcmp(path,target[0]));
    check(!build.valid&&build.map_page==255&&owned()==1);
    if(code_owned) {
        unsigned i;for(i=CODE_PHASE_FIRST;i<code_owned;++i)check(code_pages[i]==255);
    }
    if(fault()||exec_error)return exec_error?exec_error:ERR_FAILURE;
    if(exec_corrupt_map)files[find(target[1])].bytes[exec_corrupt_map==1?0:build.map_length-1]^=1;
    if(exec_corrupt_image)files[find(target[0])].bytes[exec_corrupt_image==1?0:build.length-1]^=1;
    if(exec_allocation_failure)allocation_failure=1;
    if(exec_corrupt_phase)files[fds[(uint8_t)code_phase_fd].file].length=1;
    exec_finished=1;*status=7;return 0;
}
zos_dev_t open(const char *path,uint8_t flags) {
    int f=find(path),fd;
    if(fault())return -ERR_FAILURE;
    if(f<0) {if(!(flags&O_CREAT))return -ERR_NO_SUCH_ENTRY;f=make(path,0);}
    if(files[f].dir)return -ERR_NOT_A_FILE;
    for(fd=2;fd<12;++fd)if(!fds[fd].used)break;
    assert(fd<12);fds[fd].used=1;fds[fd].file=f;fds[fd].pos=0;fds[fd].flags=flags;
    if(flags&O_TRUNC)files[f].length=0;
    return fd;
}
zos_err_t close(zos_dev_t fd) {assert(fd>=2&&fd<12&&fds[(uint8_t)fd].used);fds[(uint8_t)fd].used=0;return fault()?ERR_FAILURE:0;}
zos_err_t read(zos_dev_t fd,void *out,uint16_t *size) {
    unsigned n=*size,left;int f;
    assert(fd>=2&&fd<12&&fds[(uint8_t)fd].used);f=fds[(uint8_t)fd].file;
    if(fault())return ERR_FAILURE;
    stream_mutate();
    if(++reads==fail_read&&bad_io==3){++*size;return 0;}
    if(reads==fail_read&&bad_io==4){*size=0;return 0;}
    left=files[f].length-fds[(uint8_t)fd].pos;
    if(n>left)n=left;
    if(shorts&&n>(unsigned)(fd&1?11:7))n=fd&1?11:7;
    memcpy(out,files[f].bytes+fds[(uint8_t)fd].pos,n);
    if(reads==fail_read&&bad_io==5&&n)((uint8_t*)out)[0]^=1;
    fds[(uint8_t)fd].pos+=n;*size=n;return 0;
}
zos_err_t write(zos_dev_t fd,const void *in,uint16_t *size) {
    unsigned n=*size;int f;uint8_t error;
    assert(fd>=2&&fd<12&&fds[(uint8_t)fd].used);f=fds[(uint8_t)fd].file;
    assert((fds[(uint8_t)fd].flags&3)!=O_RDONLY);
    error=fault();
    stream_mutate();
    if(rollback_fault&&output_interrupt_reason()&&(!strcmp(files[f].path,target[0])||!strcmp(files[f].path,target[1])))error=1;
    if(++writes==fail_write) {
        if(bad_io==1){*size=0;return 0;}
        if(bad_io==2){++*size;return 0;}
    }
    if(shorts&&n>13)n=13;
    if(error&&n>3)n=3; /* A failed write may already have modified the medium. */
    assert(fds[(uint8_t)fd].pos+n<BYTES);
    memcpy(files[f].bytes+fds[(uint8_t)fd].pos,in,n);fds[(uint8_t)fd].pos+=n;
    if(fds[(uint8_t)fd].pos>files[f].length)files[f].length=fds[(uint8_t)fd].pos;
    *size=n;return error?ERR_FAILURE:0;
}
zos_err_t stat(const char *path,zos_stat_t *out) {
    int f=find(path);if(fault())return ERR_FAILURE;if(f<0)return ERR_NO_SUCH_ENTRY;
    memset(out,0,sizeof(*out));out->s_flags=!files[f].dir;out->s_size=files[f].length;return 0;
}
zos_err_t seek(zos_dev_t fd,int32_t *at,zos_whence_t whence) {
    int f;int64_t next;
    assert(fd>=2&&fd<12&&fds[(uint8_t)fd].used);f=fds[(uint8_t)fd].file;
    if(fault())return ERR_FAILURE;
    stream_mutate();
    next=*at;if(whence==SEEK_END)next+=files[f].length;
    else if(whence==SEEK_CUR)next+=fds[(uint8_t)fd].pos;
    if(next<0)next=0;if(next>files[f].length)next=files[f].length;
    fds[(uint8_t)fd].pos=(uint32_t)next;*at=(int32_t)next;return 0;
}
zos_err_t mkdir(const char *path) {if(fault())return ERR_FAILURE;if(find(path)>=0)return ERR_ALREADY_EXIST;make(path,1);return 0;}
zos_err_t rm(const char *path) {
    int f=find(path),i;unsigned n=strlen(path);
    if(fault())return ERR_FAILURE;if(f<0)return ERR_NO_SUCH_ENTRY;
    if(files[f].dir)for(i=0;i<FILES;++i)if(!strncmp(path,files[i].path,n)&&files[i].path[n]=='/')return ERR_FAILURE;
    files[f].path[0]=0;return 0;
}
static int equals(const char *path,const uint8_t *data,unsigned size) {
    int f=find(path);return f>=0&&!files[f].dir&&files[f].length==size&&!memcmp(files[f].bytes,data,size);
}
/* kind: source, Save As, block, executable+map. old is a presence bitmask. */
static void setup(unsigned kind,unsigned mask) {
    unsigned i;assert(!opened());memset(files,0,sizeof(files));memset(fds,0,sizeof(fds));memset(roles,0,sizeof(roles));
    memset(&build,0,sizeof(build));case_kind=kind;case_old=mask;events=polls=writes=reads=0;platform_error=0;
    target[0]=kind==1?"T:/new/name.pas":kind==2?"T:/block.pas":kind>=3?(kind==4?"T:/code.CHN":"T:/code.COM"):"T:/code.pas";
    target[1]=kind==1?"T:/new/name.BAK":kind>=3?"T:/code.ZMP":"T:/code.BAK";
    make("T:/",1);make("T:/ZPKEEP",1);seed("T:/ZPKEEP/FOREIGN",(const uint8_t*)"keep",4);
    if(kind==1)make("T:/new",1);
    for(i=0;i<401;++i){fresh[i]=i%79==78?'\n':(uint8_t)('a'+i%26);old[0][i]=(uint8_t)(i*3+5);old[1][i]=(uint8_t)(i*7+9);}
    check(source_new(&source,kind==1?"T:/original.pas":"T:/code.pas"));
    check(source_replace(&source,0,0,fresh,sizeof(fresh)));source.cursor=19;source.previous=6;before=source;
    for(i=0;i<2;++i)if(mask&(1u<<i))seed(target[i],old[i],old_length[i]);
    if(kind>=3) {
        build.pages[0]=page_new(PAGE_IMAGE);build.pages[1]=page_new(PAGE_IMAGE);build.pages[2]=255;build.map_page=page_new(PAGE_MAP);
        build.length=PAGE_SIZE+513;build.map_length=913;
        for(i=0;i<build.length;++i)far_set(build.pages[i/PAGE_SIZE],i%PAGE_SIZE,(uint8_t)(i*11+3));
        if(kind==4)for(i=0;i<4;++i)far_set(build.pages[0],i,(uint8_t)"ZCHN"[i]);
        for(i=0;i<build.map_length;++i)far_set(build.map_page,i,(uint8_t)(i*13+1));
    }
}
static uint8_t save(void) {
    switch(case_kind) {
    case 0:return save_source(&source,1,1);
    case 1:return save_source_as(&source,"T:/new/name.pas",1,1);
    case 2:return save_block(&source,17,389,target[0],1,1);
    default:return save_build(&source,&build,1);
    }
}
static void prove(uint8_t success,unsigned pages,uint8_t restore_failed) {
    unsigned i;int f;
    (void)restore_failed;
    ++scenarios;check(!opened());check(owned()==pages);check(equals("T:/ZPKEEP/FOREIGN",(const uint8_t*)"keep",4));
    check(!memcmp(memory[source.page],fresh,sizeof(fresh)));
    if(!success) {
        check(!memcmp(&source,&before,sizeof(source)));
        if(case_kind<2) {
            if(case_old&1)check(equals(target[0],old[0],old_length[0])||equals(target[1],old[0],old_length[0]));
            else if(case_old&2)check(equals(target[1],old[1],old_length[1]));
        } else if(case_kind>=3&&find(target[0])>=0&&!equals(target[0],old[0],old_length[0])) {
            check(!equals(target[1],old[1],old_length[1]));
        }
    } else {
        check(!output_interrupt_reason());
        if(case_kind<2) {
            check(!source.dirty);check(!strcmp(source.path,target[0]));check(equals(target[0],fresh,sizeof(fresh)));
            if(case_old&1)check(equals(target[1],old[0],old_length[0]));
            else if(case_old&2)check(equals(target[1],old[1],old_length[1]));
        } else {
            check(!memcmp(&source,&before,sizeof(source)));
            if(case_kind==2)check(equals(target[0],fresh+17,372));
            else {
                f=find(target[0]);check(f>=0&&files[f].length==build.length);
                for(i=0;i<build.length;++i)check(files[f].bytes[i]==(case_kind==4&&i<4?(uint8_t)"ZCHN"[i]:(uint8_t)(i*11+3)));
                f=find(target[1]);check(f>=0&&files[f].length==build.map_length);
                for(i=0;i<build.map_length;++i)check(files[f].bytes[i]==(uint8_t)(i*13+1));
            }
        }
    }
}
static void run_setup(void) {
    unsigned i;int f;
    fail_at=cancel_at=rollback_fault=shorts=bad_io=fail_read=fail_write=0;
    drop_failure=-1;allocation_failure=exec_allocation_failure=exec_corrupt_map=exec_corrupt_image=exec_error=exec_finished=0;
    exec_calls=cache_checks=cache_closes=0;
    setup(3,0);build.valid=1;
    f=make(target[0],0);files[f].length=build.length;
    for(i=0;i<build.length;++i)files[f].bytes[i]=far_get(build.pages[i/PAGE_SIZE],i%PAGE_SIZE);
    seed(target[1],memory[build.map_page],build.map_length);
    output_job_begin(0);events=reads=0;
}
static void saved_cache(void) {
    unsigned total,i;uint8_t status,ok;
    run_setup();status=255;
    check(run_saved(&source,&build,&status)&&status==7&&!build.valid);
    check(!cache_checks&&cache_closes==1&&owned()==1&&!opened());total=events;
    for(i=1;i<=total;++i) {
        run_setup();fail_at=i;status=255;ok=run_saved(&source,&build,&status);
        check(ok==exec_finished&&status==(exec_finished?7:255));
        check(!memcmp(&source,&before,sizeof(source))&&!memcmp(memory[source.page],fresh,sizeof(fresh))&&!opened());
        check(owned()==(build.valid?4u:1u));
        if(!ok)check(platform_error);
        if(exec_calls)check(!build.valid&&cache_closes==1&&build.map_page==255);
    }
    /* Child-side disk changes and allocation failures cannot cause a reload:
     * the compiled image/map have already been released permanently. */
    for(i=0;i<5;++i) {
        run_setup();exec_corrupt_map=i==0?1:i==1?2:0;
        exec_corrupt_image=i==2?1:i==3?2:0;exec_allocation_failure=i==4;status=255;
        check(run_saved(&source,&build,&status)&&status==7&&!build.valid&&owned()==1&&!cache_checks);
    }
    run_setup();exec_error=ERR_NOT_SUPPORTED;status=255;
    check(!run_saved(&source,&build,&status)&&status==255&&platform_error==ERR_NOT_SUPPORTED&&!build.valid&&owned()==1);
    run_setup();drop_failure=build.map_page;status=255;
    check(!run_saved(&source,&build,&status)&&status==255&&!exec_calls&&!build.valid&&build.map_page!=255);
    drop_failure=-1;build_close(&build);check(owned()==1);
    run_setup();drop_failure=build.pages[0];status=255;
    check(!run_saved(&source,&build,&status)&&status==255&&!exec_calls&&!build.valid&&owned()==2);
    drop_failure=-1;build_close(&build);check(owned()==1);
}
static CompilerMemberStore member_store;
static CompilerMember member_record;
static int8_t member_fd;
static void member_setup(void) {
    unsigned i;
    fail_at=cancel_at=rollback_fault=shorts=bad_io=fail_read=fail_write=0;
    stream_mutate_at=0;
    setup(0,0);memset(&cc,0,sizeof(cc));memset(&member_record,0x5a,sizeof(member_record));
    build.pages[2]=page_new(PAGE_IMAGE);build.map_page=page_new(PAGE_MAP);
    cc.stage_page=page_new(PAGE_COMPILER);cc.workspace=page_new(PAGE_COMPILER);
    cc.image=&build;cc.emitted=0xc201;cc.entries=5;cc.file_bytes=71;cc.relocations=7;cc.routine_count=2;
    member_record.origin=0xbff1;member_record.entry=0xc000;member_record.map_first=2;member_record.reloc_first=3;
    for(i=0;i<PAGE_SIZE;++i) {
        memory[build.pages[2]][i]=(uint8_t)(i*3+9);memory[cc.stage_page][i]=(uint8_t)(i*5+1);
        memory[build.map_page][i]=(uint8_t)(i*7+3);memory[cc.workspace][i]=(uint8_t)(i*11+5);
    }
    member_fd=open("T:/PRIVATE.SPL",O_RDWR|O_CREAT|O_TRUNC);check(member_fd>=0);
    check(cc_members_begin(&member_store,member_fd));output_job_begin(1);
    events=polls=writes=reads=0;
}
static void member_close(void) {
    output_job_end();fail_at=0;check(close(member_fd)==0);check(!opened());
}
static void member_unchanged(const CompilerMember *prior) {
    check(!memcmp(&member_record,prior,sizeof(member_record)));
    check(cc.emitted==0xc201&&cc.entries==5&&cc.relocations==7&&cc.routine_count==2);
    check(equals("T:/ZPKEEP/FOREIGN",(const uint8_t*)"keep",4));
    check(!memcmp(memory[source.page],fresh,sizeof(fresh)));
}
static void member_spool(void) {
    CompilerMember prior;
    CompilerMemberStore poisoned;
    unsigned i,at,n,ops,poll_count;int file;
    static uint8_t snapshot[4][PAGE_SIZE];
    member_setup();prior=member_record;
    memcpy(snapshot[0],memory[build.pages[2]],PAGE_SIZE);memcpy(snapshot[1],memory[cc.stage_page],PAGE_SIZE);
    memcpy(snapshot[2],memory[build.map_page],PAGE_SIZE);memcpy(snapshot[3],memory[cc.workspace],PAGE_SIZE);
    check(cc_members_flush(&member_store,&member_record,0,0));ops=events;poll_count=polls;
    check(!member_store.failed&&member_record.offset==0&&member_record.bytes==528&&member_record.maps==3&&member_record.relocs==4);
    n=24+528+48+16;check(member_store.length==n);
    file=find("T:/PRIVATE.SPL");check(files[file].length==n);
    check(!memcmp(files[file].bytes,"ZPMS\2\0\30\0",8));
    check(files[file].bytes[8]==0xf1&&files[file].bytes[9]==0xbf&&files[file].bytes[12]==15);
    at=24;
    for(i=0xbff1;i<0xc201;++i)check(files[file].bytes[at++]==(i<0xc000?memory[build.pages[2]][i-0x8000]:memory[cc.stage_page][i-0xc000]));
    for(i=199;i<247;++i)check(files[file].bytes[at++]==memory[build.map_page][i]);
    for(i=12;i<28;++i)check(files[file].bytes[at++]==memory[cc.workspace][i]);
    check(!memcmp(snapshot[0],memory[build.pages[2]],PAGE_SIZE)&&!memcmp(snapshot[1],memory[cc.stage_page],PAGE_SIZE)&&
          !memcmp(snapshot[2],memory[build.map_page],PAGE_SIZE)&&!memcmp(snapshot[3],memory[cc.workspace],PAGE_SIZE));
    check(cc_members_flush(&member_store,&member_record,0,0));
    check(member_record.offset==n&&member_store.length==2*n);
    check(!memcmp(files[file].bytes,files[file].bytes+n,n));member_close();
    for(i=1;i<=ops;++i) {
        member_setup();prior=member_record;fail_at=i;
        check(!cc_members_flush(&member_store,&member_record,0,0)&&platform_error&&member_store.failed&&!member_store.length);
        member_unchanged(&prior);poisoned=member_store;
        at=events;check(!cc_members_flush(&member_store,&member_record,0,0)&&events==at&&!memcmp(&poisoned,&member_store,sizeof(poisoned)));
        member_close();
    }
    for(i=1;i<=poll_count;++i) {
        member_setup();prior=member_record;cancel_at=i;
        check(!cc_members_flush(&member_store,&member_record,0,0)&&output_interrupt_reason()==1&&member_store.failed);
        member_unchanged(&prior);member_close();
    }
    member_setup();shorts=1;check(cc_members_flush(&member_store,&member_record,0,0));member_close();
    member_setup();check(page_drop(cc.stage_page));cc.stage_page=255;cc.emitted=0xb101;
    member_record.origin=0xb000;member_record.entry=0xb000;
    check(cc_members_flush(&member_store,&member_record,0,0)&&member_record.bytes==257);member_close();
    for(i=1;i<=5;++i) {
        member_setup();prior=member_record;bad_io=i;fail_write=fail_read=1;
        check(!cc_members_flush(&member_store,&member_record,0,0)&&platform_error&&member_store.failed);
        member_unchanged(&prior);member_close();
    }
    /* Only the member suffix belongs on disk, even with maximal retained
     * parent tables. Aggregate spool positions must survive crossing 64 KiB. */
    member_setup();member_record.origin=0x8000;member_record.entry=0x8000;cc.emitted=CC_STATIC_LIMIT;
    cc.entries=(PAGE_SIZE-96-cc.file_bytes)/16;member_record.map_first=cc.entries;
    cc.relocations=(PAGE_SIZE-cc.routine_count*ROUTINE_BYTES)/RELOCATION_BYTES;member_record.reloc_first=cc.relocations;
    check(cc_members_flush(&member_store,&member_record,0,0)&&member_record.bytes==0x7400&&!member_record.maps&&!member_record.relocs);
    check(cc_members_flush(&member_store,&member_record,0,0));check(cc_members_flush(&member_store,&member_record,0,0));
    check(member_record.offset>0xffffu||member_store.length>0xffffu);member_close();
    member_setup();prior=member_record;member_record.origin=0x7fff;prior=member_record;at=events;
    check(!cc_members_flush(&member_store,&member_record,0,0)&&events==at&&member_store.failed);member_unchanged(&prior);member_close();
    member_setup();files[find("T:/PRIVATE.SPL")].length=1;prior=member_record;
    check(!cc_members_flush(&member_store,&member_record,0,0)&&member_store.failed&&!member_store.length);member_unchanged(&prior);member_close();
    member_setup();member_store.length=0x7ffffffful;prior=member_record;at=events;
    check(!cc_members_flush(&member_store,&member_record,0,0)&&member_store.failed&&events==at);member_unchanged(&prior);member_close();
    member_setup();member_close();member_fd=open("T:/PRIVATE.SPL",O_RDWR);check(member_fd>=0);
    files[find("T:/PRIVATE.SPL")].length=1;
    check(!cc_members_begin(&member_store,member_fd)&&member_store.failed);member_close();
}
static CompilerGroups groups;
static void group_setup(void) {
    member_setup();cc.routine_count=255;cc.emitted=0xbff0;cc.entries=2;cc.relocations=3;
    cc_groups_init(&groups);check(cc_groups_begin(&groups,member_fd));
}
static void group_cleanup(void) {
    unsigned pages=owned(),released=1+(groups.late.page!=255);output_job_end();fail_at=0;
    check(cc_groups_discard(&groups));check(!opened()&&owned()+released==pages);
    check(groups.page==255&&groups.store.fd==-1&&!groups.lifetime);
}
static void group_code(unsigned bytes,uint8_t value,unsigned maps,unsigned relocs) {
    unsigned i;uint16_t p;
    for(i=0;i<bytes;++i) {
        p=cc.emitted++;
        if(p<0xc000)far_set(build.pages[2],p-0x8000,(uint8_t)(value+i));
        else far_set(cc.stage_page,p-0xc000,(uint8_t)(value+i));
    }
    cc.entries+=maps;cc.relocations+=relocs;
}
static void group_spool(void) {
    CompilerGroup root,child;CompilerMember member;
    unsigned i,page,pages;uint8_t id,ordinal;int f;
    static uint8_t map_before[PAGE_SIZE],workspace_before[PAGE_SIZE];
    group_setup();pages=owned();
    memcpy(map_before,memory[build.map_page],PAGE_SIZE);memcpy(workspace_before,memory[cc.workspace],PAGE_SIZE);
    check(cc_group_open(&groups,0)==0);check(cc_group_member_begin(&groups,0,1));
    group_code(8,0x10,1,1);
    check(cc_group_open(&groups,1)==1);check(cc_group_member_begin(&groups,1,2));
    group_code(20,0x30,2,2);check(cc_group_member_end(&groups,1,2,0xbffc));
    check(cc.emitted==0xbff8&&cc.entries==3&&cc.relocations==4&&groups.active==1);
    check(cc_group_member_begin(&groups,1,3));group_code(40,0x40,1,1);
    check(cc_group_member_end(&groups,1,3,0xc000));check(cc_group_close(&groups,1));
    check(cc.emitted==0xc020&&cc.entries==3&&cc.relocations==4&&groups.active==1);
    group_code(8,0x60,1,1);check(cc_group_member_end(&groups,0,1,0xc020));
    check(cc.emitted==0xbff0&&cc.entries==2&&cc.relocations==3&&!groups.active);
    check(cc_group_member_begin(&groups,0,4));group_code(24,0x70,1,1);
    check(cc_group_member_end(&groups,0,4,0xbff0));check(cc_group_close(&groups,0));
    check(cc.emitted==0xc028&&cc.entries==2&&cc.relocations==3&&!groups.active&&owned()==pages);
    check(cc_group_get(&groups,0,&root)&&root.origin==0xbff0&&root.bytes==56&&root.members==2&&
          root.parent_group==255&&root.parent_member==255&&root.owner==0&&root.parent==0&&root.closed);
    check(cc_group_get(&groups,1,&child)&&child.origin==0xbff8&&child.bytes==40&&child.members==2&&
          child.parent_group==0&&child.parent_member==0&&child.owner==1&&child.parent==1&&child.closed);
    check(cc_group_member_get(&groups,1,&id,&ordinal,&member)&&id==0&&ordinal==0&&member.offset==168&&
          member.origin==0xbff0&&member.bytes==56&&member.entry==0xc020&&member.maps==2&&member.relocs==2);
    f=find("T:/PRIVATE.SPL");check(f>=0&&files[f].length==356&&groups.store.length==356);
    for(i=0;i<8;++i)check(files[f].bytes[member.offset+24+i]==0x10+i);
    for(i=8;i<48;++i)check(!files[f].bytes[member.offset+24+i]);
    for(i=48;i<56;++i)check(files[f].bytes[member.offset+24+i]==0x60+i-48);
    for(i=0;i<PAGE_SIZE;++i) {
        check(memory[build.pages[2]][i]==(i>=0x3ff0?0:(uint8_t)(i*3+9)));
        check(memory[cc.stage_page][i]==(i<40?0:(uint8_t)(i*5+1)));
    }
    check(!memcmp(map_before,memory[build.map_page],PAGE_SIZE)&&!memcmp(workspace_before,memory[cc.workspace],PAGE_SIZE));
    check(!cc_group_member_get(&groups,5,&id,&ordinal,&member)&&!groups.store.failed);
    group_cleanup();

    group_setup();cc.emitted=0x9000;check(cc_group_open(&groups,0)==0);
    for(i=1;i<=255;++i) {
        check(cc_group_member_begin(&groups,0,(uint8_t)i));group_code(1,(uint8_t)i,0,0);
        check(cc_group_member_end(&groups,0,(uint8_t)i,0x9000));
    }
    check(cc_group_close(&groups,0));check(cc_group_get(&groups,0,&root)&&root.members==255&&root.bytes==1);
    check(cc_group_member_get(&groups,255,&id,&ordinal,&member)&&id==0&&ordinal==254&&member.offset==254UL*25u);
    check(groups.store.length==255UL*25u);group_cleanup();

    group_setup();cc.emitted=0x8000;
    for(i=0;i<100;++i) {
        check(cc_group_open(&groups,0)==i);check(cc_group_member_begin(&groups,(uint8_t)i,(uint8_t)(i+1)));
        group_code(4,(uint8_t)i,0,0);check(cc_group_member_end(&groups,(uint8_t)i,(uint8_t)(i+1),(uint16_t)(0x8000+i*4)));
        check(cc_group_close(&groups,(uint8_t)i));
    }
    check(cc_group_open(&groups,0)==255&&groups.store.failed&&platform_error==ERR_NO_MORE_ENTRIES);group_cleanup();

    /* Failed append cannot retire the active member, including its source
     * mapping/relocation suffixes. The partial spool remains owned. */
    group_setup();check(cc_group_open(&groups,0)==0&&cc_group_member_begin(&groups,0,1));
    group_code(24,0x55,1,1);bad_io=1;fail_write=1;writes=0;
    check(!cc_group_member_end(&groups,0,1,0xbff0)&&groups.store.failed&&groups.active==1&&
          cc.emitted==0xc008&&cc.entries==3&&cc.relocations==4&&!groups.store.length);
    group_cleanup();
    group_setup();check(cc_group_open(&groups,0)==0&&cc_group_member_begin(&groups,0,1));
    group_code(8,0x55,0,0);check(cc_group_open(&groups,1)==1);
    check(!cc_group_member_end(&groups,0,1,0xbff0)&&groups.store.failed&&!groups.store.length);group_cleanup();
    group_setup();check(cc_group_open(&groups,0)==0);++cc.emitted;
    check(!cc_group_member_begin(&groups,0,1)&&groups.store.failed&&!groups.active);group_cleanup();
    group_setup();check(cc_group_open(&groups,0)==0&&cc_group_member_begin(&groups,0,1));
    check(!cc_group_close(&groups,0)&&groups.store.failed&&groups.active==1);group_cleanup();
    group_setup();far_set(groups.page,0,1);check(cc_group_open(&groups,0)==255&&groups.store.failed);group_cleanup();

    /* Cleanup never releases a recycled page, and a genuine free failure
     * retains its lifetime-bound owner for retry without re-closing a handle. */
    group_setup();page=groups.page;check(page_drop((uint8_t)page));
    check(page_new(PAGE_COMPILER)==page&&page_lifetime((uint8_t)page)!=groups.lifetime);
    output_job_end();check(!cc_groups_discard(&groups)&&!opened()&&page_role((uint8_t)page)==PAGE_COMPILER);
    check(page_drop((uint8_t)page));cc_groups_init(&groups);
    group_setup();page=groups.page;drop_failure=(int)page;output_job_end();
    check(!cc_groups_discard(&groups)&&groups.page==page&&groups.lifetime&&!opened());
    drop_failure=-1;check(cc_groups_discard(&groups)&&groups.page==255&&!opened());
    group_setup();check(cc_group_open(&groups,0)==0);output_job_end();fail_at=events+1;
    check(!cc_groups_discard(&groups)&&platform_error&&!opened()&&groups.page==255&&
          groups.store.fd==-1&&!groups.groups&&!groups.active&&!groups.lifetime);
    fail_at=0;member_fd=open("T:/PRIVATE.SPL",O_RDWR);check(member_fd>=0);
    check(cc_groups_begin(&groups,member_fd)&&!groups.groups&&!groups.active);group_cleanup();

    member_setup();cc_groups_init(&groups);allocation_failure=1;pages=owned();
    check(!cc_groups_begin(&groups,member_fd)&&groups.store.fd==member_fd&&groups.page==255&&owned()==pages);
    allocation_failure=0;output_job_end();check(cc_groups_discard(&groups)&&!opened());
    member_setup();cc_groups_init(&groups);cancel_at=1;pages=owned();
    check(!cc_groups_begin(&groups,member_fd)&&output_interrupt_reason()==1&&groups.page!=255&&!platform_error);
    output_job_end();check(cc_groups_discard(&groups)&&owned()==pages&&!opened());
}
static unsigned raw_owned(void) {unsigned i,n=0;for(i=32;i<64;++i)n+=roles[i]!=0;return n;}
static void phase_prove(void) {
    unsigned i,j,n=0;uint8_t page;
    check(code_owned==code_required);
    for(i=0;i<code_owned;++i) {
        page=code_pages[i];if(page==255)continue;
        check(page>=32&&page<64&&roles[page]==128);++n;
        for(j=0;j<i;++j)check(code_pages[j]!=page);
        if(i<CODE_PHASE_FIRST)for(j=0;j<code_sizes[i];++j)check(memory[page][j]==(uint8_t)(i*19+j*7+3));
    }
    for(i=CODE_PHASE_FIRST;i<code_owned;++i) {
        page=code_phase_pending[i-CODE_PHASE_FIRST];if(page==255)continue;
        check(page>=32&&page<64&&roles[page]==128&&code_pages[i]==255);++n;
        for(j=0;j<code_owned;++j)check(code_pages[j]!=page);
        for(j=CODE_PHASE_FIRST;j<i;++j)check(code_phase_pending[j-CODE_PHASE_FIRST]!=page);
    }
    check(raw_owned()==n&&!memcmp(&source,&before,sizeof(source))&&
          !memcmp(memory[source.page],fresh,sizeof(fresh))&&equals("T:/ZPKEEP/FOREIGN",(const uint8_t*)"keep",4));
}
static void phase_setup(void) {
    unsigned i,j,at=16+2*code_required;uint8_t page;int f;int8_t fd;
    check(!code_owned&&code_phase_fd<0);
    allocation_failure=0;phase_free_failure=-1;exec_finished=0;exec_calls=0;
    exec_corrupt_phase=exec_error=exec_corrupt_map=exec_corrupt_image=exec_allocation_failure=0;
    fail_at=cancel_at=shorts=bad_io=fail_read=fail_write=0;setup(0,0);build.map_page=255;
    memset(code_pages,255,sizeof(code_pages));memset(code_sizes,0,sizeof(code_sizes));
    f=make("T:/PHASE.ZPL",0);memset(files[f].bytes,0,BYTES);
    for(i=0;i<code_required;++i) {
        check(palloc(&page)==0);code_pages[code_owned++]=page;
        code_sizes[i]=i<13?17:i==13?300:417;
        for(j=0;j<code_sizes[i];++j)files[f].bytes[at+j]=memory[page][j]=(uint8_t)(i*19+j*7+3);
        at+=code_sizes[i];
    }
    files[f].length=at;fd=open("T:/PHASE.ZPL",O_RDONLY);check(fd>=0);
    code_phase_bind(fd);check(code_phase_fd==fd);output_job_begin(1);events=reads=polls=0;
}
static void phase_cleanup(void) {
    unsigned i;fail_at=0;phase_free_failure=-1;allocation_failure=0;output_job_end();
    phase_prove();
    for(i=CODE_PHASE_FIRST;i<code_owned;++i)if(code_phase_pending[i-CODE_PHASE_FIRST]!=255) {
        check(!pfree(code_phase_pending[i-CODE_PHASE_FIRST]));code_phase_pending[i-CODE_PHASE_FIRST]=255;
    }
    for(i=0;i<code_owned;++i)if(code_pages[i]!=255) {check(!pfree(code_pages[i]));code_pages[i]=255;}
    code_owned=0;
    if(code_phase_fd>=0) {check(!close(code_phase_fd));code_phase_fd=-1;}
    check(!raw_owned()&&!opened());
}
/* Shared cases use the production emitter, not a second word/chain model. */
#include "late-cases.h"
static void late_spool(void) {
    char path[]="H:/LATE.SPL",helpers[]="H:/HELPERS.SPL",owners[]="H:/OWNERS.SPL";
    member_setup();late_cases(path,&late_diagnostic);
    check(files[find(path)].length==97);
    check(files[find(path)].bytes[18]==2&&files[find(path)].bytes[19]==0);
    check(!memcmp(files[find(path)].bytes+24,"\xcd\0\0\0\0\0\0\xcd\0\0",10));
    root_helper_cases(helpers,&late_diagnostic);
    remaining_helper_cases(owners,&late_diagnostic);
    member_address_guard(&late_diagnostic);
    member_close();
}
static uint16_t late_setup(CompilerLate *late) {
    member_setup();memset(&late_diagnostic,0,sizeof(late_diagnostic));memset(&late_sources,0,sizeof(late_sources));
    cc.error=&late_diagnostic;cc.sources=&late_sources;cc.type_count=TYPE_LIMIT;
    cc_late_init(late);return cc_late_symbol(late,LATE_ROOT,LATE_PARAMETERS,0);
}
static void late_close(CompilerLate *late) {
    check(cc_late_discard(late)&&late->page==255);member_close();
}
static void late_failures(void) {
    CompilerLate late;CompilerMember prior;
    uint16_t symbol,other,parent,page,i,before_refs;unsigned ops,j;uint8_t copy[4];
    static uint8_t snapshot[PAGE_SIZE];
    symbol=late_setup(&late);check(symbol==0);
    check(cc_late_reference(&late,0xbfff,symbol,1,0));
    check(cc_members_flush(&member_store,&member_record,&late,0)&&member_record.lates==1);ops=events;
    check(late.references==1);late_close(&late);
    /* Every real stream fault preserves the reference suffix and the parent
     * emitter; only the verified group commit may retire either of them. */
    for(j=1;j<=ops;++j) {
        symbol=late_setup(&late);check(cc_late_reference(&late,0xbfff,symbol,1,0));
        prior=member_record;fail_at=j;
        check(!cc_members_flush(&member_store,&member_record,&late,0)&&member_store.failed&&late.references==1);
        member_unchanged(&prior);fail_at=0;late_close(&late);
    }
    /* Invalidating the journal on ANY write/read/seek must prevent flush
     * success, including the final seek after all bytes verified correctly. */
    for(j=1;j<=ops;++j) {
        symbol=late_setup(&late);check(cc_late_reference(&late,0xbfff,symbol,1,0));
        prior=member_record;stream_page=late.page;stream_seal=0;stream_mutate_at=j;
        check(!cc_members_flush(&member_store,&member_record,&late,0)&&!stream_mutate_at&&
              member_store.failed&&late.failed&&late.references==1&&!member_store.length);
        member_unchanged(&prior);late_close(&late);
    }
    symbol=late_setup(&late);other=cc_late_symbol(&late,LATE_ROOT,LATE_RANDOM,0);
    check(cc_late_bind(&late,symbol,0,0xc100));
    memcpy(copy,memory[build.pages[2]]+0x3ffe,2);memcpy(copy+2,memory[cc.stage_page],2);
    check(cc_late_reference(&late,0xbfff,symbol,65535u,0));
    check(cc_late_reference(&late,0xc000,other,1,1));
    check(!cc_late_finish(&late)&&late.failed); /* missing owner: no first patch */
    check(!memcmp(copy,memory[build.pages[2]]+0x3ffe,2)&&!memcmp(copy+2,memory[cc.stage_page],2));late_close(&late);
    symbol=late_setup(&late);check(cc_late_bind(&late,symbol,0,0xc100));
    i=memory[build.pages[2]][0x3fff]|((uint16_t)memory[cc.stage_page][0]<<8);
    check(cc_late_reference(&late,0xbfff,symbol,65535u,0)&&cc_late_finish(&late));
    check((memory[build.pages[2]][0x3fff]|((uint16_t)memory[cc.stage_page][0]<<8))==(uint16_t)(i-0xc100u));late_close(&late);
    for(j=0;j<2;++j) {
        symbol=late_setup(&late);
        for(i=0;i<2047;++i)check(cc_late_reference(&late,0xbfff,symbol,1,0));
        check(cc_late_symbol(&late,LATE_ROOT,LATE_PARAMETERS,0)==symbol&&late.symbols==1&&late.references==2047);
        memcpy(snapshot,memory[late.page],PAGE_SIZE);
        check(j?cc_late_symbol(&late,LATE_ROOT,LATE_RANDOM,0)==LATE_NONE:!cc_late_reference(&late,0xbfff,symbol,1,0));
        check(late.failed&&platform_error==ERR_NO_MORE_MEMORY&&late.symbols==1&&late.references==2047&&
              !memcmp(snapshot,memory[late.page],PAGE_SIZE));late_close(&late);
    }
    symbol=late_setup(&late);parent=0x1234;
    /* Cyclic, forward and escaped chain links must be rejected before any
     * operand is zeroed or any suffix record is appended. */
    for(j=0;j<3;++j) {
        if(j) {late_close(&late);symbol=late_setup(&late);}
        cc_word(0xbfff,j==0?0xbfff:j==1?0xc001:0x7fff);
        memcpy(snapshot,memory[build.pages[2]],PAGE_SIZE);before_refs=late.references;
        check(!cc_late_chain(&late,0xbff0,0xbfff,symbol,&parent)&&parent==0x1234&&late.references==before_refs);
        check(!memcmp(snapshot,memory[build.pages[2]],PAGE_SIZE));
    }
    late_close(&late);
    symbol=late_setup(&late);page=late.page;far_set((uint8_t)page,7,1);
    check(!cc_late_reference(&late,0xbfff,symbol,1,0)&&late.failed);late_close(&late);
    symbol=late_setup(&late);page=late.page;check(page_drop((uint8_t)page));
    check(page_new(PAGE_COMPILER)==page&&!cc_late_discard(&late)&&page_role((uint8_t)page)==PAGE_COMPILER);
    check(page_drop((uint8_t)page));cc_late_init(&late);member_close();
    symbol=late_setup(&late);page=late.page;drop_failure=page;
    check(!cc_late_discard(&late)&&late.page==page&&page_role((uint8_t)page)==PAGE_COMPILER);
    drop_failure=-1;late_close(&late);
    member_setup();cc_late_init(&late);allocation_failure=1;
    check(cc_late_symbol(&late,LATE_ROOT,LATE_PARAMETERS,0)==LATE_NONE&&late.failed&&late.page==255);
    allocation_failure=0;late_close(&late);
    group_setup();symbol=cc_late_symbol(&groups.late,LATE_ROOT,LATE_PARAMETERS,0);
    check(cc_group_open(&groups,0)==0&&cc_group_member_begin(&groups,0,1));
    group_code(24,0x55,1,1);check(cc_late_reference(&groups.late,0xbff1,symbol,1,0));
    bad_io=1;fail_write=1;writes=0;
    check(!cc_group_member_end(&groups,0,1,0xbff0)&&groups.store.failed&&groups.active==1&&
          cc.emitted==0xc008&&cc.entries==3&&cc.relocations==4&&groups.late.references==1&&!groups.store.length);
    group_cleanup();
    group_setup();memset(&late_diagnostic,0,sizeof(late_diagnostic));memset(&late_sources,0,sizeof(late_sources));
    cc.error=&late_diagnostic;cc.sources=&late_sources;
    cc_hook_output();parent=cc.hook_output_calls;
    check(cc_group_open(&groups,0)==0&&cc_group_member_begin(&groups,0,1));
    cc_hook_output();other=cc.hook_output_calls;bad_io=1;fail_write=1;writes=0;
    check(!cc_group_member_end(&groups,0,1,0xbff3)&&groups.store.failed&&groups.active==1&&
          groups.late.references==1&&cc.hook_output_calls==other&&cc.emitted==0xbff6);
    check(cc_code_get(parent)==0&&cc_code_get(parent+1)==0);group_cleanup();
    /* Catalog invalidation or a sealed journal can happen while I/O succeeds.
     * Neither can publish a committed member, adopt chain heads or retire code. */
    for(j=0;j<2;++j) {
        group_setup();memset(&late_diagnostic,0,sizeof(late_diagnostic));memset(&late_sources,0,sizeof(late_sources));
        cc.error=&late_diagnostic;cc.sources=&late_sources;
        check(cc_group_open(&groups,0)==0&&cc_group_member_begin(&groups,0,1));
        cc_hook_output();parent=cc.hook_output_calls;
        stream_page=groups.page;stream_late=&groups.late;stream_seal=(uint8_t)j;stream_mutate_at=events+2;
        memcpy(snapshot,memory[groups.page],PAGE_SIZE);
        check(!cc_group_member_end(&groups,0,1,0xbff0)&&!stream_mutate_at&&groups.store.failed&&groups.active==1&&
              groups.late.references==1&&cc.hook_output_calls==parent&&cc.emitted==0xbff3&&
              !memcmp(snapshot,memory[groups.page],PAGE_SIZE));
        group_cleanup();
    }
    symbol=late_setup(&late);check(cc_late_reference(&late,0xbfff,symbol,1,0));
    late.sealed=1;
    check(!cc_late_retire(&late,0)&&late.failed&&late.references==1);late_close(&late);
    symbol=late_setup(&late);
    check(cc_late_symbol(&late,LATE_BRIDGE,2,1)==LATE_NONE&&late.failed);late_close(&late);
    group_setup();memset(&late_diagnostic,0,sizeof(late_diagnostic));cc.error=&late_diagnostic;cc.sources=&late_sources;
    check(cc_group_open(&groups,0)==0&&cc_group_member_begin(&groups,0,1));
    cc_hook_output();parent=cc.hook_output_calls;cc_word(parent,parent);
    check(!cc_group_member_end(&groups,0,1,0xbff0)&&groups.store.failed&&groups.active==1&&
          !groups.late.references&&cc.hook_output_calls==parent&&cc.emitted==0xbff3);
    check((cc_code_get(parent)|((uint16_t)cc_code_get(parent+1)<<8))==parent);group_cleanup();
}
static void phase_cache(void) {
    unsigned i,total,at;int f;uint8_t status;
    phase_setup();check(code_phase_park()&&raw_owned()==13&&code_pages[13]==255&&code_pages[14]==255);
    events=0;check(code_phase_restore()&&raw_owned()==15);total=events;
    at=events;check(code_phase_restore()&&events==at&&!polls);
    for(i=13;i<15;++i) {
        unsigned j;for(j=0;j<code_sizes[i];++j)check(memory[code_pages[i]][j]==(uint8_t)(i*19+j*7+3));
    }
    phase_cleanup();
    for(i=1;i<=total;++i) {
        phase_setup();check(code_phase_park());events=0;fail_at=i;
        check(!code_phase_restore()&&platform_error&&boot_error);phase_prove();
        fail_at=0;check(code_phase_restore()&&raw_owned()==15);phase_cleanup();
    }
    phase_setup();check(code_phase_park());shorts=1;
    check(code_phase_restore()&&raw_owned()==15&&!polls);phase_cleanup();
    for(i=3;i<=4;++i) {
        phase_setup();check(code_phase_park());bad_io=(uint8_t)i;fail_read=1;reads=0;
        check(!code_phase_restore()&&platform_error==ERR_ENTRY_CORRUPTED&&raw_owned()==13);
        bad_io=fail_read=0;check(code_phase_restore()&&raw_owned()==15);phase_cleanup();
    }
    /* Payload bytes are trusted; a successful read is not rehashed. */
    phase_setup();check(code_phase_park());bad_io=5;fail_read=1;reads=0;
    check(code_phase_restore()&&raw_owned()==15);bad_io=fail_read=0;phase_cleanup();
    /* Original opened-handle association survives replacement of its name. */
    phase_setup();f=fds[(uint8_t)code_phase_fd].file;
    strcpy(files[f].path,"T:/ORIGINAL.ZPL");seed("T:/PHASE.ZPL",(const uint8_t*)"wrong",5);
    check(code_phase_park()&&code_phase_restore()&&equals("T:/PHASE.ZPL",(const uint8_t*)"wrong",5));phase_cleanup();
    phase_setup();check(code_phase_park());f=fds[(uint8_t)code_phase_fd].file;
    at=files[f].length;files[f].length=16+2*code_required+13*17;phase_free_failure=45;
    check(!code_phase_restore()&&boot_error==ERR_ENTRY_CORRUPTED&&code_pages[13]==255&&code_phase_pending[0]==45&&raw_owned()==14);
    i=reads;check(!code_phase_restore()&&reads>i&&boot_error==ERR_ENTRY_CORRUPTED&&raw_owned()==14);
    phase_prove();files[f].length=at;phase_free_failure=-1;
    check(code_phase_restore()&&raw_owned()==15);phase_cleanup();
    phase_setup();phase_free_failure=46;check(!code_phase_park()&&platform_error&&raw_owned()==14);
    phase_free_failure=-1;code_phase_return();check(platform_error&&raw_owned()==15);phase_cleanup();
    phase_setup();check(code_phase_park());f=fds[(uint8_t)code_phase_fd].file;
    files[f].length=1;
    check(!code_phase_restore()&&boot_error==ERR_ENTRY_CORRUPTED&&raw_owned()==13);phase_cleanup();
    phase_setup();check(code_phase_park());check(!close(code_phase_fd));code_phase_fd=-1;
    check(!code_phase_restore()&&platform_error==ERR_INVALID_FILEDEV&&raw_owned()==13);phase_cleanup();
    /* Restoring an idle compiler cache cannot change a completed child's
     * native result/status or its primary platform error. */
    phase_setup();status=255;exec_corrupt_phase=1;platform_error=0;
    strcpy((char*)console_scratch,target[0]);
    check(!execute_native((char*)console_scratch,&status)&&status==7&&!platform_error&&boot_error==ERR_ENTRY_CORRUPTED&&raw_owned()==13);
    f=fds[(uint8_t)code_phase_fd].file;files[f].length=16+2*code_required+13*17+300+417;
    check(code_phase_restore());phase_cleanup();
    phase_setup();status=255;exec_error=ERR_NOT_SUPPORTED;
    check(execute_native(target[0],&status)==ERR_NOT_SUPPORTED&&status==255&&raw_owned()==15);phase_cleanup();
}
#include "output-path-cases.h"
int main(void) {
    output_path_cases();prepared_output_cases();single_page_block_read();
    unsigned kind,mask,pages,total_events,total_polls,i;uint8_t ok;
    for(kind=0;kind<5;++kind)for(mask=0;mask<(kind==2?2u:4u);++mask) {
        fail_at=cancel_at=rollback_fault=shorts=bad_io=fail_read=fail_write=0;setup(kind,mask);pages=owned();
        ok=save();check(ok);total_events=events;total_polls=polls;prove(ok,pages,0);
        for(i=1;i<=total_polls;++i) {
            cancel_at=i;setup(kind,mask);ok=save();check(!ok&&output_interrupt_reason()==1);prove(ok,pages,0);
        }
        cancel_at=0;
        for(i=1;i<=total_events;++i) {
            fail_at=i;setup(kind,mask);ok=save();check(!output_interrupt_reason());prove(ok,pages,0);
            if(!ok)check(platform_error);
        }
        fail_at=0;shorts=1;setup(kind,mask);ok=save();check(ok);prove(ok,pages,0);
        shorts=0;cancel_reason=2;cancel_at=total_polls;setup(kind,mask);ok=save();check(!ok&&output_interrupt_reason()==2);prove(ok,pages,0);cancel_reason=1;
        if(kind<2&&(mask&1)) {
            rollback_fault=1;cancel_at=total_polls;setup(kind,mask);ok=save();
            check(!ok&&output_interrupt_reason());prove(ok,pages,1);
            check(equals(target[1],old[0],old_length[0]));
        }
        cancel_at=rollback_fault=0;
        for(i=1;i<=2;++i) {
            bad_io=i;fail_write=fail_read=1;setup(kind,mask);ok=save();check(!ok&&platform_error);prove(ok,pages,0);
        }
    }
    /* A denied BAK prompt must not allocate a transaction or poll input. */
    bad_io=fail_write=fail_read=0;setup(0,3);pages=owned();check(!save_source(&source,0,1)&&platform_error==ERR_ALREADY_EXIST);check(!polls);prove(0,pages,0);
    /* Loading a missing path never creates it. */
    {Diagnostic d;setup(0,0);pages=owned();check(load_source(&source,"T:/absent/path/file.pas",&d));check(find("T:/absent")<0&&owned()==pages);}
    /* Error navigation requires an existing source. Missing/read/close failures
     * preserve the exact dirty Work, bytes, page ownership and open handles. */
    {
        Diagnostic d;Source prior;unsigned fault,operations;
        setup(0,0);seed("T:/include.pas",(const uint8_t*)"begin end.\n",11);
        pages=owned();check(reload_source(&source,"T:/include.pas",&d));
        operations=events;
        check(!source.dirty&&source.length==11&&owned()==pages&&!opened());
        for(fault=0;fault<=operations;++fault) {
            setup(0,0);prior=source;pages=owned();
            seed("T:/include.pas",(const uint8_t*)"begin end.\n",11);
            fail_at=fault;
            check(!reload_source(&source,fault?"T:/include.pas":"T:/missing.pas",&d));
            check(!memcmp(&source,&prior,sizeof(source))&&owned()==pages&&!opened());
            check(!memcmp(memory[source.page],fresh,sizeof(fresh)));
            bad_io=fail_read=fail_at=0;
        }
        check(reload_source(&source,"T:/include.pas",&d));
        check(!source.dirty&&source.length==11&&owned()==pages&&!opened());
    }
    saved_cache();
    member_spool();
    group_spool();
    late_spool();
    late_failures();
    phase_cache();
    printf("PASS: %u output assertions across %u publication/fault/cancellation scenarios\n",assertions,scenarios);
    return 0;
}
