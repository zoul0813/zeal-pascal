#ifndef ZP_SOURCES_H
#define ZP_SOURCES_H
#include "zp.h"

uint8_t sources_new(Sources *set) SOURCE_BLOCK_CALL;
uint8_t sources_valid(const Sources *set) SOURCE_BLOCK_CALL;
int16_t sources_find(const Sources *set,const char *path) SOURCE_BLOCK_CALL;
uint8_t sources_get(const Sources *set,uint8_t id,Source *source) SOURCE_BLOCK_CALL;
/* Descriptors borrow source pages; only metadata and include text are owned. */
int16_t sources_borrow(Sources *set,const Source *source) SOURCE_BLOCK_CALL;
int16_t sources_include(Sources *set,const Source *source,uint8_t interruptible) SOURCE_BLOCK_CALL;
int16_t sources_reference(Sources *set,const Source *source) SOURCE_BLOCK_CALL;
uint16_t sources_offset(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_finish(Sources *set) SOURCE_BLOCK_CALL;
uint8_t sources_close(Sources *set) SOURCE_BLOCK_CALL;
#ifdef ZP_UNITS_EXPERIMENT
uint8_t sources_release(const Sources *set,Source *loaded) SOURCE_BLOCK_CALL;
/* Parser lookup ignores optional unit debug records. */
int16_t sources_find_input(const Sources *set,const char *path) SOURCE_BLOCK_CALL;
/* Unit debug records append independently, even when authoring paths repeat. */
int16_t sources_unit(Sources *set,const Source *source,uint32_t checksum) SOURCE_BLOCK_CALL;
uint8_t sources_matches(const Sources *set,uint8_t id,const Source *source) SOURCE_BLOCK_CALL;
uint32_t unit_source_fingerprint(const Source *source) SOURCE_BLOCK_CALL;
#endif
uint8_t sources_ready(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_byte(const Sources *set,uint8_t id,uint16_t offset) SOURCE_BLOCK_CALL;
uint8_t sources_path(const char *name,const char *directory,char *out) SOURCE_BLOCK_CALL;
uint8_t sources_canonical(const char *name,char *out) SOURCE_BLOCK_CALL;
uint8_t sources_directory(const char *name,const char *directory,char *out) SOURCE_BLOCK_CALL;

#endif
