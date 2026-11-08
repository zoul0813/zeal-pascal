#include "units/io/stream.h"
#include <zos_sys.h>
#include <zos_vfs.h>

/* Copy only bounded, preplanned ranges into the unpublished staging image.
 * A complete file-page hash must match before linking can use that page.
 * Failure invalidates the whole build; no unverified executable is published. */
uint8_t cc_unit_emit_page(uint16_t module,uint8_t index,const CompilerFragmentLayout *layout) ROUTINES {
    uint16_t at=0,n,want,length=cc_unit_word(module+4),base;
    int32_t position;int8_t fd;uint8_t bytes[64],i,ok=0,primary;
    uint32_t hash=2166136261UL,expected;
    if(index>=4||!layout){cc_fail(UNIT_FORMAT);return 0;}
    base=(uint16_t)index*PAGE_SIZE;
    if(base>=length){cc_fail(UNIT_FORMAT);return 0;}
    length-=base;if(length>PAGE_SIZE)length=PAGE_SIZE;
    fd=cc_unit_path_open(module);if(fd<0)return 0;
    position=base;platform_error=seek(fd,&position,SEEK_SET);
    if(platform_error){cc_fail(COMPILE_PLATFORM_FAILURE);goto done;}
    if(position!=base){cc_fail(UNIT_STALE);goto done;}
    while(at<length) {
        if(!cc_poll())goto done;
        want=length-at;if(want>sizeof(bytes))want=sizeof(bytes);n=want;
        platform_error=read(fd,bytes,&n);
        if(platform_error){cc_fail(COMPILE_PLATFORM_FAILURE);goto done;}
        if(!n||n>want){cc_fail(UNIT_STALE);goto done;}
        for(i=0;i<n;++i)hash=(hash^bytes[i])*16777619UL;
        if(!cc_unit_copy_ranges(module,layout,base+at,bytes,(uint8_t)n))goto done;
        at+=n;
    }
    expected=cc_unit_word(UNIT_HASH_AT(module,2u*index))|
        ((uint32_t)cc_unit_word(UNIT_HASH_AT(module,2u*index+1u))<<16);
    if(hash!=expected){cc_fail(UNIT_STALE);goto done;}
    ok=1;
done:
    primary=platform_error;
    if(!cc_unit_files_close()){cc_fail(COMPILE_PLATFORM_FAILURE);ok=0;}
    if(primary&&!ok)platform_error=primary;return ok;
}
