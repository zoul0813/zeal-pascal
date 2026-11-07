#include "compiler_fragments.h"
#if defined(__SDCC) && defined(ZP_UNITS_EXPERIMENT)
#pragma codeseg TEXT
#pragma constseg TEXT
#endif

/* Like buffer byte access, these hot operations must not switch compiler code
 * banks for every scalar byte. The deeper chain check is needed only for a
 * source that actually has continuations. */
uint8_t cc_fragment_source_live(const CompilerFragmentObject *o) NATIVE {
    if(o->continuations>3||o->page<32||o->page>=64||!o->lifetime||
       page_role(o->page)!=PAGE_COMPILER||page_lifetime(o->page)!=o->lifetime||
       page_versions[o->page]!=o->version)return 0;
    return !o->continuations||cc_fragment_source_children_live(o);
}
uint8_t cc_fragment_source_byte(const CompilerFragmentObject *o,uint16_t at) NATIVE {
    uint8_t index=(uint8_t)(at/PAGE_SIZE);
    return far_get(index?o->following[index-1]:o->page,at%PAGE_SIZE);
}
