#ifndef ZP_SHA256_H
#define ZP_SHA256_H
#include <stdint.h>
typedef struct {
    uint32_t h[8], bytes;
    uint8_t block[64], used;
    uint32_t schedule[16], working[8];
} Sha256;
void sha_init(Sha256 *s);
void sha_byte(Sha256 *s,uint8_t byte);
void sha_end(Sha256 *s,uint8_t out[32]);
#endif
