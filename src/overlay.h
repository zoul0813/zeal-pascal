#ifndef ZP_OVERLAY_H
#define ZP_OVERLAY_H
#include <stdint.h>

/* Decoded native directory record, never a serialized C structure. The main
 * image owner must also validate directory ordering, parent links and live
 * areas before lending one of these records to the staging loader. */
typedef struct {
    uint16_t group,parent_group,parent_member,origin,bytes,members;
    uint8_t hash[32];
} OverlayGroup;
uint8_t overlay_group_decode(const uint8_t record[48],OverlayGroup *group);

/* A synchronous retained loader owns these pages until discard. Preparation
 * never writes caller code. Runtime trampolines/cache invalidation and the
 * final copy into a proven inactive area belong to the execution owner. */
typedef struct {
    uint8_t pages[2],metadata[32],ready;
    int8_t fd;
    uint16_t origin,bytes,entry,member,error;
    uint8_t detail,id[32],map[32];
} OverlayLoad;
/* Must be called before the first prepare/discard, including for a zeroed C
 * object. Decoded identities/entry are usable only while ready is nonzero.
 * Errors preserve the first failure; a failed discard retains its page ledger
 * and prevents another preparation until cleanup succeeds. */
void overlay_init(OverlayLoad *load);
uint8_t overlay_discard(OverlayLoad *load);
/* Validate the complete group's shape/hash and stage only the selected code.
 * Its member ID, normalized map hash, ZMAP header and full-compilation source
 * manifest are authenticated. Mapping-record/source-content validation is
 * still required by the source-navigation owner before using a map. The path
 * is already resolved by the runtime; the opened handle is never rebound. */
uint8_t overlay_prepare(OverlayLoad *load,const char *path,const OverlayGroup *group,
                        const uint8_t owner[32],const uint8_t manifest[32],uint16_t member);
#endif
