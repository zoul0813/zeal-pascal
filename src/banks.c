#include "zp.h"
#include <zos_errors.h>

/* All bank transitions have resident callers and stack/data arguments. The
 * ordinary SDCC ABI is sufficient: no banked return addresses or hidden bank
 * state cross a syscall, far-page access, or Pascal RAM entry. */
extern uint8_t code_select(uint8_t bank) NATIVE;
extern void code_restore(uint8_t page) NATIVE;
extern uint8_t code_owned, boot_error;
extern int app_main(void);
uint8_t compiler_compile(const Source *s,Build *b,Diagnostic *d);
void compiler_build_close(Build *b);
uint8_t compiler_build_check(const Source *s,const Build *b);
uint8_t compiler_build_ready(const Source *s,const Build *b);
uint8_t compiler_build_locate(const Source *s,const Build *b,uint16_t pc,uint16_t *position);
uint8_t compiler_image_get(const Build *b,uint16_t offset);
uint8_t files_save_build(const Source *s,const Build *b);
uint8_t files_save_source(Source *s,uint8_t replace_backup);
uint8_t files_save_source_as(Source *s,const char *name,uint8_t replace_backup);
uint8_t files_load_source(Source *s,const char *name,Diagnostic *d);
uint8_t files_run_saved(const Source *s,const Build *b,uint8_t *status);
const char *files_output_path(void);
const char *files_output_recovery(void);

int main(void) {
    uint8_t old;
    int result;
    if(code_owned!=3) {
        if(boot_error==ERR_NOT_SUPPORTED) output("Zeal Pascal needs the supported MMU machine at 4000.\n");
        else {
            output("Pascal cannot allocate its code banks. Native error ");
            output_number(boot_error); output("\n");
        }
        return 1;
    }
    old=code_select(0); result=app_main(); code_restore(old);
    return result;
}
uint8_t compile(const Source *s,Build *b,Diagnostic *d) {
    uint8_t old=code_select(1),ok=compiler_compile(s,b,d);
    code_restore(old); return ok;
}
void build_close(Build *b) {
    uint8_t old=code_select(1); compiler_build_close(b); code_restore(old);
}
uint8_t build_check(const Source *s,const Build *b) {
    uint8_t old=code_select(1),ok=compiler_build_check(s,b);
    code_restore(old); return ok;
}
uint8_t build_ready(const Source *s,const Build *b) {
    uint8_t old=code_select(1),ok=compiler_build_ready(s,b);
    code_restore(old); return ok;
}
uint8_t build_locate(const Source *s,const Build *b,uint16_t pc,uint16_t *position) {
    uint8_t old=code_select(1),ok=compiler_build_locate(s,b,pc,position);
    code_restore(old); return ok;
}
uint8_t image_get(const Build *b,uint16_t offset) {
    uint8_t old=code_select(1),value=compiler_image_get(b,offset);
    code_restore(old); return value;
}
uint8_t save_build(const Source *s,const Build *b) {
    uint8_t old=code_select(2),ok=files_save_build(s,b);
    code_restore(old); return ok;
}
uint8_t save_source(Source *s,uint8_t replace_backup) {
    uint8_t old=code_select(2),ok=files_save_source(s,replace_backup);
    code_restore(old); return ok;
}
static uint8_t capture_path(char *out,const char *name) {
    if(strlen(name)>127) { platform_error=ERR_INVALID_PATH; return 0; }
    strcpy(out,name); return 1;
}
uint8_t save_source_as(Source *s,const char *name,uint8_t replace_backup) {
    char path[128]; uint8_t old,ok;
    if(!capture_path(path,name)) return 0;
    old=code_select(2); ok=files_save_source_as(s,path,replace_backup);
    code_restore(old); return ok;
}
uint8_t load_source(Source *s,const char *name,Diagnostic *d) {
    /* A caller may pass a literal in its own bank. Capture it before mapping
     * the file bank. Other bank APIs accept only resident objects or scalars. */
    char path[128]; uint8_t old,ok;
    if(!capture_path(path,name)) { memset(d,0,sizeof(*d)); return 0; }
    old=code_select(2); ok=files_load_source(s,path,d);
    code_restore(old); return ok;
}
uint8_t run_saved(const Source *s,const Build *b,uint8_t *status) {
    uint8_t old=code_select(2),ok=files_run_saved(s,b,status);
    code_restore(old); return ok;
}
const char *output_path(void) {
    uint8_t old=code_select(2); const char *p=files_output_path();
    code_restore(old); return p;
}
const char *output_recovery(void) {
    uint8_t old=code_select(2); const char *p=files_output_recovery();
    code_restore(old); return p;
}
