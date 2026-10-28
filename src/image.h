#ifndef ZP_IMAGE_H
#define ZP_IMAGE_H
#include "zp.h"
/* Retained, decoded identity; never a serialized host structure. A chain's
 * source manifest is authenticated by its map hash rather than a header copy. */
typedef struct {
    uint16_t origin,bytes,length;
    uint8_t chain,id[32],manifest[32],map[32];
} NativeImage;
/* Zero on success, a native error otherwise. Header is 160 bytes for ZP80 or
 * 96 for ZCHN. Complete-stream hashing/EOF remain the consuming owner's job. */
uint8_t native_header(const uint8_t *header,uint8_t chain,NativeImage *identity) SOURCE_BLOCK_CALL;
#endif
