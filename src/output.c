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
#include "compiler_disk.h"
#include "path.h"
#include "sources.h"
#include "output_transport.h"
#include <zos_vfs.h>
#include <zos_errors.h>
#include <zos_sys.h>

extern uint8_t console_scratch[256];
static char destinations[2][128],member[128];
static uint8_t build_prepared,prepared_chain;
const char *output_path(void) SOURCE_BLOCK_CALL { return destinations[0]; }

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
    return !build_prepared&&path_program(s->path,destinations[0],chain);
}
static uint8_t ordinary_file(const char *path,uint8_t *exists);
/* The pinned HostFS cannot reliably truncate in place. Remove only a checked
 * regular destination before creating its replacement. Count writes and check
 * close errors; publication has no paired-artifact transaction or readback. */
static uint8_t remove_output(const char *path) {
    uint8_t exists,e;
    if(!ordinary_file(path,&exists))return 0;
    if(!exists)return 1;
    strcpy((char*)console_scratch,path);e=rm((char*)console_scratch);
    if(e)platform_error=e;
    return !e;
}
static uint8_t store(const uint8_t *pages,uint16_t total,const char *path) {
    return output_store_range(pages,0,total,path);
}
static uint8_t do_save_build(const Source *s,const Build *b) {
    uint8_t chain,exists;
    char expected[128];
    platform_error=0;
    if(!build_ready(s,b)) {platform_error=ERR_FAILURE;return 0;}
    chain=build_is_chain(b);
    if(build_prepared) {
        if(chain!=prepared_chain||!path_program(s->path,expected,chain)||
           strcmp(expected,destinations[0])) {
            platform_error=ERR_INVALID_PATH;return 0;
        }
    } else if(!paths(s,chain)) {platform_error=ERR_INVALID_PATH;return 0;}
    if(!strcmp(s->path,destinations[0])||sources_find(&b->sources,destinations[0])>=0) {
        platform_error=ERR_INVALID_PATH;return 0;
    }
    if(!ordinary_file(destinations[0],&exists))return 0;
    if(!parent_dirs(destinations[0],1))return 0;
    build_prepared=0;
    if(!output_poll()||!remove_output(destinations[0]))return 0;
    if(cc_disk_active(b)) {if(!cc_disk_publish(b,destinations[0]))return 0;}
    else if(!store(b->pages,b->length,destinations[0]))return 0;
    return 1;
}
uint8_t run_saved(const Source *s,Build *b,uint8_t *status) {
    uint8_t e;
    platform_error=0;
    if(!build_ready(s,b)) {platform_error=ERR_FAILURE;return 0;}
    if(build_is_chain(b)) {platform_error=ERR_NOT_SUPPORTED;return 0;}
    if(!paths(s,0)) {platform_error=ERR_INVALID_PATH;return 0;}
    if(!strcmp(s->path,destinations[0])) {platform_error=ERR_INVALID_PATH;return 0;}
    build_close(b);
    if(b->map_page!=255||b->pages[0]!=255||b->pages[1]!=255||b->pages[2]!=255||b->sources.page!=255) {
        if(!platform_error)platform_error=ERR_FAILURE;return 0;
    }
    strcpy((char*)console_scratch,destinations[0]);
    e=execute_native((char*)console_scratch,status);
    if(e) {platform_error=e;return 0;}
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
    uint16_t extension;
    uint8_t exists,backup,error;
    platform_error=0;
    if(!paths(s,0)) {platform_error=ERR_INVALID_PATH;return 0;}
    strcpy(destinations[1],destinations[0]);extension=(uint16_t)strlen(destinations[1]);
    extension-=3;strcpy(destinations[1]+extension,"BAK");
    if(!strcmp(s->path,destinations[1])) {platform_error=ERR_INVALID_PATH;return 0;}
    if(!ordinary_file(s->path,&exists)||!ordinary_file(destinations[1],&backup))return 0;
    if(exists&&backup&&!replace_backup) {platform_error=ERR_ALREADY_EXIST;return 0;}
    if(!parent_dirs(s->path,1)||!output_poll())return 0;
    /* TP3 Save retains exactly the previous source as BAK. A synchronous save
     * reads the suspended Work page directly; no snapshot or recovery catalog. */
    if(exists&&(!remove_output(destinations[1])||!output_copy_file(s->path,destinations[1])))return 0;
    if(!remove_output(s->path)||!output_store_source(s,s->path)) {
        error=platform_error;output_job_end();
        /* Zeal has no rename: retain BAK and attempt to restore the old source
         * after a failed replacement. A failed restore never removes BAK. */
        if(remove_output(s->path)&&exists)output_copy_file(destinations[1],s->path);
        platform_error=error;return 0;
    }
    s->dirty=0;return 1;
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
     * ordinary save succeeds; failure leaves the entire Source intact. */
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
    if(!ok) { error=platform_error; source_discard(s,&in.staged); platform_error=error; return 0; }
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
    uint8_t exists;
    platform_error=0;
    if(build_prepared) {platform_error=ERR_FAILURE;return 0;}
    if(a>=z||z>s->length) {platform_error=ERR_INVALID_PARAMETER;return 0;}
    if(!source_path(name,destinations[0])) {
        if(!platform_error)platform_error=ERR_INVALID_PATH;return 0;
    }
    if(!ordinary_file(destinations[0],&exists))return 0;
    if(exists&&!replace) {platform_error=ERR_ALREADY_EXIST;return 0;}
    if(!parent_dirs(destinations[0],1)||!output_poll()||!remove_output(destinations[0]))return 0;
    return output_store_source_slice(s,a,z-a,destinations[0]);
}

uint8_t save_build(const Source *s,const Build *b,uint8_t interruptible) {
    uint8_t ok;
    output_job_begin(interruptible); ok=do_save_build(s,b); output_job_end(); return ok;
}
/* Pin output names across read-only include loads. No output file, temporary
 * directory or RAM image cache is acquired during preparation. */
uint8_t prepare_build(const Source *s,uint8_t chain,uint8_t interruptible) SOURCE_BLOCK_CALL {
    uint8_t ok=0;
    output_job_begin(interruptible);
    if(chain>1||!paths(s,chain)||!strcmp(s->path,destinations[0])) {
        platform_error=ERR_INVALID_PATH;goto done;
    }
    if(!parent_dirs(destinations[0],1))goto done;
    prepared_chain=chain;build_prepared=1;ok=1;
done:
    output_job_end();return ok;
}
uint8_t cancel_prepared_build(void) SOURCE_BLOCK_CALL {
    build_prepared=0;return 1;
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
