#include "../../src/zp.h"
typedef struct {uint32_t length;uint8_t count,pages[3];} JobStream;
uint8_t job_hash_stream(const JobStream *s,uint16_t zero,uint16_t zero_bytes,uint8_t digest[32]);
