#include "zp.h"
#include "source_internal.h"
#include "source_move.h"
#include "source_validation.h"

static uint8_t new_source(Source *s, const char *path,uint8_t role) {
    if(strlen(path)>127) return 0;
    uint8_t p = page_new(role);
    if (p == 255) return 0;
    memset(s, 0, sizeof(*s));
    s->page = p;
    strcpy(s->path, path);
    return 1;
}
uint8_t source_new(Source *s,const char *path) { return new_source(s,path,PAGE_SOURCE); }
void source_close(Source *s) { page_drop(s->page); }

/* Delete first, then insert at the surviving boundary. Right-biased positions
 * follow the old text; left-biased viewport/selection boundaries retain prefix
 * insertion. Callers validate the edit and bound positions to the old source. */
uint16_t source_rebase(uint16_t p,uint16_t a,uint16_t z,uint16_t n,uint8_t right) {
    if(p>=z) p-=z-a;
    else if(p>=a) p=a;
    if(p>a||(p==a&&right)) p+=n;
    return p;
}

/* Validate the proposed text before the first store. A versioned complete-text
 * proof may narrow the old-byte scan to affected lines. No edit allocates
 * or loses the old buffer on a capacity/line-length error. */
uint8_t source_replace(Source *s, uint16_t a, uint16_t z,
                       const uint8_t *text, uint16_t n) {
    uint16_t i, size, previous, first,last,line=0;
    uint32_t wide;
    if (a > z || z > s->length) return 0;
    if (a==z && !n) return 1;
    wide = (uint32_t)s->length - (z-a) + n;
    if (wide > SOURCE_LIMIT) return 0;
    size = (uint16_t)wide;
    /* Validate prefix, replacement and suffix in their proposed order. This
     * visits every byte not covered by the complete-text proof. */
    /* A hit reads at most510 old bytes, including boundary discovery.
     * Small retained ranges keep the original single scan and avoid calls. */
    first=0; last=s->length;
    if(size-n>1024) source_validation_window(s,a,z,&first,&last);
    for(i=first;i<a;++i)if(!line_byte(far_get(s->page,i),&line))return 0;
    for(i=0;i<n;++i)if(!line_byte(text[i],&line))return 0;
    for(i=z;i<last;++i)if(!line_byte(far_get(s->page,i),&line))return 0;
    previous=s->previous>s->length?s->length:s->previous;
    if(n!=z-a&&!source_move_range(s->page,a+n,z,s->length-z))return 0;
    for (i=0;i<n;++i) far_set(s->page,a+i,text[i]);
    s->length=size;
    page_initialized(s->page,size);
    s->cursor=a+n;
    s->previous=source_rebase(previous,a,z,n,1);
    s->dirty=1;
    ++s->revision;
    if(size>1024) source_validation_record(s);
    return 1;
}

/* One streaming canonicalizer serves both near-byte imports and disk reads.
 * CR/LF state persists across I/O chunks; no old source is changed before commit. */
uint8_t import_begin(SourceImport *in,const char *path,Diagnostic *d) {
    memset(in,0,sizeof(*in)); memset(d,0,sizeof(*d));
    if(!source_new(&in->staged,path)) { d->code=98; return 0; }
    return 1;
}
uint8_t import_byte(SourceImport *in,uint8_t c,Diagnostic *d) {
    if(in->eof) return 1;
    if(c==26) { in->eof=1; return 1; }
    if(in->after_cr&&c=='\n') { in->after_cr=0; return 1; }
    in->after_cr=c=='\r';
    if(c=='\r'||c=='\n') {
        in->out-=in->spaces; in->spaces=0; in->line=0; c='\n';
    } else {
        if(++in->line>127) d->code=100;
        in->spaces=c==' '?in->spaces+1:0;
    }
    if(in->out==SOURCE_LIMIT&&!d->code) d->code=98;
    if(d->code) { d->start=in->out; d->end=in->out+1; return 0; }
    far_set(in->staged.page,in->out++,c); return 1;
}
uint8_t import_finish(SourceImport *in,Source *s,Diagnostic *d) {
    if(d->code) { page_drop(in->staged.page); return 0; }
    in->staged.length=in->out-in->spaces;
    in->staged.revision=s->revision+1;
    if(s->page!=255&&!page_drop(s->page)) { page_drop(in->staged.page); d->code=98; return 0; }
    *s=in->staged; page_initialized(s->page,s->length); return 1;
}
uint8_t source_import(Source *s,const uint8_t *bytes,uint16_t n,Diagnostic *d) {
    SourceImport in; uint16_t i;
    if(!import_begin(&in,s->path,d)) return 0;
    for(i=0;i<n&&!in.eof;++i) if(!import_byte(&in,bytes[i],d)) break;
    return import_finish(&in,s,d);
}

uint8_t source_snapshot(const Source *s, Source *snapshot) {
    uint16_t a=0,z,trim,i,out=0;
    if(!new_source(snapshot,s->path,PAGE_SNAPSHOT)) return 0;
    while(a<s->length) {
        z=a;
        while(z<s->length&&far_get(s->page,z)!='\n') ++z;
        trim=z;
        while(trim>a&&far_get(s->page,trim-1)==' ') --trim;
        for(i=a;i<trim;++i) far_set(snapshot->page,out++,far_get(s->page,i));
        if(z<s->length) far_set(snapshot->page,out++,'\n');
        a=z+1;
    }
    snapshot->length=out; snapshot->revision=s->revision;
    page_initialized(snapshot->page,out);
    return 1;
}
uint16_t source_editor_offset(const Source *s,uint16_t canonical) {
    uint16_t a=0,z,trim,count=0;
    while(a<s->length) {
        z=a;
        while(z<s->length&&far_get(s->page,z)!='\n') ++z;
        trim=z;
        while(trim>a&&far_get(s->page,trim-1)==' ') --trim;
        if(canonical<=count+trim-a) return a+canonical-count;
        count+=trim-a;
        if(z<s->length) ++count;
        a=z+1;
    }
    return s->length;
}
