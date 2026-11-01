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
/* Adopt an existing ready record in an unpacked catalogue, preserving its
 * descriptor. Caller must own the source page (the compiler
 * checks its invocation ledger) and already validate the descriptor.
 * This checks readiness, not arbitrary record contents. NULL origin keeps
 * disk freshness semantics. Failure changes no ownership. */
uint8_t sources_adopt(Sources *set,uint8_t id,const Source *origin) SOURCE_BLOCK_CALL;
/* Invocation-local immutable views never acquire the source page. The catalog
 * itself is still owned. Nonresident references retain only map identity. */
int16_t sources_borrow(Sources *set,const Source *source) SOURCE_BLOCK_CALL;
int16_t sources_reference(Sources *set,const Source *source) SOURCE_BLOCK_CALL;
uint8_t sources_borrowed(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_remove(Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_close(Sources *set) SOURCE_BLOCK_CALL;
/* Move a snapshot catalog into unused root-page space when it fits. Source
 * bytes remain at offset0. Growth/replacement separates storage if needed.
 * No-space is a successful no-op. A failed page release retains both owners. */
uint8_t sources_pack(Sources *set) SOURCE_BLOCK_CALL;
uint8_t sources_ready(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_resident(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_origin(const Sources *set,uint8_t id) SOURCE_BLOCK_CALL;
uint8_t sources_byte(const Sources *set,uint8_t id,uint16_t offset) SOURCE_BLOCK_CALL;
uint8_t sources_path(const char *name,const char *directory,char *out) SOURCE_BLOCK_CALL;
uint8_t sources_canonical(const char *name,char *out) SOURCE_BLOCK_CALL;
uint8_t sources_directory(const char *name,const char *directory,char *out) SOURCE_BLOCK_CALL;

#endif
