#ifndef ZP_IMAGE_H
#define ZP_IMAGE_H
#include "zp.h"
uint8_t build_image_identity(const Build *b) SOURCE_BLOCK_CALL;
#include "native_image.h"
uint8_t native_header(const uint8_t *header,uint8_t chain,NativeImage *identity) SOURCE_BLOCK_CALL;
#endif
