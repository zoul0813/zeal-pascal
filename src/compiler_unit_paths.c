#include "compiler_units.h"
#include "output_transport.h"
#include <zos_vfs.h>
#include <zos_errors.h>

/* Paths live in the compiler-owned catalog; no near pointer survives a load.
 * Only an active scan/window owns a descriptor, including failed-close retry. */
uint16_t cc_unit_path_store(const char *path) SOURCE_BLOCK_CALL {
    uint16_t first=0,record=0;uint8_t i=0,n=0,c;
    do {
        if(n==128){cc_fail(UNIT_FORMAT);return 0;}
        if(!i) {
            record=cc_unit_record(UR_PATH,0,0,0,0,0);
            if(!record)return 0;if(!first)first=record;
        }
        c=(uint8_t)path[n++];cc_unit_set(record+2+i,c);i=(i+1)%10;
    }while(c);
    return first;
}
int8_t cc_unit_path_open(uint16_t module) SOURCE_BLOCK_CALL {
    char path[128];uint16_t at=cc_unit_word(UNIT_FILE_AT(module)+2),end=cc_unit_word(UH_USED);
    uint8_t i=0,n=0,c;int8_t fd;
    do {
        if(n==sizeof(path)||at<UNIT_RECORD_BASE||(uint32_t)at+UNIT_RECORD_BYTES>end||cc_unit_word(at)!=UR_PATH) {
            cc_fail(UNIT_FORMAT);return -1;
        }
        c=cc_unit_byte(at+2+i);path[n++]=(char)c;
        if(++i==10){at+=UNIT_RECORD_BYTES;i=0;}
    }while(c);
    fd=output_open_path(path,O_RDONLY);
    if(fd<0){cc_fail(platform_error==ERR_NO_SUCH_ENTRY?UNIT_STALE:COMPILE_PLATFORM_FAILURE);return -1;}
    if(cc_unit_file_bind(fd)==255){output_finish(fd,0);cc_fail(99);return -1;}
    return fd;
}
