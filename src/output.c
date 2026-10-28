#ifdef __SDCC
#define save_build files_save_build
#define save_source files_save_source
#define save_source_as files_save_source_as
#define load_source files_load_source
#define read_block files_read_block
#define save_block files_save_block
#define run_saved files_run_saved
#define output_path files_output_path
#define output_recovery files_output_recovery
#endif
#include "zp.h"
#include "path.h"
#include "sources.h"
#include "saved_map.h"
#include <zos_vfs.h>
#include <zos_errors.h>
#include <zos_sys.h>

extern uint8_t console_scratch[256];
static char destinations[2][128], transaction[128], member[128];
static uint16_t serial;
static uint8_t transaction_owned;
static char no_recovery[1]; /* Returned pointers must survive a code-bank switch. */
const char *output_path(void) { return destinations[0]; }
const char *output_recovery(void) {
    if(transaction_owned) return transaction;
    return no_recovery;
}

static int8_t open_path(const char *path,uint8_t flags) {
    int8_t fd;
    strcpy((char*)console_scratch,path);
    fd=open((char*)console_scratch,flags);
    if(fd<0) platform_error=(uint8_t)-fd;
    return fd;
}
static uint8_t stat_path(const char *path) {
    strcpy((char*)console_scratch,path);
    return stat((char*)console_scratch,(zos_stat_t*)(console_scratch+128));
}
static uint8_t parent_dirs(const char *path,uint8_t create) {
    uint16_t i;
    uint8_t e;
    strcpy(member,path);
    for(i=3;member[i];++i) if(member[i]=='/') {
        member[i]=0;
        e=stat_path(member);
        if(e==ERR_NO_SUCH_ENTRY&&create) {
            strcpy((char*)console_scratch,member); e=mkdir((char*)console_scratch);
            if(!e) { member[i]='/'; continue; }
            if(e==ERR_ALREADY_EXIST) e=stat_path(member);
        }
        member[i]='/';
        if(e==ERR_NO_SUCH_ENTRY&&!create) return 1;
        if(!e&&!D_ISDIR(((zos_stat_t*)(console_scratch+128))->s_flags)) e=ERR_NOT_A_DIR;
        if(e) { platform_error=e; return 0; }
    }
    return 1;
}
static uint8_t finish(int8_t fd,uint8_t ok) {
    uint8_t prior=platform_error,e=close(fd);
    if(!ok) platform_error=prior?prior:ERR_FAILURE;
    else if(e) { platform_error=e; ok=0; }
    return ok;
}
static uint8_t write_all(int8_t fd,uint16_t n) {
    uint16_t at=0,size;
    while(at<n) {
        size=n-at; platform_error=write(fd,console_scratch+at,&size);
        if(platform_error) return 0;
        if(!size||size>n-at) { platform_error=ERR_FAILURE; return 0; }
        at+=size;
    }
    return 1;
}
static uint8_t paths(const Source *s) {
    uint16_t base;
    transaction[0]=0;
    transaction_owned=0;
    if(strlen(s->path)>123||!saved_paths(s->path,destinations))return 0;
    base=(uint16_t)strlen(destinations[0]);
    while(base&&destinations[0][base-1]!='/')--base;
    if(base+15>=128)return 0;
    memcpy(transaction,s->path,base); transaction[base]=0;
    return 1;
}
static void in_transaction(const char *name) {
    strcpy(member,transaction); strcat(member,"/"); strcat(member,name);
}
static void cleanup_success(uint8_t source,uint8_t members) {
    uint8_t i,e;
    const char *name;
    /* The final files have been closed and independently verified. Remove only
     * this operation's own known members, never old or unrelated ZP folders.
     * A cleanup failure leaves a visible recovery path but does not turn an
     * already verified save into failure. No failure path calls this routine. */
    for(i=0;i<(source?3:4);++i) {
        if(!(members&(1u<<i))) continue;
        if(source) name=i==0?"NEW.PAS":i==1?"OLD.PAS":"PRIOR.BAK";
        else name=i==0?"NEW.COM":i==1?"NEW.ZMP":i==2?"OLD.COM":"OLD.ZMP";
        in_transaction(name);
        strcpy((char*)console_scratch,member); e=rm((char*)console_scratch);
        if(e&&e!=ERR_NO_SUCH_ENTRY) { platform_error=e; return; }
    }
    strcpy((char*)console_scratch,transaction); e=rm((char*)console_scratch);
    if(e) { platform_error=e; return; }
    transaction_owned=0;
}
static uint8_t create_transaction(void) {
    uint16_t base=(uint16_t)strlen(transaction),attempt,value;
    uint8_t j,e;
    /* Reserve the folder, slash and longest recovery member (PRIOR.BAK). */
    if(base+16>127) {platform_error=ERR_INVALID_PATH;return 0;}
    for(attempt=0;attempt<256;++attempt) {
        value=serial++;
        transaction[base]='Z'; transaction[base+1]='P';
        for(j=0;j<4;++j) transaction[base+2+j]="0123456789ABCDEF"[(value>>(12-j*4))&15];
        transaction[base+6]=0;
        /* Drivers need not use ERR_ALREADY_EXIST for mkdir collisions. Never
         * claim an occupied name, and keep unrelated recovery entries intact. */
        e=stat_path(transaction);
        if(!e) continue;
        if(e!=ERR_NO_SUCH_ENTRY) { platform_error=e; return 0; }
        strcpy((char*)console_scratch,transaction);
        e=mkdir((char*)console_scratch);
        if(!e) { transaction_owned=1; return 1; }
        /* An external HostFS writer may have occupied it since the stat. A
         * failed mkdir plus an existing entry is a collision, never ownership. */
        if(e!=ERR_ALREADY_EXIST&&stat_path(transaction)) { platform_error=e; return 0; }
    }
    platform_error=ERR_NO_MORE_ENTRIES; return 0;
}
static uint8_t stream_byte(const uint8_t *pages,uint16_t p) {
    return far_get(pages[p/PAGE_SIZE],p%PAGE_SIZE);
}
static uint8_t store_range(const uint8_t *pages,uint16_t first,uint16_t total,const char *path) {
    uint16_t p=0,n,i;
    int8_t fd=open_path(path,O_WRONLY|O_CREAT|O_TRUNC);
    uint8_t ok=1;
    if(fd<0) return 0;
    while(p<total&&ok) {
        n=total-p; if(n>128) n=128;
        for(i=0;i<n;++i) console_scratch[i]=stream_byte(pages,first+p+i);
        ok=write_all(fd,n); p+=n;
    }
    return finish(fd,ok);
}
static uint8_t store(const uint8_t *pages,uint16_t total,const char *path) {
    return store_range(pages,0,total,path);
}
static uint8_t verify_range(const uint8_t *pages,uint16_t first,uint16_t total,const char *path) {
    uint16_t p=0,n,i,requested;
    int8_t fd=open_path(path,O_RDONLY);
    uint8_t ok=1;
    if(fd<0) return 0;
    while(p<total&&ok) {
        requested=total-p; if(requested>128) requested=128; n=requested;
        platform_error=read(fd,console_scratch,&n);
        if(platform_error) { ok=0; break; }
        if(!n||n>requested) { ok=0; break; }
        for(i=0;i<n;++i) if(console_scratch[i]!=stream_byte(pages,first+p+i)) ok=0;
        p+=n;
    }
    if(ok) { n=1; platform_error=read(fd,console_scratch,&n); if(platform_error||n) ok=0; }
    if(!ok&&!platform_error) platform_error=ERR_FAILURE;
    return finish(fd,ok);
}
static uint8_t verify(const uint8_t *pages,uint16_t total,const char *path) {
    return verify_range(pages,0,total,path);
}
static uint8_t copy_file(const char *from,const char *to) {
    int8_t input=open_path(from,O_RDONLY),out;
    uint16_t n;
    uint8_t ok=1;
    if(input<0) return 0;
    out=open_path(to,O_WRONLY|O_CREAT|O_TRUNC);
    if(out<0) { finish(input,0); return 0; }
    for(;;) {
        n=128; platform_error=read(input,console_scratch,&n);
        if(platform_error||n>128) { ok=0; break; }
        if(!n) break;
        if(!write_all(out,n)) { ok=0; break; }
    }
    ok=finish(input,ok); return finish(out,ok);
}
static uint8_t same_files(const char *a,const char *b) {
    int8_t first=open_path(a,O_RDONLY),second;
    uint16_t n[2]={0,0},at[2]={0,0},count,i;
    uint8_t j,ok=1;
    if(first<0) return 0;
    second=open_path(b,O_RDONLY);
    if(second<0) { finish(first,0); return 0; }
    for(;;) {
        for(j=0;j<2;++j) if(at[j]==n[j]) {
            n[j]=128; at[j]=0;
            platform_error=read(j?second:first,console_scratch+j*128,&n[j]);
            if(platform_error||n[j]>128) { ok=0; break; }
        }
        if(!ok||!n[0]||!n[1]) { if(n[0]!=n[1]) ok=0; break; }
        count=n[0]-at[0]; if(count>n[1]-at[1]) count=n[1]-at[1];
        for(i=0;i<count;++i) if(console_scratch[at[0]+i]!=console_scratch[128+at[1]+i]) ok=0;
        if(!ok) break;
        at[0]+=count; at[1]+=count;
    }
    if(!ok&&!platform_error) platform_error=ERR_FAILURE;
    ok=finish(first,ok); return finish(second,ok);
}
/* Every old_name is a closed, verified recovery member. Recheck before removing
 * the destination, then create it afresh: the pinned HostFS reports stale size
 * on truncate, and the OS can return a size byte as an error for in-file writes.
 * Do not reinterpret that error as success. Failed copies retain all recovery
 * members; this is deliberately no more atomic than the previous copy/truncate. */
