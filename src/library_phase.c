#include "library.h"
#include <zos_sys.h>
#include <zos_vfs.h>

extern uint8_t console_scratch[256];
int8_t code_phase_fd=-1;
uint8_t code_phase_pending[CODE_BANK_LIMIT-CODE_PHASE_FIRST];
static uint8_t identities[(CODE_BANK_LIMIT-CODE_PHASE_FIRST)*32u],valid;
static uint32_t offsets[CODE_BANK_LIMIT-CODE_PHASE_FIRST];

/* Called only after the entire original container has matched code_digest.
 * Per-phase identities are derived from that verified immutable snapshot,
 * never accepted from a second, mutable file header. Keep the original open
 * handle: changing a pathname cannot redirect future reloads. */
void code_phase_bind(int8_t fd) SOURCE_BLOCK_CALL {
    Sha256 hash;uint16_t p;uint8_t bank,index;
    uint32_t at=16u+2u*code_required;
    valid=0;
    memset(code_phase_pending,255,sizeof(code_phase_pending));
    for(bank=0;bank<code_required;++bank) {
        if(bank>=CODE_PHASE_FIRST) {
            index=bank-CODE_PHASE_FIRST;offsets[index]=at;
            sha_init(&hash);
            for(p=0;p<code_sizes[bank];++p)sha_byte(&hash,far_get(code_pages[bank],p));
            sha_end(&hash,identities+(uint16_t)index*32u);
            valid|=(uint8_t)(1u<<index);
        }
        at+=code_sizes[bank];
    }
    code_phase_fd=fd;
}
static uint8_t failure(uint8_t error) {platform_error=boot_error=error;return 0;}
uint8_t code_phase_park(void) SOURCE_BLOCK_CALL {
    uint8_t bank,error,index,page;
    for(bank=CODE_PHASE_FIRST;bank<code_owned;++bank) {
        index=bank-CODE_PHASE_FIRST;page=code_phase_pending[index];
        if(page!=255) {
            error=pfree(page);if(error)return failure(error);
            code_phase_pending[index]=255;
        }
        page=code_pages[bank];
        if(page!=255) {
            error=pfree(page);if(error)return failure(error);
            code_pages[bank]=255;valid&=(uint8_t)~(1u<<index);
        }
    }
    return 1;
}
uint8_t code_phase_restore(void) SOURCE_BLOCK_CALL {
    Sha256 hash;uint8_t bank,index,page,digest[32],error;
    uint16_t at,n,j;int32_t seek_to;
    for(bank=CODE_PHASE_FIRST;bank<code_owned;++bank) {
        index=bank-CODE_PHASE_FIRST;
        if((valid&(1u<<index))&&code_pages[bank]!=255)continue;
        valid&=(uint8_t)~(1u<<index);
        if(code_phase_fd<0)return failure(ERR_INVALID_FILEDEV);
        if(code_pages[bank]!=255)return failure(ERR_INVALID_PARAMETER);
        page=code_phase_pending[index];
        if(page==255) {
            error=palloc(&page);if(error)return failure(error);
            code_phase_pending[index]=page;
        }
        if(page<32||page>=64)return failure(ERR_INVALID_PARAMETER);
        seek_to=(int32_t)offsets[index];error=seek(code_phase_fd,&seek_to,SEEK_SET);
        if(!error&&(uint32_t)seek_to!=offsets[index])error=ERR_ENTRY_CORRUPTED;
        sha_init(&hash);
        for(at=0;!error&&at<code_sizes[bank];at+=n) {
            n=code_sizes[bank]-at;if(n>128)n=128;
            if(!code_read_exact(code_phase_fd,n,&hash)) {error=boot_error;break;}
            for(j=0;j<n;++j)far_set(page,at+j,console_scratch[j]);
        }
        if(!error) {
            sha_end(&hash,digest);
            if(memcmp(digest,identities+(uint16_t)index*32u,32))error=ERR_ENTRY_CORRUPTED;
        }
        if(error) {
            /* A failed free retains an unready page for retry/terminal release.
             * Only the expected digest can set its executable-valid bit. */
            if(!pfree(page))code_phase_pending[index]=255;
            return failure(error);
        }
        code_pages[bank]=page;code_phase_pending[index]=255;
        valid|=(uint8_t)(1u<<index);
    }
    platform_error=boot_error=0;
    return 1;
}
void code_phase_return(void) SOURCE_BLOCK_CALL {
    uint8_t primary=platform_error;
    code_phase_restore();platform_error=primary;
}
uint8_t execute_native(const char *path,uint8_t *status) SOURCE_BLOCK_CALL {
    char owned[128];uint8_t error;
    if(strlen(path)>127)return ERR_INVALID_PATH;
    strcpy(owned,path); /* Shared syscall scratch cannot retain the pathname. */
    if(!code_phase_park()) {error=platform_error;code_phase_return();return error;}
    error=exec(EXEC_PRESERVE_PROGRAM,owned,0,status);
    code_phase_return();return error;
}
