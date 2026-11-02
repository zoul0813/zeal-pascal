/* Deterministic SDK disk for saved compiler jobs. Source includes keep their
 * existing independent read_block oracle; only compiler scratch uses this. */
#include "output_transport.h"
#include <zos_vfs.h>
static struct {
    char name[128];uint8_t bytes[65536],exists,opened;
    uint32_t at,length;
} compiler_spool;
enum { CD_STAT,CD_OPEN,CD_SEEK,CD_READ,CD_WRITE,CD_REMOVE,CD_OPERATIONS };
static unsigned compiler_sdk_calls[CD_OPERATIONS],compiler_sdk_fail_at;
static uint8_t compiler_sdk_fail_kind;
/* Reproduce the pinned kernel's interior-overwrite epilogue independently of
 * the compiler: bytes are committed, then the file-size high byte is reported
 * as an error. Modes2/3 deliberately invalidate the committed sector. */
enum { CD_QUIRK_NONE,CD_QUIRK_COMMITTED,CD_QUIRK_CORRUPT,CD_QUIRK_TRUNCATE };
static uint8_t compiler_overwrite_mode,compiler_overwrite_pending,compiler_overwrite_reported;
static unsigned compiler_overwrite_reports,compiler_overwrite_readbacks;
static uint32_t compiler_overwrite_at;
static uint8_t compiler_overwrite_expected[128];
static uint8_t compiler_sdk_fault(uint8_t operation) {
    ++compiler_sdk_calls[operation];
    if(compiler_sdk_fail_at&&operation==compiler_sdk_fail_kind&&
       compiler_sdk_calls[operation]==compiler_sdk_fail_at) {
        compiler_sdk_fail_at=0;return 1;
    }
    return 0;
}
static void compiler_sdk_reset(void) {
    memset(compiler_sdk_calls,0,sizeof(compiler_sdk_calls));compiler_sdk_fail_at=0;
    compiler_overwrite_mode=compiler_overwrite_pending=compiler_overwrite_reported=0;
    compiler_overwrite_reports=compiler_overwrite_readbacks=0;
}
zos_dev_t open(const char *path,uint8_t flags) {
    if(compiler_sdk_fault(CD_OPEN))return -ERR_FAILURE;
    if(compiler_spool.opened)return -ERR_ALREADY_OPENED;
    if(!(flags&O_CREAT)&&!compiler_spool.exists)return -ERR_NO_SUCH_ENTRY;
    if(!compiler_spool.exists){strcpy(compiler_spool.name,path);compiler_spool.length=0;}
    if(strcmp(path,compiler_spool.name))return -ERR_NO_SUCH_ENTRY;
    compiler_spool.opened=compiler_spool.exists=1;compiler_spool.at=0;return 2;
}
zos_err_t close(zos_dev_t fd) {
    if(fd!=2||!compiler_spool.opened)return ERR_INVALID_FILEDEV;
    compiler_spool.opened=0;return 0;
}
zos_err_t stat(const char *path,zos_stat_t *info) {
    if(compiler_sdk_fault(CD_STAT))return ERR_FAILURE;
    if(!compiler_spool.exists||strcmp(path,compiler_spool.name))return ERR_NO_SUCH_ENTRY;
    memset(info,0,sizeof(*info));info->s_flags=1;info->s_size=compiler_spool.length;return 0;
}
zos_err_t rm(const char *path) {
    if(compiler_sdk_fault(CD_REMOVE))return ERR_FAILURE;
    if(!compiler_spool.exists||strcmp(path,compiler_spool.name))return ERR_NO_SUCH_ENTRY;
    if(compiler_spool.opened)return ERR_ALREADY_OPENED;
    compiler_spool.exists=0;return 0;
}
zos_err_t seek(zos_dev_t fd,int32_t *at,zos_whence_t whence) {
    int64_t next=*at;
    if(compiler_sdk_fault(CD_SEEK))return ERR_FAILURE;
    if(fd!=2||!compiler_spool.opened)return ERR_INVALID_FILEDEV;
    if(whence==SEEK_CUR)next+=compiler_spool.at;
    if(whence==SEEK_END)next+=compiler_spool.length;
    if(next<0)next=0;if(next>compiler_spool.length)next=compiler_spool.length;
    compiler_spool.at=(uint32_t)next;*at=(int32_t)next;return 0;
}
uint8_t output_write_buffer(int8_t fd,const uint8_t *bytes,uint16_t n) {
    uint32_t at=compiler_spool.at,length=compiler_spool.length;
    if(compiler_sdk_fault(CD_WRITE)){platform_error=ERR_FAILURE;return 0;}
    if(fd!=2||!compiler_spool.opened||compiler_spool.at+n>sizeof(compiler_spool.bytes)){platform_error=ERR_FAILURE;return 0;}
    memcpy(compiler_spool.bytes+compiler_spool.at,bytes,n);compiler_spool.at+=n;
    if(compiler_spool.length<compiler_spool.at)compiler_spool.length=compiler_spool.at;
    if(compiler_overwrite_mode&&n==128&&length>=PREFIX_BYTES&&at<length) {
        compiler_overwrite_at=at;compiler_overwrite_pending=1;
        memcpy(compiler_overwrite_expected,bytes,128);++compiler_overwrite_reports;
        if(compiler_overwrite_mode==CD_QUIRK_CORRUPT)compiler_spool.bytes[at+64]^=1;
        if(compiler_overwrite_mode==CD_QUIRK_TRUNCATE)compiler_spool.length=at+64;
        compiler_overwrite_reported=(uint8_t)(length>>8);
        platform_error=compiler_overwrite_reported;return 0;
    }
    return 1;
}
uint8_t output_read_buffer(int8_t fd,uint8_t *bytes,uint16_t n) {
    if(compiler_sdk_fault(CD_READ)){platform_error=ERR_FAILURE;return 0;}
    if(fd!=2||!compiler_spool.opened||compiler_spool.at+n>compiler_spool.length){platform_error=ERR_FAILURE;return 0;}
    memcpy(bytes,compiler_spool.bytes+compiler_spool.at,n);
    if(compiler_overwrite_pending&&compiler_spool.at==compiler_overwrite_at&&n==128) {
        if(!memcmp(bytes,compiler_overwrite_expected,128))++compiler_overwrite_readbacks;
        compiler_overwrite_pending=0;
    }
    compiler_spool.at+=n;return 1;
}
