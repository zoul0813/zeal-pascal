#ifndef ZP_SOURCES_H
#define ZP_SOURCES_H
#include "zp.h"

uint8_t sources_new(Sources *set) SOURCE_BLOCK_CALL;
uint8_t sources_valid(const Sources *set) SOURCE_BLOCK_CALL;
int16_t sources_find(const Sources *set,const char *path) SOURCE_BLOCK_CALL;
uint8_t sources_get(const Sources *set,uint8_t id,Source *source) SOURCE_BLOCK_CALL;
/* Success transfers source page ownership to set. Replacing the same page
 * updates its descriptor. A failed transfer leaves both owners unchanged.
 * An origin records the borrowed editor page used to make a snapshot; NULL
 * denotes a disk dependency, which the compiler must reread before reuse. */
int16_t sources_take(Sources *set,const Source *source,const Source *origin) SOURCE_BLOCK_CALL;
uint8_t sources_remove(Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_close(Sources *set) SOURCE_BLOCK_CALL;
uint8_t sources_hash(const Sources *set,uint8_t id,uint8_t hash[32]) SOURCE_BLOCK_CALL;
uint8_t sources_ready(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_resident(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_origin(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_byte(const Sources *set,uint8_t id,uint16_t offset) SOURCE_BLOCK_CALL;
uint8_t sources_path(const char *name,const char *directory,char *out) SOURCE_BLOCK_CALL;
uint8_t sources_canonical(const char *name,char *out) SOURCE_BLOCK_CALL;
uint8_t sources_directory(const char *name,const char *directory,char *out) SOURCE_BLOCK_CALL;

#endif
