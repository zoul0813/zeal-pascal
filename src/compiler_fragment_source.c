#include "compiler_fragments.h"
#if defined(__SDCC) && defined(ZP_UNITS_EXPERIMENT)
#pragma codeseg BANK11
#pragma constseg BANK11
#endif

/* A fragment descriptor seals every physical page it borrows. Comparing only
 * the head's version would miss direct child writes and child free/reuse. */
static uint8_t page_at(const CompilerFragmentObject *o,uint8_t index) {
    return index?o->following[index-1]:o->page;
}
uint8_t cc_fragment_source_children_live(const CompilerFragmentObject *o) CC_FRAGMENT_CALL {
    uint8_t i,j,p,previous=o->page;
    if(o->continuations>3)return 0;
    for(i=1;i<=o->continuations;++i) {
        p=page_at(o,i);
        if(p<32||p>=64||page_role(p)!=PAGE_COMPILER||
           !o->following_lifetimes[i-1]||page_lifetime(p)!=o->following_lifetimes[i-1]||
           page_versions[p]!=o->following_versions[i-1])return 0;
        for(j=0;j<i;++j)if(page_at(o,j)==p)return 0;
        if(buffer_next(previous)!=p)return 0;
        previous=p;
    }
    return 1;
}
uint8_t cc_fragment_source_capture(CompilerFragmentObject *o) CC_FRAGMENT_CALL {
    uint8_t p=o->page,i=0,j;
    o->continuations=0;
    while((p=buffer_next(p))!=255) {
        if(i==3)return 0;
        for(j=0;j<=i;++j)if(page_at(o,j)==p)return 0;
        o->following[i]=p;o->following_lifetimes[i]=page_lifetime(p);
        o->following_versions[i]=page_versions[p];o->continuations=++i;
    }
    return cc_fragment_source_live(o);
}
uint8_t cc_fragment_source_overlap(const CompilerFragmentObject *a,const CompilerFragmentObject *b) CC_FRAGMENT_CALL {
    uint8_t i,j;
    for(i=0;i<=a->continuations;++i)for(j=0;j<=b->continuations;++j)
        if(page_at(a,i)==page_at(b,j))return 1;
    return 0;
}
uint8_t cc_fragment_source_read(const CompilerFragmentObject *o,uint16_t at,uint8_t *out,uint16_t count) CC_FRAGMENT_CALL {
    uint16_t n;
    if(count>FAR_READ_LIMIT||(o->stored_bytes&&(uint32_t)at+count>(uint32_t)o->offset+o->stored_bytes)||(uint32_t)at+count>(uint32_t)(o->continuations+1u)*PAGE_SIZE||
       !cc_fragment_source_live(o))return 0;
    while(count) {
        n=PAGE_SIZE-at%PAGE_SIZE;if(n>count)n=count;
        if(!far_read(page_at(o,(uint8_t)(at/PAGE_SIZE)),at%PAGE_SIZE,out,n))return 0;
        count-=n;out+=n;at+=n;
    }
    return 1;
}
