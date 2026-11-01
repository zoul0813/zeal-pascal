#ifndef ZP_SOURCES_INTERNAL_H
#define ZP_SOURCES_INTERNAL_H
#include "sources.h"

/* Private serialized records, never an installed/source-map ABI. */
typedef char source_layout[sizeof(Source)==140?1:-1];
enum { ORIGIN=140,ORIGIN_LIFETIME=141,ORIGIN_VERSION=145,
       OWN_LIFETIME=149,OWN_VERSION=153,RECORD_BYTES=157,
       SOURCE_COUNT=PAGE_SIZE/RECORD_BYTES };
enum { SOURCE_BORROWED=254,SOURCE_DISK=255 };
/* Change catalog storage without transferring any source-page ownership. */
uint8_t sources_storage(Sources *set,uint8_t packed) SOURCE_BLOCK_CALL;
#endif
