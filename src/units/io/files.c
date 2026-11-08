#include "units/internal.h"
#include <zos_vfs.h>

/* A scan or page read owns one descriptor. Zeal VFS removes its descriptor
 * entry even when the filesystem reports a close error; never retry that
 * numeric handle after the close attempt, since it may have been reused. */
static int8_t active=-1;
uint8_t cc_unit_file_bind(int8_t fd) SOURCE_BLOCK_CALL {
    if(fd<0||active>=0)return 255;
    active=fd;return 0;
}
int8_t cc_unit_file(uint8_t slot) SOURCE_BLOCK_CALL {
    return slot==0?active:-1;
}
uint8_t cc_unit_files_close(void) SOURCE_BLOCK_CALL {
    int8_t fd=active;uint8_t error;
    active=-1;
    if(fd<0)return 1;
    error=close(fd);
    if(error){platform_error=error;return 0;}
    return 1;
}
