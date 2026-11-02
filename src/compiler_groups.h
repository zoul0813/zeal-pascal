#ifndef ZP_COMPILER_GROUPS_H
#define ZP_COMPILER_GROUPS_H
#include "compiler_members.h"

/* One far page owns the active compilation's group/member catalog. The
 * supplied private spool handle transfers to this owner on a fresh begin,
 * including on a subsequent allocation/I/O failure. Directory/file removal
 * remains the output transaction owner's responsibility. */
typedef struct CompilerGroups {
    CompilerMemberStore store;
    CompilerLate late;
    uint32_t lifetime,version;
    uint8_t page;uint8_t groups;uint8_t active;
} CompilerGroups;
typedef struct {
    uint16_t origin,bytes,map_first,reloc_first,members;
    TypeId owner;uint8_t parent;uint8_t parent_group;uint8_t parent_member;uint8_t closed;
} CompilerGroup;
void cc_groups_init(CompilerGroups *groups) ROUTINES;
uint8_t cc_groups_begin(CompilerGroups *groups,int8_t fd) ROUTINES;
uint8_t cc_groups_discard(CompilerGroups *groups) ROUTINES;
/* Record a newly declared routine's code domain, not its lexical parent.
 * Scopes predating the first overlay belong to resident domain0. The full
 * 0..255 domain range remains valid, independently of group numbering. */
uint8_t cc_groups_scope(CompilerGroups *groups,uint8_t scope) ROUTINES;
uint8_t cc_groups_domain(CompilerGroups *groups,uint8_t scope,uint8_t *domain) ROUTINES;
/* Group ID255 means failure. An open group's parent emits no code until
 * close. The caller owns routine stubs, late-symbol journals and helper
 * deferral; close reserves/zeros the shared area before those are emitted. */
uint8_t cc_group_open(CompilerGroups *groups,TypeId owner) ROUTINES;
uint8_t cc_group_member_begin(CompilerGroups *groups,uint8_t group,uint8_t scope) ROUTINES;
uint8_t cc_group_member_end(CompilerGroups *groups,uint8_t group,uint8_t scope,uint16_t entry) ROUTINES;
uint8_t cc_group_close(CompilerGroups *groups,uint8_t group) ROUTINES;
uint8_t cc_group_get(CompilerGroups *groups,uint8_t group,CompilerGroup *record) ROUTINES;
uint8_t cc_group_member_get(CompilerGroups *groups,uint8_t scope,uint8_t *group,uint8_t *ordinal,CompilerMember *record) ROUTINES;
#endif
