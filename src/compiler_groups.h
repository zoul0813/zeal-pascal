#ifndef ZP_COMPILER_GROUPS_H
#define ZP_COMPILER_GROUPS_H
#include "compiler_members.h"

/* One far page owns the active compilation's group/member catalog. The
 * supplied private spool handle transfers to this owner on a fresh begin,
 * including on a subsequent allocation/I/O failure. Directory/file removal
 * remains the output transaction owner's responsibility. */
typedef struct {
    CompilerMemberStore store;
    uint32_t lifetime,version;
    uint8_t page,groups,active;
} CompilerGroups;
typedef struct {
    uint16_t origin,bytes,map_first,reloc_first,members;
    uint8_t owner,parent,parent_group,parent_member,closed;
} CompilerGroup;
void cc_groups_init(CompilerGroups *groups) ROUTINES;
uint8_t cc_groups_begin(CompilerGroups *groups,int8_t fd) ROUTINES;
uint8_t cc_groups_discard(CompilerGroups *groups) ROUTINES;
/* Group ID255 means failure. An open group's parent emits no code until
 * close. The caller owns routine stubs, late-symbol journals and helper
 * deferral; close reserves/zeros the shared area before those are emitted. */
uint8_t cc_group_open(CompilerGroups *groups,uint8_t owner) ROUTINES;
uint8_t cc_group_member_begin(CompilerGroups *groups,uint8_t group,uint8_t scope) ROUTINES;
uint8_t cc_group_member_end(CompilerGroups *groups,uint8_t group,uint8_t scope,uint16_t entry) ROUTINES;
uint8_t cc_group_close(CompilerGroups *groups,uint8_t group) ROUTINES;
uint8_t cc_group_get(CompilerGroups *groups,uint8_t group,CompilerGroup *record) ROUTINES;
uint8_t cc_group_member_get(CompilerGroups *groups,uint8_t scope,uint8_t *group,uint8_t *ordinal,CompilerMember *record) ROUTINES;
#endif
