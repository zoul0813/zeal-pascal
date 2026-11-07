#include "library.h"
#include <zos_sys.h>
#include <zos_vfs.h>

int8_t code_phase_fd=-1;
uint8_t code_phase_pending[CODE_BANK_LIMIT]={
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
};
static uint32_t valid,resume_mask;

/* Keep the original open handle and declared bank offsets for later reads. */
void code_phase_bind(int8_t fd) SOURCE_BLOCK_CALL {
    uint8_t bank,index;
    valid=resume_mask=0;
    memset(code_phase_pending,255,sizeof(code_phase_pending));
    for(bank=0;bank<code_required;++bank) {
        if(CODE_DEFERRED(bank)) {
            index=bank;
            if(code_pages[bank]!=255)valid|=(1UL<<index);
        }
    }
    code_phase_fd=fd;
}
static uint8_t failure(uint8_t error) {platform_error=boot_error=error;return 0;}
static uint8_t drop_bank(uint8_t bank) {
    uint8_t page=code_phase_pending[bank],error;
    if(page!=255) {
        error=pfree(page);if(error)return failure(error);
        code_phase_pending[bank]=255;
    }
    page=code_pages[bank];
    if(page!=255) {
        error=pfree(page);if(error)return failure(error);
        code_pages[bank]=255;valid&=~(1UL<<bank);
    }
    return 1;
}
uint8_t code_phase_park(void) SOURCE_BLOCK_CALL {
    uint8_t bank;
    resume_mask=valid;
    for(bank=0;bank<code_owned;++bank)
        if(CODE_DEFERRED(bank)&&!drop_bank(bank))return 0;
    return 1;
}
/* The immutable size directory already owns these extents; avoid retaining
 * a second array of offsets in scarce page3 data. At most five seeks use it. */
static uint32_t bank_offset(uint8_t bank) {
    uint32_t at=16u+2u*code_required;uint8_t i;
    for(i=0;i<bank;++i)at+=code_sizes[i];
    return at;
}
static uint8_t restore(uint32_t wanted) {
    uint8_t bank,index,page,error;
    int32_t seek_to;
    for(bank=0;bank<code_owned;++bank)if(CODE_DEFERRED(bank)&&(wanted&(1UL<<bank))) {
        index=bank;
        if((valid&(1UL<<index))&&code_pages[bank]!=255)continue;
        valid&=~(1UL<<index);
        if(code_phase_fd<0)return failure(ERR_INVALID_FILEDEV);
        if(code_pages[bank]!=255)return failure(ERR_INVALID_PARAMETER);
        page=code_phase_pending[index];
        if(page==255) {
            error=palloc(&page);if(error)return failure(error);
            code_phase_pending[index]=page;
        }
        if(page<32||page>=64)return failure(ERR_INVALID_PARAMETER);
        seek_to=(int32_t)bank_offset(bank);error=seek(code_phase_fd,&seek_to,SEEK_SET);
        if(!error&&(uint32_t)seek_to!=bank_offset(bank))error=ERR_ENTRY_CORRUPTED;
        if(!error&&!code_read_bank(code_phase_fd,page,code_sizes[bank]))error=boot_error;
        if(error) {
            /* A failed free retains an unready page for retry/terminal release.
             * A complete reload is required before the page becomes usable. */
            if(!pfree(page))code_phase_pending[index]=255;
            return failure(error);
        }
        code_pages[bank]=page;code_phase_pending[index]=255;
        valid|=(1UL<<index);
    }
    platform_error=boot_error=0;
    return 1;
}
uint8_t code_phase_restore(void) SOURCE_BLOCK_CALL {
    uint32_t wanted=0xffffffffUL;
#ifdef ZP_OPTIONAL_REAL
    wanted&=~(1UL<<CODE_RUNTIME);
#endif
#ifdef ZP_UNITS_EXPERIMENT
    wanted&=~((1UL<<CODE_UNIT_IO)|(1UL<<CODE_UNIT_META)|(1UL<<CODE_LINKER));
#endif
    return restore(wanted);
}
#ifdef ZP_OPTIONAL_REAL
uint8_t code_phase_real(void) SOURCE_BLOCK_CALL {return restore(1UL<<CODE_RUNTIME);}
uint8_t code_phase_real_release(void) SOURCE_BLOCK_CALL {
    if(!drop_bank(CODE_RUNTIME))return 0;
    resume_mask&=~(1UL<<CODE_RUNTIME);return 1;
}
#endif
#ifdef ZP_UNITS_EXPERIMENT
/* Unit graph checks, serialization and final placement run outside the file
 * generator. Lend its idle physical slot, then restore before parsing/helpers. */
