#include "zp.h"
/* Resident byte access: logical source banks must not require a code-bank
 * transition for every character scanned or drawn. */
uint8_t buffer_following[64];
uint8_t buffer_next(uint8_t page) NATIVE {
    uint8_t next=page<64?buffer_following[page]:255;
    return next>=32&&next<64?next:255;
}
uint8_t buffer_page(uint8_t page,uint16_t offset) NATIVE {
    while(offset>=PAGE_SIZE&&page!=255) {page=buffer_next(page);offset-=PAGE_SIZE;}
    return page;
}
uint8_t buffer_get(uint8_t page,uint16_t offset) NATIVE {
    page=buffer_page(page,offset);
    return page==255?0:far_get(page,offset%PAGE_SIZE);
}
void buffer_set(uint8_t page,uint16_t offset,uint8_t value) NATIVE {
    uint8_t actual=buffer_page(page,offset);
    if(actual==255)return;
    far_set(actual,offset%PAGE_SIZE,value);
    if(actual!=page)++page_versions[page];
}

/* A compiler include can start partway through a shared physical bank. */
uint8_t buffer_view_get(uint8_t page,uint16_t base,uint16_t offset) NATIVE {
    while(offset>=PAGE_SIZE&&page!=255){page=buffer_next(page);offset-=PAGE_SIZE;}
    offset+=base;
    if(offset>=PAGE_SIZE){page=buffer_next(page);offset-=PAGE_SIZE;}
    return page==255?0:far_get(page,offset);
}
