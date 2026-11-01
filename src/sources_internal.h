#ifndef ZP_SOURCES_INTERNAL_H
#define ZP_SOURCES_INTERNAL_H
#include "sources.h"

/* Private serialized records, never an installed/source-map ABI. */
typedef char source_layout[sizeof(Source)==140?1:-1];
enum { HASH=140,ORIGIN=172,ORIGIN_LIFETIME=173,ORIGIN_VERSION=177,
       OWN_LIFETIME=181,OWN_VERSION=185,RECORD_BYTES=189,
       SOURCE_COUNT=PAGE_SIZE/RECORD_BYTES };
enum { SOURCE_BORROWED=254,SOURCE_DISK=255 };
/* Change catalog storage without transferring any source-page ownership. */
uint8_t sources_storage(Sources *set,uint8_t packed) SOURCE_BLOCK_CALL;
#endif