uint8_t code_phase_unit_link_begin(void) SOURCE_BLOCK_CALL {
    if(!drop_bank(CODE_FILE_COMPILER))return 0;
    if(restore(1UL<<CODE_LINKER))return 1;
    {uint8_t primary=platform_error;drop_bank(CODE_LINKER);restore(1UL<<CODE_FILE_COMPILER);platform_error=primary;return 0;}
}
/* Final linking and serialization run after every parser frame has returned. Unsigned value
 * decoding lives in the retained support bank, so the expression generator
 * can lend its slot as well as the file generator until output is sealed. */
uint8_t code_phase_unit_finish_begin(void) SOURCE_BLOCK_CALL {
    uint8_t primary;
    if(!drop_bank(CODE_FILE_COMPILER))return 0;
    if(drop_bank(CODE_EXPRESSIONS)&&restore(1UL<<CODE_LINKER))return 1;
    primary=platform_error;drop_bank(CODE_LINKER);
    restore((1UL<<CODE_FILE_COMPILER)|(1UL<<CODE_EXPRESSIONS));platform_error=primary;return 0;
}
uint8_t code_phase_unit_finish_end(void) SOURCE_BLOCK_CALL {
    if(!drop_bank(CODE_LINKER))return 0;
    resume_mask&=~(1UL<<CODE_LINKER);
    return restore((1UL<<CODE_FILE_COMPILER)|(1UL<<CODE_EXPRESSIONS));
}
uint8_t code_phase_unit_link_end(void) SOURCE_BLOCK_CALL {
    if(!drop_bank(CODE_LINKER))return 0;
    resume_mask&=~(1UL<<CODE_LINKER);return restore(1UL<<CODE_FILE_COMPILER);
}
uint8_t code_phase_unit_meta(void) SOURCE_BLOCK_CALL {return restore(1UL<<CODE_UNIT_META);}
uint8_t code_phase_unit_meta_release(void) SOURCE_BLOCK_CALL {
    if(!drop_bank(CODE_UNIT_META))return 0;
    resume_mask&=~(1UL<<CODE_UNIT_META);return 1;
}
uint8_t code_phase_unit_io(void) SOURCE_BLOCK_CALL {return restore(1UL<<CODE_UNIT_IO);}
uint8_t code_phase_unit_io_release(void) SOURCE_BLOCK_CALL {
    if(!drop_bank(CODE_UNIT_IO))return 0;
    resume_mask&=~(1UL<<CODE_UNIT_IO);return 1;
}
#endif
void code_phase_return(void) SOURCE_BLOCK_CALL {
    uint8_t primary=platform_error;
    restore(resume_mask);platform_error=primary;
}
uint8_t execute_native(const char *path,uint8_t *status) SOURCE_BLOCK_CALL {
    char owned[128];uint8_t error;
    if(strlen(path)>127)return ERR_INVALID_PATH;
    strcpy(owned,path); /* Shared syscall scratch cannot retain the pathname. */
    if(!code_phase_park()) {error=platform_error;code_phase_return();return error;}
    error=exec(EXEC_PRESERVE_PROGRAM,owned,0,status);
    code_phase_return();return error;
}
