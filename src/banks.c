#include "library.h"
#include "compiler_job.h"
#include "worker.h"
#include "output_transport.h"
#include <zos_errors.h>

/* All bank transitions have resident callers and stack/data arguments. The
 * ordinary SDCC ABI is sufficient: no banked return addresses or hidden bank
 * state cross a syscall, far-page access, or Pascal RAM entry. */
extern uint8_t code_select(uint8_t bank) NATIVE;
extern int app_main(void);
uint8_t compiler_compile_project(const Source *root,Build *b,Diagnostic *d,uint8_t interruptible,const CompileOptions *options);
void compiler_build_close(Build *b);
uint8_t compiler_build_check(const Source *s,const Build *b);
uint8_t compiler_build_ready(const Source *s,const Build *b);
uint8_t compiler_build_locate(const Source *s,const Build *b,uint16_t pc,uint16_t *position);
uint8_t compiler_build_location(const Source *s,const Build *b,uint16_t pc,uint8_t *file,uint16_t *position);
uint8_t compiler_image_get(const Build *b,uint16_t offset);
uint8_t files_save_build(const Source *s,const Build *b,uint8_t interruptible);
uint8_t files_save_source(Source *s,uint8_t replace_backup,uint8_t interruptible);
uint8_t files_save_source_as(Source *s,const char *name,uint8_t replace_backup,uint8_t interruptible);
uint8_t files_load_source(Source *s,const char *name,Diagnostic *d);
uint8_t files_reload_source(Source *s,const char *name,Diagnostic *d);
uint8_t files_read_block(Source *block,const char *name,Diagnostic *d);
uint8_t files_save_block(const Source *s,uint16_t first,uint16_t last,const char *name,uint8_t replace,uint8_t interruptible);
uint8_t files_run_saved(const Source *s,Build *b,uint8_t *status);
static uint8_t capture_path(char *out,const char *name);
uint8_t files_worker_control(uint8_t mode);

uint8_t worker_control(uint8_t mode) {
    uint8_t old=code_select(CODE_SHARED),ok=files_worker_control(mode);
    code_restore(old); return ok;
}

void code_ready(void) { __asm nop __endasm; }
int main(int argc,char **argv) {
    uint8_t old;
    int result;
    char path[128];
    const char *requested=DEFAULT_LIBRARY;
    if(argc) requested=argv[0];
    if(!platform_supported()) {
        output("Zeal Pascal needs the supported MMU machine at 4000.\n"); return 1;
    }
    /* Own the command tail before the first syscall/mapping transition. */
    if(!capture_path(path,requested)) { output("Invalid Pascal library path\n"); return 1; }
    output("Loading Zeal Pascal...\n");
    if(!code_load(path)) {
        output("Missing or invalid Pascal library: "); output(path);
        output(" (native error "); output_number(boot_error); output(")\n"); return 1;
    }
#ifdef ZP_TEST_COMPILER
    /* Compiler component fixtures start at the compiler boundary. The IDE
     * startup test instead observes the deferred, thirteen-bank session. */
    if(!code_phase_restore())return 1;
#endif
    code_ready();
    old=code_select(CODE_IDE); result=app_main(); code_restore(old);
    return result;
}
uint8_t compile(const Source *s,Build *b,Diagnostic *d) {
    return compile_project(s,b,d,0,0);
}
uint8_t compile_project(const Source *root,Build *b,Diagnostic *d,uint8_t interruptible,const CompileOptions *options) {
    uint8_t old,ok;
    if(!code_phase_restore()) {memset(d,0,sizeof(*d));d->code=COMPILE_PLATFORM_FAILURE;return 0;}
    old=code_select(CODE_PHASE_FIRST);ok=compiler_compile_project(root,b,d,interruptible,options);
    code_restore(old);return ok;
}
uint8_t compiler_compile_request(const Source *root,Build *build,CompileFailure *failure,
    uint8_t mode,const CompileOptions *options);
