#ifndef ZP_HASH_EVIDENCE_H
#define ZP_HASH_EVIDENCE_H
/* Independent test evidence only. Never linked into any Z80 executable, including test programs. */
#include "zp.h"
#include "sha256.h"
void test_source_hash(const Source *s,uint8_t out[32]) SOURCE_BLOCK_CALL;
void test_image_hash(const Build *b,uint8_t out[32]);
#endif
