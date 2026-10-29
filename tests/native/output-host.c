/* Execute product publication against a deterministic VFS. Faults are injected
 * at API boundaries, independently of the implementation's transaction stages. */
#include "zp.h"
#include "output_transport.h"
#include <zos_vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define exit zos_exit
#include <zos_sys.h>
#undef exit

#define FILES 40
#define BYTES 20000
static struct { char path[128]; uint8_t bytes[BYTES],dir; uint16_t length; } files[FILES];
static struct { int file; uint16_t pos; uint8_t flags,used; } fds[12];
static uint8_t memory[16][PAGE_SIZE],roles[16];
volatile uint32_t page_versions[64];
uint8_t console_scratch[256],platform_error;
static unsigned assertions,scenarios,events,polls,fail_at,cancel_at;
static uint8_t shorts,cancel_reason=1,rollback_fault,bad_io;
static unsigned writes,reads,fail_write,fail_read;
static unsigned case_kind,case_old;
static int drop_failure=-1;
static uint8_t allocation_failure,exec_allocation_failure,exec_corrupt_map,exec_corrupt_image,exec_error,exec_finished;
static unsigned exec_calls,cache_checks,cache_closes;
static Source source,before;
static Build build;
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
uint8_t page_new(uint8_t role) {uint8_t i;if(allocation_failure)return 255;for(i=0;i<16;++i)if(!roles[i]){roles[i]=role;memset(memory[i],0xa5,PAGE_SIZE);return i;}return 255;}
uint8_t page_drop(uint8_t p) {assert(p<16&&roles[p]);if(p==drop_failure){platform_error=ERR_FAILURE;return 0;}roles[p]=0;return 1;}
uint8_t page_role(uint8_t p) {return p<16?roles[p]:0;}
uint32_t page_lifetime(uint8_t p) {return page_role(p)?1:0;}
uint8_t far_get(uint8_t p,uint16_t at) {assert(p<16&&roles[p]&&at<PAGE_SIZE);return memory[p][at];}
void far_set(uint8_t p,uint16_t at,uint8_t b) {assert(p<16&&roles[p]&&at<PAGE_SIZE);memory[p][at]=b;++page_versions[p];}
void page_initialized(uint8_t p,uint16_t n) {assert(page_role(p)&&n<=PAGE_SIZE);}
uint8_t key_interrupt(void) {platform_error=0;return ++polls==cancel_at?cancel_reason:0;}
uint8_t build_ready(const Source *s,const Build *b) {return s->page==source.page&&b==&build;}
/* This VFS model verifies cache policy, not the compiler's cryptography. Native
 * execution and shared-front-end tests use the actual image/map authenticator. */