static uint8_t publish_file(const char *new_name,const char *old_name,const char *destination) {
    uint8_t e=stat_path(destination);
    if(e!=ERR_NO_SUCH_ENTRY) {
        if(e) { platform_error=e; return 0; }
        /* ERR_ALREADY_EXIST is reserved by source saving for BAK approval. An
         * unexpected destination must fail without offering that permission. */
        if(!old_name) { platform_error=ERR_FAILURE; return 0; }
        if(!D_ISFILE(((zos_stat_t*)(console_scratch+128))->s_flags)) {
            platform_error=ERR_NOT_A_FILE; return 0;
        }
        in_transaction(old_name);
        if(!same_files(destination,member)) return 0;
        strcpy((char*)console_scratch,destination); e=rm((char*)console_scratch);
        if(e) { platform_error=e; return 0; }
    }
    in_transaction(new_name);
    return copy_file(member,destination);
}
uint8_t save_build(const Source *s,const Build *b) {
    uint8_t i,e,members=3; /* NEW.COM / NEW.ZMP, then optional OLD pair */
    zos_stat_t *info=(zos_stat_t*)(console_scratch+128);
    platform_error=0;
    if(!build_ready(s,b)) { platform_error=ERR_FAILURE; return 0; }
    if(!paths(s)) { platform_error=ERR_INVALID_PATH; return 0; }
    if(!strcmp(s->path,destinations[0])||!strcmp(s->path,destinations[1])) { platform_error=ERR_INVALID_PATH; return 0; }
    if(sources_find(&b->sources,destinations[0])>=0||sources_find(&b->sources,destinations[1])>=0) {
        platform_error=ERR_INVALID_PATH;return 0;
    }
    if(!parent_dirs(transaction,1)) return 0;
    if(!create_transaction()) return 0;
    /* Finish both new outputs and every old recovery copy before truncating
     * either destination. Failed publication retains the recovery files. */
    for(i=0;i<2;++i) {
        in_transaction(i?"NEW.ZMP":"NEW.COM");
        if(!store(i?&b->map_page:b->pages,i?b->map_length:b->length,member)||
           !verify(i?&b->map_page:b->pages,i?b->map_length:b->length,member)) return 0;
    }
    for(i=0;i<2;++i) {
        e=stat_path(destinations[i]);
        if(e==ERR_NO_SUCH_ENTRY) continue;
        if(e) { platform_error=e; return 0; }
        if(!D_ISFILE(info->s_flags)) { platform_error=ERR_NOT_A_FILE; return 0; }
        in_transaction(i?"OLD.ZMP":"OLD.COM");
        if(!copy_file(destinations[i],member)||!same_files(destinations[i],member)) return 0;
        members|=1u<<(i+2);
    }
    for(i=0;i<2;++i) {
        if(!publish_file(i?"NEW.ZMP":"NEW.COM",
                         members&(1u<<(i+2))?(i?"OLD.ZMP":"OLD.COM"):0,destinations[i])||
           !verify(i?&b->map_page:b->pages,i?b->map_length:b->length,destinations[i])) return 0;
    }
    cleanup_success(0,members);
    return 1;
}
uint8_t run_saved(const Source *s,const Build *b,uint8_t *status) {
    uint8_t e;
    platform_error=0;
    if(!build_ready(s,b)) { platform_error=ERR_FAILURE; return 0; }
    if(!paths(s)) { platform_error=ERR_INVALID_PATH; return 0; }
    if(!strcmp(s->path,destinations[0])||!strcmp(s->path,destinations[1])) { platform_error=ERR_INVALID_PATH; return 0; }
    if(!verify(b->pages,b->length,destinations[0])||
       !verify(&b->map_page,b->map_length,destinations[1])) return 0;
    strcpy((char*)console_scratch,destinations[0]);
    e=exec(EXEC_PRESERVE_PROGRAM,(char*)console_scratch,0,status);
    if(e) { platform_error=e; return 0; }
    return 1;
}
static uint8_t ordinary_file(const char *path,uint8_t *exists) {
    uint8_t e;
    e=stat_path(path);
    *exists=e!=ERR_NO_SUCH_ENTRY;
    if(e==ERR_NO_SUCH_ENTRY) return 1;
    if(e) { platform_error=e; return 0; }
    if(!D_ISFILE(((zos_stat_t*)(console_scratch+128))->s_flags)) { platform_error=ERR_NOT_A_FILE; return 0; }
    return 1;
}
uint8_t save_source(Source *s,uint8_t replace_backup) {
    Source snapshot;
    uint16_t extension;
    uint8_t exists,backup=0,ok=0,error;
    platform_error=0;
    if(!paths(s)) { platform_error=ERR_INVALID_PATH; return 0; }
    strcpy(destinations[1],destinations[0]);
    extension=(uint16_t)strlen(destinations[1]);
    extension-=3;
    strcpy(destinations[1]+extension,"BAK");
    if(!strcmp(s->path,destinations[1])) { platform_error=ERR_INVALID_PATH; return 0; }
    if(!ordinary_file(s->path,&exists)) return 0;
    if(exists) {
        if(!ordinary_file(destinations[1],&backup)) return 0;
        /* Asking for overwrite permission must not allocate staging copies or
         * fill a small disk with a transaction the user has not authorized. */
        if(backup&&!replace_backup) { platform_error=ERR_ALREADY_EXIST; return 0; }
    }
    if(!source_snapshot(s,&snapshot)) return 0;
    if(!parent_dirs(transaction,1)) goto cleanup;
    if(!create_transaction()) goto cleanup;
    in_transaction("NEW.PAS");
    if(!store(&snapshot.page,snapshot.length,member)||!verify(&snapshot.page,snapshot.length,member)) goto cleanup;
    if(exists) {
        in_transaction("OLD.PAS");
        if(!copy_file(s->path,member)||!same_files(s->path,member)) goto cleanup;
        if(backup) {
            in_transaction("PRIOR.BAK");
            if(!copy_file(destinations[1],member)||!same_files(destinations[1],member)) goto cleanup;
        }
        if(!publish_file("OLD.PAS",backup?"PRIOR.BAK":0,destinations[1])||
           !same_files(member,destinations[1])) goto cleanup;
    }
    if(!publish_file("NEW.PAS",exists?"OLD.PAS":0,s->path)||
       !verify(&snapshot.page,snapshot.length,s->path)) goto cleanup;
    ok=1;
cleanup:
    error=platform_error;
    if(!page_drop(snapshot.page)) { if(ok) error=platform_error; ok=0; }
    platform_error=error;
    if(ok) { s->dirty=0; cleanup_success(1,1u|(exists?2u:0u)|(backup?4u:0u)); }
    return ok;
}
uint8_t source_path(const char *name,char *out) SOURCE_BLOCK_CALL {
    console_scratch[0]=0;
    if(strlen(name)<2||name[1]!=':') {
        platform_error=curdir((char*)console_scratch);
        if(platform_error) return 0;
    }
    return sources_path(name,(char*)console_scratch,out);
}
uint8_t save_source_as(Source *s,const char *name,uint8_t replace_backup) {
    Source candidate;
    memcpy(&candidate,s,sizeof(candidate));
    platform_error=0;
    if(!source_path(name,candidate.path)) { if(!platform_error) platform_error=ERR_INVALID_PATH; return 0; }
    /* Publish from the same RAM page. Adopt the new identity only after the
     * ordinary verified save succeeds; failure leaves the entire Source intact. */
    if(!save_source(&candidate,replace_backup)) return 0;
    if(strcmp(s->path,candidate.path)) { strcpy(s->path,candidate.path); ++s->revision; }
    s->dirty=0; return 1;
}
static uint8_t read_source(Source *s,const char *name,Diagnostic *d,uint8_t required) {
    SourceImport in;
    uint8_t exists,error,ok=1;
    uint16_t n,i;
    int8_t fd;
    memset(d,0,sizeof(*d)); platform_error=0;
    if(!source_path(name,destinations[0])) { if(!platform_error) platform_error=ERR_INVALID_PATH; return 0; }
    if(!parent_dirs(destinations[0],0)) return 0;
    if(!ordinary_file(destinations[0],&exists)) return 0;
    if(!exists) {
        if(required) { platform_error=ERR_NO_SUCH_ENTRY; return 0; }
        if(!import_begin(&in,destinations[0],d)) return 0;
        return import_finish(&in,s,d);
    }
    fd=open_path(destinations[0],O_RDONLY); if(fd<0) return 0;
    if(!import_begin(&in,destinations[0],d)) { finish(fd,0); return 0; }
    while(!in.eof) {
        n=128; platform_error=read(fd,console_scratch,&n);
        if(platform_error||n>128) { ok=0; break; }
        if(!n) break;
        for(i=0;i<n;++i) if(!import_byte(&in,console_scratch[i],d)) { ok=0; break; }
        if(!ok) break;
    }
    ok=finish(fd,ok);
    if(!ok) { error=platform_error; page_drop(in.staged.page); platform_error=error; return 0; }
    return import_finish(&in,s,d);
}
uint8_t load_source(Source *s,const char *name,Diagnostic *d) {
    return read_source(s,name,d,0);
}
uint8_t read_block(Source *block,const char *name,Diagnostic *d) {
    uint8_t error;
    memset(d,0,sizeof(*d)); platform_error=0;
    if(!source_new(block,"")) { d->code=98; return 0; }
    if(read_source(block,name,d,1)) return 1;
    error=platform_error; source_close(block); platform_error=error; return 0;
}
uint8_t save_block(const Source *s,uint16_t a,uint16_t z,const char *name,uint8_t replace) {
    uint8_t exists;
    uint16_t base;
    platform_error=0; transaction_owned=0; transaction[0]=0;
    if(a>=z||z>s->length) { platform_error=ERR_INVALID_PARAMETER; return 0; }
    if(!source_path(name,destinations[0])) {
        if(!platform_error) platform_error=ERR_INVALID_PATH;
        return 0;
    }
    base=(uint16_t)strlen(destinations[0]);
    while(base>3&&destinations[0][base-1]!='/') --base;
    if(base>121) { platform_error=ERR_INVALID_PATH; return 0; }
    memcpy(transaction,destinations[0],base); transaction[base]=0;
    if(!ordinary_file(destinations[0],&exists)) return 0;
    if(exists&&!replace) { platform_error=ERR_ALREADY_EXIST; return 0; }
    if(!parent_dirs(transaction,1)||!create_transaction()) return 0;
    in_transaction("NEW.PAS");
    /* Block output is an exact byte slice, not a canonicalized source save.
     * The editor is suspended throughout this synchronous publication. */
    if(!store_range(&s->page,a,z-a,member)||!verify_range(&s->page,a,z-a,member)) return 0;
    if(exists) {
        in_transaction("OLD.PAS");
        if(!copy_file(destinations[0],member)||!same_files(destinations[0],member)) return 0;
    }
    if(!publish_file("NEW.PAS",exists?"OLD.PAS":0,destinations[0])||
       !verify_range(&s->page,a,z-a,destinations[0])) return 0;
    cleanup_success(1,exists?3:1);
    return 1;
}
