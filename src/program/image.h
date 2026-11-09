#ifndef ZP_IMAGE_H
#define ZP_IMAGE_H
#include "common/zp.h"
#include "program/decode.h"
uint8_t native_header(const uint8_t *header,uint8_t chain,NativeImage *identity) SOURCE_BLOCK_CALL;
#endif
