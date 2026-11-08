#include "units/io/stream.h"
#include "output_transport.h"
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zos_errors.h>

/* Read and authenticate the whole stream, but allocate pages only for metadata.
 * This makes the import peak independent of the unreferenced code payload. */
uint16_t cc_units_load_object(const char *name,const char *path,const char *fallback) ROUTINES {
    uint16_t module=0,at=0,n,i,seal,record,prefix=0,blob=0,declared=0,keep=ZPU_HEADER_BYTES;
    uint8_t buffer[64],page,ok=1,index;int8_t fd;
    uint32_t version,hashes[4],hash=2166136261UL;
    fd=path[0]?output_open_path(path,O_RDONLY):-ERR_NO_SUCH_ENTRY;
    if(!path[0])platform_error=ERR_NO_SUCH_ENTRY;
    if(fd<0&&platform_error==ERR_NO_SUCH_ENTRY&&fallback&&strcmp(path,fallback))
        {path=fallback;fd=output_open_path(path,O_RDONLY);}
    if(fd<0){cc_fail(platform_error==ERR_NO_SUCH_ENTRY?UNIT_MISSING:COMPILE_PLATFORM_FAILURE);return 0;}
    if(cc_unit_file_bind(fd)==255){output_finish(fd,0);cc_fail(99);return 0;}
    module=cc_unit_owned_page();if(!module)return 0;
    record=cc_unit_record(UR_FILE,0,0,0,0,0);
    if(!record||!cc_unit_record(UR_HASH,0,0,0,0,0))return 0;
    cc_unit_put(record+2,cc_unit_path_store(path));
    if(cc.error->code)return 0;
    page=(uint8_t)cc_unit_word(module+2);
    for(i=0;i<4;++i)hashes[i]=2166136261UL;
    for(;;) {
        if(!cc_poll()){ok=0;break;}n=sizeof(buffer);platform_error=read(fd,buffer,&n);
        if(platform_error||n>sizeof(buffer)||(uint32_t)at+n>ZPU_ARTIFACT_LIMIT){ok=0;break;}
        if(!n)break;
        for(i=0;i<n;++i) {
            index=(uint8_t)(at/PAGE_SIZE);
            hashes[index]=(hashes[index]^buffer[i])*16777619UL;
            if(at<28||at>=32)hash=(hash^buffer[i])*16777619UL;
            if(at<keep) {
                if(!(at%PAGE_SIZE)&&!buffer_reserve(page,at+1)){cc_fail(99);ok=0;break;}
                buffer_set(page,at,buffer[i]);
            }
            ++at;
            if(at==ZPU_HEADER_BYTES) {
                keep=cc_unit_prefix_header(page,&declared);
                if(!keep){ok=0;break;}blob=keep-CC_FRAGMENT_HEADER;
            }
            if(blob&&at==blob+CC_FRAGMENT_HEADER) {
                prefix=keep=cc_unit_prefix_finish(page,blob,declared);
                if(!keep){ok=0;break;}
            }
            if(declared&&at>declared){ok=0;break;}
        }
        if(!ok)break;
    }
    if(!cc_unit_files_close()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    if(!ok||!prefix||at!=declared){cc_fail(UNIT_FORMAT);return 0;}
    if(!buffer_reserve(page,prefix+2u*CC_FRAGMENT_OBJECTS)){cc_fail(99);return 0;}
    cc_unit_put(module+4,at);cc_unit_put(record+4,prefix);
    seal=module+UNIT_RECORD_BYTES;version=page_versions[page];
    cc_unit_put(seal+6,(uint16_t)version);cc_unit_put(seal+8,(uint16_t)(version>>16));
    cc_unit_put(module+8,cc_unit_word(UH_MODULES)+1);
    if(!cc_unit_validate_object(module,name,hash)){cc_fail(UNIT_FORMAT);return 0;}
    cc_unit_hashes_store(module,hashes);
    return module;
}
