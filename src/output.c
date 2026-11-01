#ifdef __SDCC
#define save_build files_save_build
#define save_source files_save_source
#define save_source_as files_save_source_as
#define load_source files_load_source
#define reload_source files_reload_source
#define read_block files_read_block
#define save_block files_save_block
#define run_saved files_run_saved
#endif
#include "zp.h"
#include "path.h"
#include "sources.h"
#include "saved_map.h"
#include "output_transport.h"
#include <zos_vfs.h>
#include <zos_errors.h>
#include <zos_sys.h>

extern uint8_t console_scratch[256];
static char destinations[2][128], transaction[128], member[128];
static uint16_t serial;
static uint8_t transaction_owned,build_prepared,prepared_chain;
/* Directory prefix + ZPxxxx + '/' + PRIOR.BAK must fit with its NUL. */
enum { TRANSACTION_PARENT_LIMIT=sizeof(transaction)-1-6-1-9 };
static char no_recovery[1]; /* Returned pointers must survive a code-bank switch. */
const char *output_path(void) SOURCE_BLOCK_CALL { return destinations[0]; }
const char *output_recovery(void) SOURCE_BLOCK_CALL {
    if(transaction_owned) return transaction;
    return no_recovery;
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
        if(!output_poll()) return 0;
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
static uint8_t paths(const Source *s,uint8_t chain) {
    uint16_t base;
    if(build_prepared)return 0;
    transaction[0]=0;
    transaction_owned=0;
    if(strlen(s->path)>123||!saved_paths_kind(s->path,destinations,chain))return 0;
    base=(uint16_t)strlen(destinations[0]);
    while(base&&destinations[0][base-1]!='/')--base;
    if(base>TRANSACTION_PARENT_LIMIT)return 0;
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
    for(i=0;i<(source==1?3:4);++i) {
        if(!(members&(1u<<i))) continue;
        if(source==1) name=i==0?"NEW.PAS":i==1?"OLD.PAS":"PRIOR.BAK";
        else name=i==0?(source==2?"NEW.CHN":"NEW.COM"):i==1?"NEW.ZMP":i==2?(source==2?"OLD.CHN":"OLD.COM"):"OLD.ZMP";
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
    if(base>TRANSACTION_PARENT_LIMIT) {platform_error=ERR_INVALID_PATH;return 0;}
    for(attempt=0;attempt<256;++attempt) {
        if(!output_poll()) return 0;
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
static uint8_t store(const uint8_t *pages,uint16_t total,const char *path) {
    return output_store_range(pages,0,total,path);
}
static uint8_t verify(const uint8_t *pages,uint16_t total,const char *path) {
    return output_verify_range(pages,0,total,path);
}
/* Every old_name is a closed, verified recovery member. Recheck before removing
 * the destination, then create it afresh: the pinned HostFS reports stale size
 * on truncate, and the OS can return a size byte as an error for in-file writes.
 * Do not reinterpret that error as success. Failed copies retain all recovery
 * members; this is deliberately no more atomic than the previous copy/truncate. */
static uint8_t publish_file(const char *new_name,const char *old_name,const char *destination,uint8_t *touched) {
    uint8_t e;
    if(!output_poll()) return 0;
    e=stat_path(destination);
    if(e!=ERR_NO_SUCH_ENTRY) {
        if(e) { platform_error=e; return 0; }
        /* ERR_ALREADY_EXIST is reserved by source saving for BAK approval. An
         * unexpected destination must fail without offering that permission. */
        if(!old_name) { platform_error=ERR_FAILURE; return 0; }
        if(!D_ISFILE(((zos_stat_t*)(console_scratch+128))->s_flags)) {
            platform_error=ERR_NOT_A_FILE; return 0;
        }
        in_transaction(old_name);
        if(!output_same_files(destination,member)) return 0;
        if(!output_poll()) return 0;
        *touched=1;
        strcpy((char*)console_scratch,destination); e=rm((char*)console_scratch);
        if(e) { platform_error=e; return 0; }
    }
    if(!output_poll()) return 0;
    *touched=1;
    in_transaction(new_name);
    return output_copy_file(member,destination);
}
/* Restoration never removes a recovery member. A failed restore leaves the
 * complete NEW/OLD copies available at the reported transaction path. Try both
 * destinations even if one restoration fails, preserving the first error. */
static void restore_file(const char *destination,const char *old_name,uint8_t touched) {
    uint8_t prior=platform_error,e;
    if(!touched) return;
    strcpy((char*)console_scratch,destination); e=rm((char*)console_scratch);
    if(e&&e!=ERR_NO_SUCH_ENTRY) { platform_error=prior?prior:e; return; }
    platform_error=0;
    if(old_name) {
        in_transaction(old_name);
        if(!output_copy_file(member,destination)||!output_same_files(member,destination)) {
            if(!platform_error) platform_error=ERR_FAILURE;
        }
    }
    if(prior) platform_error=prior;
}
static uint8_t do_save_build(const Source *s,const Build *b) {
    uint8_t chain,i,e,members=3,touched[2]={0,0}; /* NEW.COM / NEW.ZMP, then optional OLD pair */
    char expected[2][128];
    zos_stat_t *info=(zos_stat_t*)(console_scratch+128);
    platform_error=0;
    if(!build_ready(s,b)) { platform_error=ERR_FAILURE; return 0; }
    chain=build_is_chain(b);
    if(build_prepared) {
        if(chain!=prepared_chain||!saved_paths_kind(s->path,expected,chain)||
           strcmp(expected[0],destinations[0])||strcmp(expected[1],destinations[1])) {
            platform_error=ERR_INVALID_PATH;return 0;
        }
    } else if(!paths(s,chain)) { platform_error=ERR_INVALID_PATH; return 0; }
    if(!strcmp(s->path,destinations[0])||!strcmp(s->path,destinations[1])) { platform_error=ERR_INVALID_PATH; return 0; }
    if(sources_find(&b->sources,destinations[0])>=0||sources_find(&b->sources,destinations[1])>=0) {
        platform_error=ERR_INVALID_PATH;return 0;
    }
    if(!build_prepared) {
        if(!parent_dirs(transaction,1)) return 0;
        if(!create_transaction()) return 0;
    }
    build_prepared=0;
    /* Finish both new outputs and every old recovery copy before truncating
     * either destination. Failed publication retains the recovery files. */
    for(i=0;i<2;++i) {
        in_transaction(i?"NEW.ZMP":chain?"NEW.CHN":"NEW.COM");
        if(!store(i?&b->map_page:b->pages,i?b->map_length:b->length,member)||
           !verify(i?&b->map_page:b->pages,i?b->map_length:b->length,member)) return 0;
    }
    for(i=0;i<2;++i) {
        e=stat_path(destinations[i]);
        if(e==ERR_NO_SUCH_ENTRY) continue;
        if(e) { platform_error=e; return 0; }
        if(!D_ISFILE(info->s_flags)) { platform_error=ERR_NOT_A_FILE; return 0; }
        in_transaction(i?"OLD.ZMP":chain?"OLD.CHN":"OLD.COM");
        if(!output_copy_file(destinations[i],member)||!output_same_files(destinations[i],member)) return 0;
        members|=1u<<(i+2);
    }
    for(i=0;i<2;++i) {
        if(!publish_file(i?"NEW.ZMP":chain?"NEW.CHN":"NEW.COM",
                         members&(1u<<(i+2))?(i?"OLD.ZMP":chain?"OLD.CHN":"OLD.COM"):0,destinations[i],&touched[i])||
           !verify(i?&b->map_page:b->pages,i?b->map_length:b->length,destinations[i])) goto failed;
    }
    if(!output_poll()) goto failed;
    output_job_end();
    cleanup_success(chain?2:0,members);
    return 1;
failed:
    output_job_end();
    for(i=0;i<2;++i)
        restore_file(destinations[i],members&(1u<<(i+2))?(i?"OLD.ZMP":chain?"OLD.CHN":"OLD.COM"):0,touched[i]);
    return 0;
}
uint8_t run_saved(const Source *s,Build *b,uint8_t *status) {
    uint8_t e;
    platform_error=0;
    if(!build_ready(s,b)) { platform_error=ERR_FAILURE; return 0; }
    if(build_is_chain(b)) {platform_error=ERR_NOT_SUPPORTED;return 0;}
    if(!paths(s,0)) { platform_error=ERR_INVALID_PATH; return 0; }
    if(!strcmp(s->path,destinations[0])||!strcmp(s->path,destinations[1])) { platform_error=ERR_INVALID_PATH; return 0; }
    if(!verify(b->pages,b->length,destinations[0])||
       !verify(&b->map_page,b->map_length,destinations[1])) return 0;
    /* The saved executable is the program. Release its transient compiler
     * image/map before entering it; no RAM cache is reconstructed afterward. */
    build_close(b);
    if(b->map_page!=255||b->pages[0]!=255||b->pages[1]!=255||b->pages[2]!=255||b->sources.page!=255) {
        if(!platform_error)platform_error=ERR_FAILURE;return 0;
    }
    strcpy((char*)console_scratch,destinations[0]);
    e=execute_native((char*)console_scratch,status);
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
static uint8_t do_save_source(Source *s,uint8_t replace_backup) {
    Source snapshot;
    uint16_t extension;
    uint8_t exists,backup=0,ok=0,error,touched[2]={0,0};
    platform_error=0;
    if(!paths(s,0)) { platform_error=ERR_INVALID_PATH; return 0; }
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
        if(!output_copy_file(s->path,member)||!output_same_files(s->path,member)) goto cleanup;
        if(backup) {
            in_transaction("PRIOR.BAK");
            if(!output_copy_file(destinations[1],member)||!output_same_files(destinations[1],member)) goto cleanup;
        }
        if(!publish_file("OLD.PAS",backup?"PRIOR.BAK":0,destinations[1],&touched[1])||
           !output_same_files(member,destinations[1])) goto cleanup;
    }
    if(!publish_file("NEW.PAS",exists?"OLD.PAS":0,s->path,&touched[0])||
       !verify(&snapshot.page,snapshot.length,s->path)) goto cleanup;
    if(!output_poll()) goto cleanup;
    ok=1;
cleanup:
    output_job_end();
    if(!ok) {
        restore_file(s->path,exists?"OLD.PAS":0,touched[0]);
        restore_file(destinations[1],backup?"PRIOR.BAK":0,touched[1]);
    }
    error=platform_error;
    if(!page_drop(snapshot.page)) { if(ok) error=platform_error; ok=0; }
    platform_error=error;
    if(ok) { s->dirty=0; cleanup_success(1,1u|(exists?2u:0u)|(backup?4u:0u)); }
    return ok;
}
static uint8_t current_directory(const char *name) {
    platform_error=0;console_scratch[0]=0;
    if(strlen(name)<2||name[1]!=':') {
        platform_error=curdir((char*)console_scratch);
        if(platform_error) return 0;
    }
    return 1;
}
uint8_t native_path(const char *name,char *out) SOURCE_BLOCK_CALL {
    if(!current_directory(name))return 0;
    if(path_resolve(name,(char*)console_scratch,out))return 1;
    platform_error=ERR_INVALID_PATH;return 0;
}
uint8_t source_path(const char *name,char *out) SOURCE_BLOCK_CALL {
    if(!current_directory(name))return 0;
    return sources_path(name,(char*)console_scratch,out);
}
static uint8_t do_save_source_as(Source *s,const char *name,uint8_t replace_backup) {
    Source candidate;
    memcpy(&candidate,s,sizeof(candidate));
    platform_error=0;
    if(!source_path(name,candidate.path)) { if(!platform_error) platform_error=ERR_INVALID_PATH; return 0; }
    /* Publish from the same RAM page. Adopt the new identity only after the
     * ordinary verified save succeeds; failure leaves the entire Source intact. */
    if(!do_save_source(&candidate,replace_backup)) return 0;
    if(strcmp(s->path,candidate.path)) { strcpy(s->path,candidate.path); ++s->revision; }
    s->dirty=0; return 1;
}
static uint8_t read_source(Source *s,const char *name,Diagnostic *d,uint8_t required) {
    SourceImport in;
    char path[128];
    uint8_t exists,error,ok=1;
    uint16_t n,i;
    int8_t fd;
    memset(d,0,sizeof(*d)); platform_error=0;
    if(!source_path(name,path)) { if(!platform_error) platform_error=ERR_INVALID_PATH; return 0; }
    if(!parent_dirs(path,0)) return 0;
    if(!ordinary_file(path,&exists)) return 0;
    if(!exists) {
        if(required) { platform_error=ERR_NO_SUCH_ENTRY; return 0; }
        if(!import_begin(&in,path,d)) return 0;
        return import_finish(&in,s,d);
    }
    fd=output_open_path(path,O_RDONLY); if(fd<0) return 0;
    if(!import_begin(&in,path,d)) { output_finish(fd,0); return 0; }
    while(!in.eof) {
        n=128; platform_error=read(fd,console_scratch,&n);
        if(platform_error||n>128) { ok=0; break; }
        if(!n) break;
        for(i=0;i<n;++i) if(!import_byte(&in,console_scratch[i],d)) { ok=0; break; }
        if(!ok) break;
    }
    ok=output_finish(fd,ok);
    if(!ok) { error=platform_error; page_drop(in.staged.page); platform_error=error; return 0; }
    return import_finish(&in,s,d);
}
uint8_t load_source(Source *s,const char *name,Diagnostic *d) {
    return read_source(s,name,d,0);
}
uint8_t reload_source(Source *s,const char *name,Diagnostic *d) {
    return read_source(s,name,d,1);
}
uint8_t read_block(Source *block,const char *name,Diagnostic *d) {
    uint8_t error;
    memset(d,0,sizeof(*d)); platform_error=0;
    /* A new block has no old buffer to preserve. The streaming importer
     * acquires its one page; loading an include must not need a dummy page. */
    memset(block,0,sizeof(*block));block->page=255;
    if(read_source(block,name,d,1)) return 1;
    error=platform_error;if(block->page!=255)source_close(block);platform_error=error;return 0;
}
static uint8_t do_save_block(const Source *s,uint16_t a,uint16_t z,const char *name,uint8_t replace) {
    uint8_t exists,touched=0;
    uint16_t base;
    platform_error=0;
    if(build_prepared) {platform_error=ERR_FAILURE;return 0;}
    transaction_owned=0; transaction[0]=0;
    if(a>=z||z>s->length) { platform_error=ERR_INVALID_PARAMETER; return 0; }
    if(!source_path(name,destinations[0])) {
        if(!platform_error) platform_error=ERR_INVALID_PATH;
        return 0;
    }
    base=(uint16_t)strlen(destinations[0]);
    while(base>3&&destinations[0][base-1]!='/') --base;
    if(base>TRANSACTION_PARENT_LIMIT) { platform_error=ERR_INVALID_PATH; return 0; }
    memcpy(transaction,destinations[0],base); transaction[base]=0;
    if(!ordinary_file(destinations[0],&exists)) return 0;
    if(exists&&!replace) { platform_error=ERR_ALREADY_EXIST; return 0; }
    if(!parent_dirs(transaction,1)||!create_transaction()) return 0;
    in_transaction("NEW.PAS");
    /* Block output is an exact byte slice, not a canonicalized source save.
     * The editor is suspended throughout this synchronous publication. */
    if(!output_store_range(&s->page,a,z-a,member)||!output_verify_range(&s->page,a,z-a,member)) return 0;
    if(exists) {
        in_transaction("OLD.PAS");
        if(!output_copy_file(destinations[0],member)||!output_same_files(destinations[0],member)) return 0;
    }
    if(!publish_file("NEW.PAS",exists?"OLD.PAS":0,destinations[0],&touched)||
       !output_verify_range(&s->page,a,z-a,destinations[0])) goto failed;
    if(!output_poll()) goto failed;
    output_job_end();
    cleanup_success(1,exists?3:1);
    return 1;
failed:
    output_job_end();
    restore_file(destinations[0],exists?"OLD.PAS":0,touched);
    return 0;
}

uint8_t save_build(const Source *s,const Build *b,uint8_t interruptible) {
    uint8_t ok;
    output_job_begin(interruptible); ok=do_save_build(s,b); output_job_end(); return ok;
}
/* The compiler receives this already-owned directory. Includes are read-only
 * and use their own path buffer, so they cannot retarget later publication. */
uint8_t prepare_build(const Source *s,uint8_t chain,uint8_t interruptible) SOURCE_BLOCK_CALL {
    uint8_t ok=0;
    output_job_begin(interruptible);
    if(build_prepared) {platform_error=ERR_FAILURE;goto done;}
    if(chain>1||!paths(s,chain)||!strcmp(s->path,destinations[0])||!strcmp(s->path,destinations[1])) {
        platform_error=ERR_INVALID_PATH;goto done;
    }
    if(!parent_dirs(transaction,1)||!create_transaction())goto done;
    prepared_chain=chain;build_prepared=1;ok=1;
done:
    output_job_end();return ok;
}
uint8_t cancel_prepared_build(void) SOURCE_BLOCK_CALL {
    uint8_t e,previous=platform_error;
    if(!build_prepared)return 1;
    /* Until publication starts this directory is empty. A foreign/new member
     * makes rm fail; never enumerate or recursively remove an unknown file. */
    strcpy((char*)console_scratch,transaction);e=rm((char*)console_scratch);
    /* A leftover directory is recovery, not an active publication. Keeping
     * the reservation would prevent every subsequent save of dirty source. */
    build_prepared=0;
    if(e) {platform_error=e;return 0;}
    transaction_owned=0;platform_error=previous;return 1;
}
uint8_t save_source(Source *s,uint8_t replace_backup,uint8_t interruptible) {
    uint8_t ok;
    output_job_begin(interruptible); ok=do_save_source(s,replace_backup); output_job_end(); return ok;
}
uint8_t save_source_as(Source *s,const char *name,uint8_t replace_backup,uint8_t interruptible) {
    uint8_t ok;
    output_job_begin(interruptible); ok=do_save_source_as(s,name,replace_backup); output_job_end(); return ok;
}
uint8_t save_block(const Source *s,uint16_t a,uint16_t z,const char *name,uint8_t replace,uint8_t interruptible) {
    uint8_t ok;
    output_job_begin(interruptible); ok=do_save_block(s,a,z,name,replace); output_job_end(); return ok;
}
