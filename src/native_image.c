#include "native_image.h"
#include <string.h>
#include <zos_errors.h>
static uint16_t word(const uint8_t *p) {return p[0]|((uint16_t)p[1]<<8);}
static uint8_t zeros(const uint8_t *p,uint8_t n) {while(n--)if(*p++)return 0;return 1;}
uint8_t native_decode(const uint8_t *h,uint8_t chain,NativeImage *id) {
    uint32_t heap;
    uint16_t end,stack=2048,globals;
    memset(id,0,sizeof(*id));id->chain=chain;
#ifdef ZP_CHAIN_ONLY
    if(!chain)return ERR_NOT_SUPPORTED;
#endif
    if(chain) {
        if(memcmp(h,"ZCHN",4)||h[4]!=2||h[5]||word(h+6)!=96||word(h+8)!=1||
           word(h+14)!=0x8000u||!zeros(h+92,4))return ERR_ENTRY_CORRUPTED;
        id->origin=word(h+10);id->bytes=word(h+12);globals=word(h+16);end=word(h+18);
        heap=(uint32_t)id->origin+id->bytes;
        id->length=96u+id->bytes;
        if(!zeros(h+20,64))return ERR_ENTRY_CORRUPTED;
    }
#ifndef ZP_CHAIN_ONLY
    else {
        id->length=word(h+10);heap=word(h+28);end=word(h+30);stack=word(h+32);
        id->origin=word(h+20);id->bytes=word(h+22);globals=word(h+26);
        if(h[0]!=0xc3||memcmp(h+3,"ZP80",4)||h[7]!=2||word(h+8)!=160||
           id->length<16384u||id->length>48128u||word(h+1)!=word(h+12)||
           word(h+12)<0x40a0u||word(h+12)>=(uint32_t)0x4000u+id->length||
           word(h+14)<0x40a0u||word(h+14)>=(uint32_t)0x4000u+id->length||
           word(h+16)!=1||(word(h+18)&~1u)||word(h+24)!=0x8000u||
           (uint32_t)0x4000u+id->length!=heap||!zeros(h+34,2)||
           word(h+132)!=16384u||!zeros(h+140,20))return ERR_ENTRY_CORRUPTED;
        if(!zeros(h+36,96))return ERR_ENTRY_CORRUPTED;
    }
#endif
    if(!id->bytes||(uint32_t)0x8000u+globals>id->origin||
       (uint32_t)id->origin+id->bytes>heap||heap>0xf400UL||
       end<0xcfffu||end>0xfbffu||stack<2048||
       ((heap+1)&~1UL)+stack>(uint32_t)end+1)return ERR_ENTRY_CORRUPTED;
    /* Overlay selection and directory validation remain a separate owner.
     * Never guess the location of a PC in an unsupported overlay artifact. */
    if(chain?!zeros(h+84,8):!zeros(h+134,6))return ERR_NOT_SUPPORTED;
    return 0;
}
