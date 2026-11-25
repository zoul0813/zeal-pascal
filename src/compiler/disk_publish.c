#include "compiler/disk.h"
#include "storage/transport.h"
#include <zos_vfs.h>
/* Streaming publication needs no permanently mapped code. The disk cache and
 * final image mapper remain resident while this file-service bank is lent. */
#ifdef __SDCC
#pragma codeseg BANK3
#pragma constseg BANK3
#endif
uint8_t cc_disk_publish(const Build *b,const char *path) SOURCE_BLOCK_CALL {
    uint8_t bytes[128];uint8_t ok=1;uint16_t at=0,n,i;int8_t fd;
    if(!cc_disk_prepare_publish(b))return 0;
    fd=output_open_path(path,O_WRONLY|O_CREAT|O_TRUNC);if(fd<0)return 0;
    while(at<b->length&&ok) {
        n=b->length-at;if(n>128)n=128;
        for(i=0;i<n;++i)bytes[i]=cc_disk_image(b,at+i);
        ok=!platform_error&&output_write_buffer(fd,bytes,n);at+=n;
    }
    return output_finish(fd,ok);
}
