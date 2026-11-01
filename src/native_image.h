#ifndef ZP_NATIVE_IMAGE_H
#define ZP_NATIVE_IMAGE_H
#include <stdint.h>
/* Decoded version-2 layout, never a serialized host structure. */
typedef struct {
    uint16_t origin,bytes,length;
    uint8_t chain;
} NativeImage;
/* Header is160 bytes for ZP80 or96 for ZCHN. Former digest slots are
 * reserved zeros. The consumer checks exact EOF; zero means success. */
uint8_t native_decode(const uint8_t *header,uint8_t chain,NativeImage *identity);
#endif
