#ifndef ZP_OUTPUT_TRANSPORT_H
#define ZP_OUTPUT_TRANSPORT_H
#include "zp.h"

/* Buffers, paths and page vectors cross a bank switch: callers retain them in
 * stack/data, never in the caller's code window.
 * Keyboard decoding owns separate raw storage and does not alter this scratch. */
void output_job_begin(uint8_t enabled) SOURCE_BLOCK_CALL;
void output_job_end(void) SOURCE_BLOCK_CALL;
uint8_t output_poll(void) SOURCE_BLOCK_CALL;
int8_t output_open_path(const char *path,uint8_t flags) SOURCE_BLOCK_CALL;
uint8_t output_finish(int8_t fd,uint8_t ok) SOURCE_BLOCK_CALL;
/* Exact bounded transfers on an already-owned handle. The caller retains
 * handle/transaction ownership; short successful transfers are retried. */
uint8_t output_write_buffer(int8_t fd,const uint8_t *buffer,uint16_t size) SOURCE_BLOCK_CALL;
uint8_t output_read_buffer(int8_t fd,uint8_t *buffer,uint16_t size) SOURCE_BLOCK_CALL;
uint8_t output_store_range(const uint8_t *pages,uint16_t first,uint16_t total,const char *path) SOURCE_BLOCK_CALL;
uint8_t output_verify_range(const uint8_t *pages,uint16_t first,uint16_t total,const char *path) SOURCE_BLOCK_CALL;
/* Restore an invalidated cache from disk. The owner checks retained layout
 * headers and page/source versions before making it usable again. */
uint8_t output_restore_range(const uint8_t *pages,uint16_t first,uint16_t total,const char *path) SOURCE_BLOCK_CALL;
uint8_t output_copy_file(const char *from,const char *to) SOURCE_BLOCK_CALL;
uint8_t output_same_files(const char *a,const char *b) SOURCE_BLOCK_CALL;
#endif
