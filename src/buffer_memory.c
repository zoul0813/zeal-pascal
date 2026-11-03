#include "zp.h"

/* Native source/map buffers own their continuation banks.
 * Only source allocation mutates it; source descriptors keep their old ABI. */
extern uint8_t buffer_following[64];
#define following buffer_following
void buffer_init(uint8_t page) SOURCE_BLOCK_CALL {following[page]=255;}
uint8_t buffer_reserve(uint8_t page,uint16_t length) SOURCE_BLOCK_CALL {
    uint8_t next,role=page_role(page);
    if(role!=PAGE_SOURCE&&role!=PAGE_SNAPSHOT&&role!=PAGE_MAP&&role!=PAGE_SOURCES)return 0;
    while(length>PAGE_SIZE) {
        next=buffer_next(page);
        if(next==255) {
            next=page_new(role);if(next==255)return 0;
            buffer_init(next);following[page]=next;
        }
        page=next;length-=PAGE_SIZE;
    }
    return 1;
}
void buffer_initialized(uint8_t page,uint16_t length) SOURCE_BLOCK_CALL {
    do {
        page_initialized(page,length>PAGE_SIZE?PAGE_SIZE:length);
        length=length>PAGE_SIZE?length-PAGE_SIZE:0;
        page=buffer_next(page);
    }while(page!=255);
}
uint8_t buffer_drop(uint8_t page) SOURCE_BLOCK_CALL {
    uint8_t last,previous,next;
    if(page==255)return 1;
    /* Retain the head and every surviving link when a native free fails. */
    for(;;) {
        last=page;previous=255;
        while((next=buffer_next(last))!=255){previous=last;last=next;}
        if(!page_drop(last))return 0;
        following[last]=255;
        if(previous==255)return 1;
        following[previous]=255;
    }
}

/* Retired storage follows the live text as spare capacity until its last free
 * succeeds. Only pages beyond the complete live buffer are ever trimmed. */
void buffer_retire(uint8_t page,uint8_t retired,uint16_t length) SOURCE_BLOCK_CALL {
    uint8_t last=page,next,error=platform_error;
    if(retired!=255) {
        while((next=buffer_next(last))!=255)last=next;
        following[last]=retired;
    }
    last=page;
    while(length>PAGE_SIZE){last=buffer_next(last);length-=PAGE_SIZE;}
    next=buffer_next(last);
    if(next!=255&&buffer_drop(next))following[last]=255;
    /* A completed edit remains successful if releasing spare storage fails. */
    platform_error=error;
}

uint8_t buffer_append(uint8_t page) SOURCE_BLOCK_CALL {
    uint8_t next=page_new(page_role(page));
    if(next==255)return 255;
    buffer_init(next);following[page]=next;return next;
}