uint8_t compile_request(const Source *root,Build *build,CompileFailure *failure,
    uint8_t mode,const CompileOptions *options) {
    uint8_t old,ok;
    if(!compiler_resources_ready()||!code_phase_restore()) {
        memset(failure,0,sizeof(*failure));failure->diagnostic.code=COMPILE_PLATFORM_FAILURE;return 0;
    }
    old=code_select(CODE_PHASE_FIRST);ok=compiler_compile_request(root,build,failure,mode,options);
    code_restore(old);return ok;
}
void build_close(Build *b) {
    uint8_t old=code_select(CODE_BUILD); compiler_build_close(b); code_restore(old);
}
uint8_t build_check(const Source *s,const Build *b) {
    uint8_t old=code_select(CODE_BUILD),ok=compiler_build_check(s,b);
    code_restore(old); return ok;
}
uint8_t build_ready(const Source *s,const Build *b) {
    uint8_t old=code_select(CODE_BUILD),ok=compiler_build_ready(s,b);
    code_restore(old); return ok;
}
uint8_t build_locate(const Source *s,const Build *b,uint16_t pc,uint16_t *position) {
    uint8_t old=code_select(CODE_BUILD),ok=compiler_build_locate(s,b,pc,position);
    code_restore(old); return ok;
}
uint8_t build_location(const Source *s,const Build *b,uint16_t pc,uint8_t *file,uint16_t *position) {
    uint8_t old=code_select(CODE_BUILD),ok=compiler_build_location(s,b,pc,file,position);
    code_restore(old);return ok;
}
uint8_t image_get(const Build *b,uint16_t offset) {
    uint8_t old=code_select(CODE_BUILD),value=compiler_image_get(b,offset);
    code_restore(old); return value;
}
uint8_t save_build(const Source *s,const Build *b,uint8_t interruptible) {
    uint8_t old=code_select(CODE_SHARED),ok=files_save_build(s,b,interruptible);
    code_restore(old); return ok;
}
uint8_t save_source(Source *s,uint8_t replace_backup,uint8_t interruptible) {
    uint8_t old=code_select(CODE_SHARED),ok=files_save_source(s,replace_backup,interruptible);
    code_restore(old); return ok;
}
static uint8_t capture_path(char *out,const char *name) {
    if(strlen(name)>127) { platform_error=ERR_INVALID_PATH; return 0; }
    strcpy(out,name); return 1;
}
uint8_t save_source_as(Source *s,const char *name,uint8_t replace_backup,uint8_t interruptible) {
    char path[128]; uint8_t old,ok;
    if(!capture_path(path,name)) { output_job_begin(0); return 0; }
    old=code_select(CODE_SHARED); ok=files_save_source_as(s,path,replace_backup,interruptible);
    code_restore(old); return ok;
}
uint8_t load_source(Source *s,const char *name,Diagnostic *d) {
    /* A caller may pass a literal in its own bank. Capture it before mapping
     * the file bank. Other bank APIs accept only resident objects or scalars. */
    char path[128]; uint8_t old,ok;
    if(!capture_path(path,name)) { memset(d,0,sizeof(*d)); return 0; }
    old=code_select(CODE_SHARED); ok=files_load_source(s,path,d);
    code_restore(old); return ok;
}
uint8_t reload_source(Source *s,const char *name,Diagnostic *d) {
    char path[128]; uint8_t old,ok;
    if(!capture_path(path,name)) { memset(d,0,sizeof(*d)); return 0; }
    old=code_select(CODE_SHARED); ok=files_reload_source(s,path,d);
    code_restore(old); return ok;
}
uint8_t run_saved(const Source *s,Build *b,uint8_t *status) {
    uint8_t old=code_select(CODE_SHARED),ok=files_run_saved(s,b,status);
    code_restore(old); return ok;
}
uint8_t read_block(Source *block,const char *name,Diagnostic *d) {
    char path[128]; uint8_t old,ok;
    if(!capture_path(path,name)) { memset(d,0,sizeof(*d)); return 0; }
    old=code_select(CODE_SHARED); ok=files_read_block(block,path,d);
    code_restore(old); return ok;
}
uint8_t save_block(const Source *s,uint16_t first,uint16_t last,const char *name,uint8_t replace,uint8_t interruptible) {
    char path[128]; uint8_t old,ok;
    if(!capture_path(path,name)) { output_job_begin(0); return 0; }
    old=code_select(CODE_SHARED); ok=files_save_block(s,first,last,path,replace,interruptible);
    code_restore(old); return ok;
}
