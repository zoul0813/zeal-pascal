#include "compiler_units.h"

/* Unit serialization merges the existing relocation ledger with the catalog's
 * data/import references. Duplicating every code fixup as a12-byte catalog node
 * needlessly keeps both representations live at the compiler's RAM peak. */
static uint16_t relocation_word(uint16_t index,uint8_t field) {
    uint16_t at=index*RELOCATION_BYTES+field;
    return cc_reloc_get(at)|((uint16_t)cc_reloc_get(at+1)<<8);
}
uint8_t cc_unit_relocations_prepare(void) ROUTINES {
    uint16_t i,j,site,previous,at,from;uint8_t bytes[RELOCATION_BYTES],k;
    for(i=0;i<cc.relocations;++i) {
        if(!cc_poll())return 0;
        site=relocation_word(i,0);
        if((site&0x8000u)||relocation_word(i,2)!=1){cc_fail(UNIT_UNSUPPORTED);return 0;}
        if((uint32_t)site+2u>cc.emitted-CC_CODE_BASE){cc_fail(UNIT_FORMAT);return 0;}
    }
    /* Most entries already arrive in order. Late patches can arrive behind
     * earlier entries; insertion sort moves only that out-of-order suffix. */
    for(i=1;i<cc.relocations;++i) {
        site=relocation_word(i,0);at=i*RELOCATION_BYTES;
        for(k=0;k<RELOCATION_BYTES;++k)bytes[k]=cc_reloc_get(at+k);
        for(j=i;j;--j) {
            if(!cc_poll())return 0;
            previous=relocation_word(j-1,0);
            if(previous==site){cc_fail(UNIT_FORMAT);return 0;}
            if(previous<site)break;
            at=j*RELOCATION_BYTES;from=at-RELOCATION_BYTES;
            for(k=0;k<RELOCATION_BYTES;++k)cc_reloc_put(at+k,cc_reloc_get(from+k));
        }
        at=j*RELOCATION_BYTES;
        for(k=0;k<RELOCATION_BYTES;++k)cc_reloc_put(at+k,bytes[k]);
    }
    return !cc.error->code;
}
void cc_unit_reference_next(UnitReference *cursor) ROUTINES {
    uint16_t site=65535u,node=cursor->next;
    cursor->kind=0;
    if(cc.error->code||!cc_poll())return;
    if(node&&(cc_unit_word(node+2)<CC_CODE_BASE||(uint32_t)cc_unit_word(node+2)+2u>cc.emitted)) {
        cc_fail(UNIT_FORMAT);return;
    }
    if(cursor->relocation<cc.relocations)site=CC_CODE_BASE+relocation_word(cursor->relocation,0);
    if(node&&cursor->relocation<cc.relocations&&site==cc_unit_word(node+2)) {
        /* Imported call chains also have ordinary code fixups. The external
         * target supersedes their placeholder, exactly once. */
        if(cc_unit_word(node)!=UR_EXTERNAL){cc_fail(UNIT_FORMAT);return;}
        ++cursor->relocation;site=65535u;
        if(cursor->relocation<cc.relocations)site=CC_CODE_BASE+relocation_word(cursor->relocation,0);
    }
    if(cursor->relocation<cc.relocations&&(!node||site<cc_unit_word(node+2))) {
        cursor->kind=UR_CODE;cursor->record=0;cursor->site=site;
        cursor->value=cc_code_get(site)|((uint16_t)cc_code_get(site+1)<<8);
        ++cursor->relocation;
    } else if(node) {
        cursor->record=node;cursor->site=cc_unit_word(node+2);cursor->value=cc_unit_word(node+4);
        cursor->kind=(uint8_t)cc_unit_word(node);cursor->next=cc_unit_word(node+6);
        if(cursor->kind!=UR_GLOBAL&&cursor->kind!=UR_EXTERNAL)cc_fail(UNIT_FORMAT);
    }
}
void cc_unit_reference_begin(UnitReference *cursor) ROUTINES {
    memset(cursor,0,sizeof(*cursor));cursor->next=cc_unit_word(UH_REFERENCES);
    cc_unit_reference_next(cursor);
}
