/* Exercise the production final-link phase and each native failure boundary. */
#include "compiler/internal.h"
#include "platform/banks/loader.h"
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zos_errors.h>
#include <assert.h>
#include <stdio.h>
uint8_t code_pages[CODE_BANK_LIMIT],code_owned,boot_error,platform_error;
uint16_t code_sizes[CODE_BANK_LIMIT];
const uint8_t code_required=CODE_BANK_LIMIT,code_version[4]={0};
static uint8_t owned[64];
static unsigned events,fail_at,checks;
#define check(c) do {++checks;assert(c);}while(0)
static uint8_t fault(void){return ++events==fail_at;}
zos_err_t palloc(uint8_t *out){uint8_t p;if(fault())return ERR_FAILURE;for(p=32;p<64;++p)if(!owned[p]){owned[p]=1;*out=p;return 0;}return ERR_NO_MORE_MEMORY;}
zos_err_t pfree(uint8_t p){if(fault())return ERR_FAILURE;check(p>=32&&p<64&&owned[p]);owned[p]=0;return 0;}
zos_err_t seek(zos_dev_t fd,int32_t *offset,zos_whence_t whence){check(fd==1&&*offset>=0&&whence==SEEK_SET);return fault()?ERR_FAILURE:0;}
uint8_t code_read_bank(int8_t fd,uint8_t page,uint16_t size){check(fd==1&&owned[page]&&size==17);if(fault()){boot_error=ERR_FAILURE;return 0;}return 1;}
zos_err_t exec(zos_exec_mode_t mode,const char *name,const char *argv[],uint8_t *status){(void)mode;(void)name;(void)argv;(void)status;assert(0);return ERR_FAILURE;}
static void setup(void){
 uint8_t bank,page=32;memset(owned,0,sizeof(owned));memset(code_pages,255,sizeof(code_pages));
 code_owned=CODE_BANK_LIMIT;platform_error=boot_error=0;events=fail_at=0;
 for(bank=0;bank<CODE_BANK_LIMIT;++bank){code_sizes[bank]=17;if(bank==CODE_RUNTIME||bank==CODE_UNIT_IO||bank==CODE_UNIT_META||bank==CODE_LINKER)continue;code_pages[bank]=page;owned[page++]=1;}
 code_phase_bind(1);
}
static void inventory(void){
 uint8_t bank,other,p;unsigned total=0,seen=0;
 for(p=32;p<64;++p)total+=owned[p];
 for(bank=0;bank<CODE_BANK_LIMIT;++bank){p=code_pages[bank];if(p!=255){check(p>=32&&p<64&&owned[p]);++seen;for(other=0;other<bank;++other)check(code_pages[other]!=p);}
 p=code_phase_pending[bank];if(p!=255){check(p>=32&&p<64&&owned[p]&&code_pages[bank]==255);++seen;for(other=0;other<CODE_BANK_LIMIT;++other)check(code_pages[other]!=p);for(other=0;other<bank;++other)check(code_phase_pending[other]!=p);}}
 check(seen==total);
}
int main(void){
 unsigned failures,total;uint8_t coordinator,source;
 setup();coordinator=code_pages[CODE_COMPILE];source=code_pages[CODE_FILE_RUNTIME];
 check(code_phase_unit_finish_begin());total=events;
 check(code_pages[CODE_STATEMENTS]==255&&code_pages[CODE_EXPRESSIONS]==255&&code_pages[CODE_FILE_COMPILER]==255&&code_pages[CODE_TRANSFER]==255&&code_pages[CODE_LINKER]!=255);
 check(code_pages[CODE_COMPILE]==coordinator&&code_pages[CODE_FILE_RUNTIME]==source);
 check(code_phase_unit_io()&&code_phase_unit_meta());inventory();
 check(code_phase_unit_meta_release()&&code_phase_unit_io_release()&&code_phase_unit_finish_end());inventory();
 for(failures=1;failures<=total;++failures){
  setup();coordinator=code_pages[CODE_COMPILE];source=code_pages[CODE_FILE_RUNTIME];fail_at=failures;
  check(!code_phase_unit_finish_begin()&&platform_error);inventory();
  check(code_pages[CODE_COMPILE]==coordinator&&code_pages[CODE_FILE_RUNTIME]==source);
  fail_at=0;check(code_phase_unit_finish_end()&&code_phase_restore());inventory();
  check(code_pages[CODE_STATEMENTS]!=255&&code_pages[CODE_EXPRESSIONS]!=255&&code_pages[CODE_FILE_COMPILER]!=255&&code_pages[CODE_TRANSFER]!=255&&code_pages[CODE_LINKER]==255);
 }
 /* Fail every restoration boundary, including the eagerly loaded statement
  * bank; the next guarded compiler entry retries the complete warm set. */
 setup();check(code_phase_unit_finish_begin());events=0;
 check(code_phase_unit_finish_end());total=events;
 for(failures=1;failures<=total;++failures){
  setup();check(code_phase_unit_finish_begin());events=0;fail_at=failures;
  check(!code_phase_unit_finish_end()&&platform_error);inventory();
  fail_at=0;check(code_phase_restore());inventory();
  check(code_pages[CODE_STATEMENTS]!=255&&code_pages[CODE_EXPRESSIONS]!=255&&
        code_pages[CODE_FILE_COMPILER]!=255&&code_pages[CODE_TRANSFER]!=255);
 }
 printf("PASS: %u unit-phase assertions across %u restoration failure boundaries\n",checks,total);
 return 0;
}
