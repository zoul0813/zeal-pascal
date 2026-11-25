#include "source/catalog.h"
#ifdef __SDCC
#pragma codeseg BANK11
#pragma constseg BANK11
#endif
static uint16_t word(const Sources *s,uint16_t at) {
    return buffer_get(s->page,at)|((uint16_t)buffer_get(s->page,at+1)<<8);
}
#ifdef ZP_OVERLAYS
/* Saved overlays already own an immutable compiler spool. Only its private
 * parser uses kind2; finished builds retain ordinary disk source identities. */
int16_t sources_spooled(Sources *s,const Source *source,uint32_t offset) SOURCE_BLOCK_CALL {
    Source view;int16_t id;uint16_t at;uint8_t i;
    view=*source;view.page=255;id=sources_reference(s,&view);if(id<0)return id;
    at=word(s,(uint16_t)id*2u);buffer_set(s->page,at+9,2);
    for(i=0;i<4;++i)buffer_set(s->page,at+10+i,(uint8_t)(offset>>(8*i)));
    return id;
}
uint8_t sources_spool_offset(const Sources *s,uint8_t id,uint16_t *length,uint32_t *offset) SOURCE_BLOCK_CALL {
    uint16_t at;uint8_t i;
    if(!sources_valid(s)||id>=s->count)return 0;
    at=word(s,(uint16_t)id*2u);if(buffer_get(s->page,at+9)!=2)return 0;
    *length=word(s,at+1);
    *offset=0;for(i=0;i<4;++i)*offset|=(uint32_t)buffer_get(s->page,at+10+i)<<(8*i);
    return 1;
}
#endif
