#include "compiler.h"
#include "compiler_disk.h"
#include "path.h"
#include "output_transport.h"
#include <zos_vfs.h>
#include <zos_errors.h>

/* Saved compilation keeps runtime/globals and relocatable code in one disk
 * workspace. A128-byte sector is the only code cache; no image page exists.
 * Final publication streams the selected prefix/globals/gap/code layout. */
uint8_t compiler_disk_requested;uint8_t compiler_disk_placed;
static const Build *owner;
static int8_t disk=-1;
static char scratch[128];
static uint8_t cache[128];uint8_t zero[128];uint8_t cached;uint8_t dirty;uint8_t placed;uint8_t sealed;uint8_t chain_kind;uint8_t owned;
static uint16_t cache_at,extent,code_at,code_bytes,globals,code_base,code_used;

static uint8_t failure(void) {
    if(!platform_error)platform_error=ERR_FAILURE;
    if(cc.polling&&owner&&cc.image==owner&&cc.error&&!cc.error->code)cc.error->code=COMPILE_PLATFORM_FAILURE;
    return 0;
}
static uint8_t position(uint16_t at) {
    int32_t target=at;
    platform_error=seek(disk,&target,SEEK_SET);
    return !platform_error&&target==at?1:failure();
}
/* The pinned kernel's overwrite epilogue can return a file-size byte as an
 * error after committing the write. Scratch files are read/write and sector
 * aligned: accept that undocumented status only after exact sector readback.
 * Reuse the zero buffer, restoring it before any later gap extension. */
static uint8_t write_cache(void) {
    uint8_t reported,ok;
    if(output_write_buffer(disk,cache,128))return 1;
    reported=platform_error;
    if(extent<PREFIX_BYTES||cache_at>=extent||reported<0x40u||reported>=0xc0u)return 0;
    ok=position(cache_at)&&output_read_buffer(disk,zero,128)&&!memcmp(cache,zero,128);
    memset(zero,0,sizeof(zero));
    platform_error=ok?0:reported;
    return ok;
}
static uint8_t flush(void) {
    uint16_t n;
    if(!dirty)return 1;
    if(!cc_poll())return 0;
    /* Supported HostFS seek clamps at EOF: explicitly extend sparse gaps. */
    if(extent<cache_at) {
        if(!position(extent))return 0;
        while(extent<cache_at) {
            n=cache_at-extent;if(n>128)n=128;
            if(!output_write_buffer(disk,zero,n))return failure();extent+=n;
        }
    }
    if(!position(cache_at)||!write_cache())return failure();
    if(extent<cache_at+128)extent=cache_at+128;
    dirty=0;return 1;
}
static uint8_t sector(uint16_t at) {
    uint16_t base=at&0xff80u;
    if(cached&&base==cache_at)return 1;
    if(!flush()||!cc_poll())return 0;
    cache_at=base;cached=1;memset(cache,0,sizeof(cache));
    if(base<extent&&(!position(base)||!output_read_buffer(disk,cache,128)))return failure();
    return 1;
}
uint8_t cc_disk_active(const Build *b) {return owner==b&&disk>=0;}
uint8_t cc_disk_ready(const Build *b) {return cc_disk_active(b)&&sealed;}
uint8_t cc_disk_chain(const Build *b) {return cc_disk_active(b)&&chain_kind;}
void cc_disk_discard(const Build *b) {
    uint8_t primary=platform_error;uint8_t e=0;
    if(owner!=b)return;
    if(disk>=0){e=close(disk);disk=-1;}
    if(owned) {uint8_t removed=rm(scratch);if(!removed)owned=0;if(!e)e=removed;}
    owner=0;cached=dirty=placed=sealed=compiler_disk_placed=0;
    platform_error=primary?primary:e;
}
uint8_t cc_disk_begin(Build *b,const Source *root,uint8_t chain) {
    char path[128];zos_stat_t info;uint16_t n;uint8_t e;
    if(owner)return failure();
    /* A failed cleanup retains only our known scratch pathname, never a new
     * user's colliding file. Retry that cleanup before acquiring another. */
    if(owned) {e=rm(scratch);if(e){platform_error=e;return failure();}owned=0;}
    if(!path_program(root->path,path,chain)) {platform_error=ERR_INVALID_PATH;return failure();}
    strcpy(scratch,path);n=(uint16_t)strlen(scratch);n-=3;strcpy(scratch+n,"$$$");
    e=stat(scratch,&info);
    if(e!=ERR_NO_SUCH_ENTRY){platform_error=e?e:ERR_ALREADY_EXIST;return failure();}
    disk=open(scratch,O_RDWR|O_CREAT);
    if(disk<0){platform_error=(uint8_t)-disk;disk=-1;return failure();}
    owner=b;owned=1;extent=0;cached=dirty=placed=sealed=compiler_disk_placed=0;chain_kind=chain;
    code_base=PREFIX_BYTES;code_used=0;
    memset(zero,0,sizeof(zero));return 1;
}
static void raw_put(uint16_t at,uint8_t value) {
    if(disk<0||!sector(at))return;
    cache[at&127u]=value;dirty=1;
}
static uint8_t raw_get(uint16_t at) {return disk>=0&&sector(at)?cache[at&127u]:0;}
/* Declarations may follow routines. Slide the relocatable code backwards on
 * disk when globals grow, before a typed initializer could overwrite it.
 * Globals and code share the actual native static capacity, not separate
 * fixed compiler ceilings. No full image or code page is held in RAM. */
