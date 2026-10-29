#ifndef ZP_COMPILER_MEMBERS_H
#define ZP_COMPILER_MEMBERS_H
#include "compiler_late.h"

/* Private append-only compiler spool, never a published native artifact.
 * The output transaction owner supplies a fresh, exclusively owned O_RDWR
 * handle and closes it. A failed append poisons the store; its partial bytes
 * remain the transaction owner's recovery evidence. */
typedef struct { uint32_t length; int8_t fd; uint8_t failed; } CompilerMemberStore;
typedef struct {
    uint32_t offset;
    uint16_t origin,bytes,entry,map_first,maps,reloc_first,relocs,lates;
    uint8_t hash[32]; /* trusted catalog identity, not read back from the spool */
} CompilerMember;
uint8_t cc_members_begin(CompilerMemberStore *store,int8_t fd) ROUTINES;
/* Input: origin, entry (absolute preferred address), map_first, reloc_first.
 * Output: immutable section extents and digest, only after verified I/O.
 * The caller journals late symbols before flushing. No compiler cursor, map
 * count or relocation count changes here; the group owner retires those only
 * after success. Parent prefixes and routine descriptors are never serialized. */
uint8_t cc_members_flush(CompilerMemberStore *store,CompilerMember *member,CompilerLate *late,uint16_t first) ROUTINES;
#endif
