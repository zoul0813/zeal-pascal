#ifndef ZP_NATIVE_IMAGE_H
#define ZP_NATIVE_IMAGE_H
#include <stdint.h>
/* Decoded identity, never a serialized host structure. A chain authenticates
 * its source manifest through its map hash rather than a header copy. */
typedef struct {
    uint16_t origin,bytes,length;
    uint8_t chain,id[32],manifest[32],map[32];
} NativeImage;
/* Header is160 bytes for ZP80 or96 for ZCHN. The consumer owns full-stream
 * hashing and exact EOF; zero is success, otherwise a native error. */
uint8_t native_decode(const uint8_t *header,uint8_t chain,NativeImage *identity);
#endif