static uint8_t global_space(uint16_t code_length) {
    uint16_t wanted,at,n,i;uint8_t bytes[128];
    if((uint32_t)PREFIX_BYTES+owner->globals_bytes+code_length>CC_STATIC_LIMIT-0x4000u) {
        if(cc.polling&&!cc.error->code)cc_fail(99);return 0;
    }
    wanted=(PREFIX_BYTES+owner->globals_bytes+127u)&0xff80u;
    if(wanted<=code_base)return 1;
    at=(code_used+127u)&0xff80u;
    while(at) {
        at-=128;
        if(!cc_poll())return 0;
        for(i=0;i<128;++i)bytes[i]=raw_get(code_base+at+i);
        for(i=0;i<128;++i)raw_put(wanted+at+i,bytes[i]);
        if(platform_error)return failure();
    }
    for(at=code_base;at<wanted;at+=n) {
        n=wanted-at;if(n>128)n=128;
        for(i=0;i<n;++i)raw_put(at+i,0);
        if(platform_error)return failure();
    }
    code_base=wanted;return 1;
}
void cc_disk_global_put(uint16_t at,uint8_t value) {
    if(global_space(code_used))raw_put(at,value);
}
void cc_disk_put(uint16_t at,uint8_t value) {
    uint16_t offset;
    if(at<CC_CODE_BASE){raw_put(at,value);return;}
    offset=at-CC_CODE_BASE;
    if(!global_space(offset+1))return;
    raw_put(code_base+offset,value);
    if(code_used<=offset)code_used=offset+1;
}
uint8_t cc_disk_get(uint16_t at) {
    if(at<CC_CODE_BASE||!global_space(code_used))return 0;
    return raw_get(code_base+at-CC_CODE_BASE);
}
void cc_disk_place(Build *b,uint16_t origin,uint16_t bytes) {
    code_at=origin-0x4000u;code_bytes=bytes;globals=b->globals_bytes;placed=compiler_disk_placed=1;
}
static uint16_t image_offset(uint16_t at) {
    if(sealed&&chain_kind)return at<96?at:code_base+at-96;
    if(placed&&at>=code_at&&at-code_at<code_bytes)return code_base+at-code_at;
    return at;
}
uint8_t cc_disk_image(const Build *b,uint16_t at) {
    if(!cc_disk_active(b))return 0;
    if(placed&&!chain_kind&&at>=0x4000u+globals&&at<code_at)return 0;
    return raw_get(image_offset(at));
}
/* Final-image writes after placement share the same mapper as reads. */
void cc_disk_final_put(uint16_t at,uint8_t value) {
    raw_put(image_offset(at),value);
}
void cc_disk_seal(Build *b) {
    if(owner==b&&flush())sealed=1;
}
void cc_disk_adopt(const Build *from,Build *to) {if(owner==from)owner=to;}
uint8_t cc_disk_publish(const Build *b,const char *path) {
    uint8_t bytes[128];uint8_t ok=1;uint16_t at=0,n,i;int8_t fd;
    if(!cc_disk_ready(b)||!flush())return failure();
    fd=output_open_path(path,O_WRONLY|O_CREAT|O_TRUNC);if(fd<0)return 0;
    while(at<b->length&&ok) {
        n=b->length-at;if(n>128)n=128;
        for(i=0;i<n;++i)bytes[i]=cc_disk_image(b,at+i);
        ok=!platform_error&&output_write_buffer(fd,bytes,n);at+=n;
    }
    return output_finish(fd,ok);
}
