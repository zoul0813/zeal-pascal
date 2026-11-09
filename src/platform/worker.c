#include "platform/worker.h"
#include <zos_vfs.h>
#include <zos_errors.h>

/* Parent-owned state, never allocated on the public Pascal Mark/Release heap.
 * This bank runs with parent data/stack restored by native.asm. All allocation
 * still goes through platform.c's authoritative page ledger. */
static uint8_t active,handles[16];

static uint8_t bit(uint8_t fd) { return (uint8_t)(1u<<(fd&7)); }
static void publish(void) {
    uint8_t page,n=0;
    for(page=32;page<64;++page) if(page_role(page)==PAGE_WORKER)
        far_set(worker_child_page,0x3c52u+n++,page);
    for(page=n;page<32;++page) far_set(worker_child_page,0x3c52u+page,0);
    far_set(worker_child_page,0x3c51,n);
}
static uint8_t clean(void) {
    uint8_t i,e,first=0;
    for(i=2;i<128;++i) if(handles[i>>3]&bit(i)) {
        e=close((int8_t)i);
        /* VFS consumes the entry even when a lower driver close fails. */
        handles[i>>3]&=(uint8_t)~bit(i);
        if(e&&!first) first=e;
    }
    for(i=32;i<64;++i) if(page_role(i)==PAGE_WORKER&&!page_drop(i))
        if(!first) first=platform_error;
    if(first) platform_error=first;
    return !first;
}
static uint8_t matches(void) {
    uint8_t i,j,n=0,count=ram_result[81],page;
    if(count>32) return 0;
    for(i=32;i<64;++i) if(page_role(i)==PAGE_WORKER) ++n;
    if(count!=n) return 0;
    for(i=0;i<count;++i) {
        page=ram_result[82+i];
        if(page_role(page)!=PAGE_WORKER) return 0;
        for(j=0;j<i;++j) if(ram_result[82+j]==page) return 0;
    }
    for(i=count;i<32;++i) if(ram_result[82+i]) return 0;
    return 1;
}
uint8_t files_worker_control(uint8_t mode) {
    uint8_t valid=1,ok;
    if(mode==WORKER_START) {
        if(active) { platform_error=ERR_ALREADY_OPENED; return 0; }
        if(!clean()) return 0;
        active=1; return 1;
    }
    if(mode==WORKER_FINISH) valid=active&&matches();
    else if(mode!=WORKER_CANCEL) valid=0;
    active=0;
    ok=clean();
    if(!valid) platform_error=ERR_INVALID_PARAMETER;
    /* A cleanup failure cannot replace a validated primary fault/break. Keep
     * its native error as secondary state; any unreleased page stays owned
     * in the ledger for the next preflight cleanup attempt. */
    return valid&&(ok||(mode==WORKER_FINISH&&
        (ram_result[26]==3||ram_result[26]==5)));
}
static uint8_t mapping(uint8_t actual,uint8_t image) {
    return actual==image||page_role(actual)==PAGE_WORKER;
}
static uint8_t get(uint16_t at) {
    uint8_t page=at<0x8000u?worker_child_prefix:
        at<0xc000u?worker_child_page2:worker_child_page;
    return far_get(page,at&0x3fffu);
}
uint8_t worker_dispatch(uint8_t operation,uint16_t argument) NATIVE {
    uint8_t page,e=ERR_INVALID_PARAMETER,flags,i,fd;
    int8_t opened;
    char path[128];
    worker_value=0xffff;
    if(!active||worker_child_page!=ram_pages[2]||
       !mapping(worker_child_prefix,ram_pages[0])||
       !mapping(worker_child_page2,ram_pages[1])) return e;
    if(operation==WORKER_ALLOC&&argument==0) {
        page=page_new(PAGE_WORKER);
        if(page!=255) { worker_value=page; e=0; }
        else e=platform_error;
    } else if(operation==WORKER_FREE&&argument<64&&page_role((uint8_t)argument)==PAGE_WORKER) {
        e=page_drop((uint8_t)argument)?0:platform_error;
        if(!e) worker_value=0;
    } else if(operation==WORKER_OPEN&&argument>=0x4000u&&argument<=0xfb7fu) {
        flags=get(argument++);
        for(i=0;i<128;++i) { path[i]=(char)get(argument+i); if(!path[i]) break; }
        if(i==128) e=ERR_PATH_TOO_LONG;
        else {
            opened=open(path,flags);
            if(opened<0) e=(uint8_t)-opened;
            else {
                fd=(uint8_t)opened;
                if(fd<2||(handles[fd>>3]&bit(fd))) e=ERR_ALREADY_OPENED;
                else { handles[fd>>3]|=bit(fd); worker_value=fd; e=0; }
            }
        }
    } else if(operation==WORKER_CLOSE&&argument>=2&&argument<128) {
        fd=(uint8_t)argument;
        if(handles[fd>>3]&bit(fd)) {
            e=close((int8_t)fd); handles[fd>>3]&=(uint8_t)~bit(fd);
            worker_value=0;
        }
    }
    publish();
    return e;
}
