#include "zp.h"
#include "source_internal.h"

/* Compose block edits in one owned page. A span with page255 supplies virtual
 * spaces. Input spans may alias the old source: it remains intact until every
 * byte and line of the result is validated. Failure never adopts partial text. */
typedef struct { uint8_t page; uint16_t first,length; } Span;
static uint8_t compose(Source *s,const Span *parts,uint8_t count,uint16_t cursor,uint16_t previous) {
    Source staged;
    uint32_t size=0;
    uint16_t out=0,i,line=0;
    uint8_t j,c;
    for(j=0;j<count;++j) size+=parts[j].length;
    if(size>SOURCE_LIMIT||!source_new(&staged,s->path)) return 0;
    for(j=0;j<count;++j) for(i=0;i<parts[j].length;++i) {
        c=parts[j].page==255?' ':far_get(parts[j].page,parts[j].first+i);
        if(!line_byte(c,&line)) { source_close(&staged); return 0; }
        far_set(staged.page,out++,c);
    }
    page_initialized(staged.page,out);
    if(!page_drop(s->page)) { source_close(&staged); return 0; }
    s->page=staged.page; s->length=out; s->cursor=cursor;
    s->previous=previous; s->dirty=1; ++s->revision;
    return 1;
}
uint8_t source_insert_from(Source *s,uint16_t p,const Source *from,uint16_t a,uint16_t z,uint16_t spaces) SOURCE_BLOCK_CALL {
    Span parts[4];
    uint16_t n;
    if(p>s->length||a>z||z>from->length||spaces>127) return 0;
    if(a==z&&!spaces) return 1;
    if((uint32_t)(z-a)+spaces>SOURCE_LIMIT) return 0;
    n=z-a+spaces;
    parts[0].page=s->page; parts[0].first=0; parts[0].length=p;
    parts[1].page=255; parts[1].first=0; parts[1].length=spaces;
    parts[2].page=from->page; parts[2].first=a; parts[2].length=z-a;
    parts[3].page=s->page; parts[3].first=p; parts[3].length=s->length-p;
    return compose(s,parts,4,p+n,source_rebase(s->previous>s->length?s->length:s->previous,p,p,n,1));
}
uint8_t source_move(Source *s,uint16_t a,uint16_t z,uint16_t p,uint16_t spaces) SOURCE_BLOCK_CALL {
    Span parts[5];
    uint8_t i;
    uint16_t n=z-a,d,previous;
    if(a>z||z>s->length||p>s->length||spaces>127) return 0;
    if(!n||(p>=a&&p<z)||(p==z&&!spaces)) return 1;
    for(i=0;i<5;++i) parts[i].page=s->page;
    parts[0].first=0;
    parts[4].first=p<a?z:p; parts[4].length=s->length-parts[4].first;
    if(p<a) {
        parts[0].length=p;
        parts[1].page=255; parts[1].first=0; parts[1].length=spaces;
        parts[2].first=a; parts[2].length=n;
        parts[3].first=p; parts[3].length=a-p;
    } else {
        parts[0].length=a;
        parts[1].first=z; parts[1].length=p-z;
        parts[2].page=255; parts[2].first=0; parts[2].length=spaces;
        parts[3].first=a; parts[3].length=n;
    }
    d=p>=z?p-n:p;
    previous=source_rebase(s->previous>s->length?s->length:s->previous,a,z,0,1);
    previous=source_rebase(previous,d,d,n+spaces,1);
    return compose(s,parts,5,d+n+spaces,previous);
}

