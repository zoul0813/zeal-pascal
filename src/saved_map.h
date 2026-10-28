#ifndef ZP_SAVED_MAP_H
#define ZP_SAVED_MAP_H
#include "sources.h"

typedef struct {
    char path[128];
    uint8_t hash[32];
    uint16_t position;
} SavedLocation;

/* Arguments/results live in retained data or the caller's stack. No mapping
 * window pointer escapes these synchronous, read-only operations. */
uint8_t saved_paths_kind(const char *root,char files[2][128],uint8_t chain) SOURCE_BLOCK_CALL;
uint8_t saved_location_kind(const char *root,const Sources *buffers,uint16_t pc,
                       SavedLocation *location,uint8_t chain) SOURCE_BLOCK_CALL;
uint8_t saved_paths(const char *root,char files[2][128]) SOURCE_BLOCK_CALL;
uint8_t saved_location(const char *root,const Sources *buffers,uint16_t pc,
                       SavedLocation *location) SOURCE_BLOCK_CALL;
#endif