uint8_t build_check(const Source *s,const Build *b) {
    unsigned i;++cache_checks;
    if(!build_ready(s,b)||!b->valid||page_role(b->map_page)!=PAGE_MAP)return 0;
    for(i=0;i<b->map_length;++i)if(far_get(b->map_page,i)!=(uint8_t)(i*13+1))return 0;
    for(i=0;i<b->length;++i)if(far_get(b->pages[i/PAGE_SIZE],i%PAGE_SIZE)!=(uint8_t)(i*11+3))return 0;
    return 1;
}
void build_close(Build *b) {
    unsigned i;++cache_closes;b->valid=0;
    for(i=0;i<3;++i)if(b->pages[i]!=255&&page_drop(b->pages[i]))b->pages[i]=255;
    if(b->map_page!=255&&page_drop(b->map_page))b->map_page=255;
}
zos_err_t curdir(char *path) {strcpy(path,"T:/");return 0;}
zos_err_t exec(zos_exec_mode_t mode,const char *path,const char *args[],uint8_t *status) {
    (void)args;++exec_calls;
    check(mode==EXEC_PRESERVE_PROGRAM&&!strcmp(path,target[0]));
    check(!build.valid&&build.map_page==255&&owned()==1);
    if(fault()||exec_error)return exec_error?exec_error:ERR_FAILURE;
    if(exec_corrupt_map)files[find(target[1])].bytes[0]^=1;
    if(exec_corrupt_image)files[find(target[0])].bytes[0]^=1;
    if(exec_allocation_failure)allocation_failure=1;
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
    if(++reads==fail_read&&bad_io==3){++*size;return 0;}
    left=files[f].length-fds[(uint8_t)fd].pos;
    if(n>left)n=left;
    if(shorts&&n>(unsigned)(fd&1?11:7))n=fd&1?11:7;
    memcpy(out,files[f].bytes+fds[(uint8_t)fd].pos,n);fds[(uint8_t)fd].pos+=n;*size=n;return 0;
}
zos_err_t write(zos_dev_t fd,const void *in,uint16_t *size) {
    unsigned n=*size;int f;uint8_t error;
    assert(fd>=2&&fd<12&&fds[(uint8_t)fd].used);f=fds[(uint8_t)fd].file;
    assert(fds[(uint8_t)fd].flags&O_WRONLY);
    error=fault();
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
static int retained(const char *name,const uint8_t *data,unsigned size) {
    char path[128];if(!*output_recovery())return 0;
    strcpy(path,output_recovery());strcat(path,"/");strcat(path,name);return equals(path,data,size);
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
    ++scenarios;check(!opened());check(owned()==pages);check(equals("T:/ZPKEEP/FOREIGN",(const uint8_t*)"keep",4));
    check(!memcmp(memory[source.page],fresh,sizeof(fresh)));
    if(!success) {
        check(!memcmp(&source,&before,sizeof(source)));
        if(!restore_failed)for(i=0;i<(case_kind==2?1u:2u);++i) {
            if(case_old&(1u<<i))check(equals(target[i],old[i],old_length[i]));
            else check(find(target[i])<0);
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
    check(run_saved(&source,&build,&status)&&status==7&&build.valid);
    check(cache_checks==1&&!cache_closes&&owned()==4&&!opened());total=events;
    for(i=1;i<=total;++i) {
        run_setup();fail_at=i;status=255;ok=run_saved(&source,&build,&status);
        check(ok==exec_finished&&status==(exec_finished?7:255));
        check(!memcmp(&source,&before,sizeof(source))&&!memcmp(memory[source.page],fresh,sizeof(fresh))&&!opened());
        check(owned()==(build.valid?4u:1u));
        if(!ok)check(platform_error&&build.valid);
        if(build.valid)check(build.versions[3]==page_versions[build.map_page]||!exec_calls);
        else check(cache_closes==1&&build.map_page==255);
    }
    run_setup();exec_corrupt_map=1;status=255;
    check(run_saved(&source,&build,&status)&&status==7&&!build.valid&&cache_checks==1&&cache_closes==1&&owned()==1);
    run_setup();exec_corrupt_image=1;status=255;
    check(run_saved(&source,&build,&status)&&status==7&&!build.valid&&cache_checks==1&&cache_closes==1&&owned()==1);
    run_setup();exec_allocation_failure=1;status=255;
    check(run_saved(&source,&build,&status)&&status==7&&!build.valid&&cache_closes==1&&owned()==1);
    run_setup();exec_error=ERR_NOT_SUPPORTED;status=255;
    check(!run_saved(&source,&build,&status)&&status==255&&platform_error==ERR_NOT_SUPPORTED&&build.valid&&owned()==4);
    run_setup();drop_failure=build.map_page;status=255;
    check(!run_saved(&source,&build,&status)&&status==255&&!exec_calls&&build.valid&&owned()==4);
    drop_failure=-1;
    run_setup();drop_failure=build.pages[0];status=255;
    check(!run_saved(&source,&build,&status)&&status==255&&!exec_calls&&!build.valid&&owned()==3);
    drop_failure=-1;build_close(&build);check(owned()==1);
}
int main(void) {
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
        {
            char recovery[128];strcpy(recovery,output_recovery());
            cancel_at=0;ok=save();check(ok);prove(ok,pages,0);
            check(find(recovery)>=0); /* Retry cleans only its own transaction. */
        }
        cancel_at=total_polls;
        /* After both final writes, cancel and fail every attempted restoration.
         * Recovery copies, source identity and all ownership must survive. */
        if(mask&1) {
            rollback_fault=1;setup(kind,mask);ok=save();check(!ok&&platform_error&&output_interrupt_reason());prove(ok,pages,1);
            if(kind>=3) {
                check(retained(kind==4?"OLD.CHN":"OLD.COM",old[0],old_length[0]));
                check(retained("NEW.ZMP",memory[build.map_page],build.map_length));
            } else {
                check(retained("OLD.PAS",old[0],old_length[0]));
                check(retained("NEW.PAS",kind==2?fresh+17:fresh,kind==2?372:sizeof(fresh)));
                if(kind!=2&&(mask&2))check(retained("PRIOR.BAK",old[1],old_length[1]));
            }
        }
        cancel_at=rollback_fault=0;
        for(i=1;i<=3;++i) {
            bad_io=i;fail_write=fail_read=1;setup(kind,mask);ok=save();check(!ok&&platform_error);prove(ok,pages,0);
        }
    }
    /* A denied BAK prompt must not allocate a transaction or poll input. */
    bad_io=fail_write=fail_read=0;setup(0,3);pages=owned();check(!save_source(&source,0,1)&&platform_error==ERR_ALREADY_EXIST);check(!polls&&!*output_recovery());prove(0,pages,0);
    /* Loading a missing path never creates it. */
    {Diagnostic d;setup(0,0);pages=owned();check(load_source(&source,"T:/absent/path/file.pas",&d));check(find("T:/absent")<0&&owned()==pages);}
    saved_cache();
    printf("PASS: %u output assertions across %u publication/fault/cancellation scenarios\n",assertions,scenarios);
    return 0;
}
