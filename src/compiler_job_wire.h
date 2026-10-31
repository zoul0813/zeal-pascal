#ifndef ZP_COMPILER_JOB_WIRE_H
#define ZP_COMPILER_JOB_WIRE_H
#include "sources.h"

/* These are decoded values, never wire structs. No pointer into a bank or
 * mapped far page escapes a synchronous operation. */
typedef struct {Source source;uint8_t hash[32],resident;uint16_t id;} JobSource;
typedef struct {uint32_t length;uint8_t count,pages[3];} JobStream;
#define JOB_SOURCE_COUNT 86u
uint16_t job_word(const uint8_t *p) SOURCE_BLOCK_CALL;
uint32_t job_long(const uint8_t *p) SOURCE_BLOCK_CALL;
void job_put16(uint8_t *p,uint16_t value) SOURCE_BLOCK_CALL;
void job_put32(uint8_t *p,uint32_t value) SOURCE_BLOCK_CALL;
uint8_t job_zero(const uint8_t *p,uint16_t n) SOURCE_BLOCK_CALL;
uint8_t job_page(const uint8_t allowed[8],uint8_t page) SOURCE_BLOCK_CALL;
uint8_t job_handle(const uint8_t *handle,uint16_t bytes,const uint8_t allowed[8]) SOURCE_BLOCK_CALL;
uint8_t job_read_source(uint8_t page,uint16_t *at,uint16_t end,const uint8_t allowed[8],JobSource *record) SOURCE_BLOCK_CALL;
uint8_t job_write_source(uint8_t page,uint16_t *at,const JobSource *record) SOURCE_BLOCK_CALL;
/* Digest is defined only on success (canonical source bytes). */
uint8_t job_source_digest(const Source *source,uint8_t digest[32]) SOURCE_BLOCK_CALL;
uint8_t job_source_hash(const JobSource *record) SOURCE_BLOCK_CALL;
uint8_t job_validate(const uint8_t context[128],const uint8_t job[128],const uint8_t allowed[8]) SOURCE_BLOCK_CALL;
void job_identity(const uint8_t job[128],uint8_t digest[32]) SOURCE_BLOCK_CALL;
uint8_t job_find_source(const uint8_t job[128],uint16_t id,JobSource *record) SOURCE_BLOCK_CALL;
uint8_t job_find_path(const uint8_t job[128],const char *path,JobSource *record) SOURCE_BLOCK_CALL;
uint8_t job_read_stream(const uint8_t *handle,const uint8_t allowed[8],JobStream *stream) SOURCE_BLOCK_CALL;
void job_hash_stream(const JobStream *stream,uint16_t zero,uint16_t zero_bytes,uint8_t digest[32]) SOURCE_BLOCK_CALL;
#endif
