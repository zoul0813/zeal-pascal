#include "compiler_fragments.h"

uint8_t cc_poll(void);
/* One far catalog stores {selected:u16, native_address:u16} for at most4096
 * fragments. Source bytes remain in immutable caller-owned far pages. */
typedef struct {
    const CompilerFragmentObject *objects;
    CompilerFragmentImage *image;
    uint16_t bases[CC_FRAGMENT_OBJECTS+1];
    uint16_t ids[CC_FRAGMENT_OBJECTS],counts[CC_FRAGMENT_OBJECTS];
    uint16_t relocations[CC_FRAGMENT_OBJECTS],payloads[CC_FRAGMENT_OBJECTS];
    uint8_t count,error;
} Link;
static uint8_t fail(Link *l,uint8_t error) {if(!l->error)l->error=error;return 0;}
static uint8_t poll(Link *l) {return !l->error&&(cc_poll()?1:fail(l,CF_CANCELLED));}
static uint8_t live(const CompilerFragmentObject *o) {
    return o->page>=32&&o->page<64&&page_role(o->page)==PAGE_COMPILER&&
        page_lifetime(o->page)==o->lifetime&&o->lifetime&&page_versions[o->page]==o->version;
}
static uint8_t byte(Link *l,uint8_t object,uint16_t offset) {
    const CompilerFragmentObject *o=l->objects+object;
    if(l->error)return 0;
    if(offset>=o->bytes){fail(l,CF_OBJECT);return 0;}
    if(!live(o)){fail(l,CF_OWNERSHIP);return 0;}
    return far_get(o->page,o->offset+offset);
}
static uint16_t word(Link *l,uint8_t object,uint16_t offset) {
    uint16_t value=byte(l,object,offset);return value|((uint16_t)byte(l,object,offset+1)<<8);
}
static uint16_t field(Link *l,uint8_t object,uint16_t fragment,uint8_t offset) {
    return word(l,object,CC_FRAGMENT_HEADER+fragment*CC_FRAGMENT_RECORD+offset);
}
static uint16_t ref(Link *l,uint8_t object,uint16_t relocation,uint8_t offset) {
    return word(l,object,CC_FRAGMENT_HEADER+l->counts[object]*CC_FRAGMENT_RECORD+
        relocation*CC_FRAGMENT_RELOCATION+offset);
}
static uint16_t catalog(Link *l,uint16_t id,uint8_t field) {
    uint16_t at=id*4u+field;
    return far_get(l->image->workspace,at)|((uint16_t)far_get(l->image->workspace,at+1)<<8);
}
static void put(Link *l,uint16_t id,uint8_t field,uint16_t value) {
    uint16_t at=id*4u+field;
    far_set(l->image->workspace,at,(uint8_t)value);
    far_set(l->image->workspace,at+1,(uint8_t)(value>>8));
}
static uint8_t object_id(Link *l,uint16_t identity,uint8_t *object) {
    uint8_t i;
    for(i=0;i<l->count;++i)if(l->ids[i]==identity){*object=i;return 1;}
    return fail(l,CF_TARGET);
}
static uint8_t target(Link *l,uint8_t object,uint16_t relocation,uint16_t *id,uint16_t *addend) {
    uint8_t to;uint16_t fragment,identity=ref(l,object,relocation,2);
    if(l->objects[object].binding_count) {
        const CompilerFragmentObject *o=l->objects+object;uint16_t at;
        if(!identity||identity>o->binding_count)return fail(l,CF_TARGET);
        at=o->bindings+2u*(identity-1u);
        identity=far_get(o->page,at)|((uint16_t)far_get(o->page,at+1)<<8);
    } else if(l->objects[object].binding&&identity==word(l,object,4))identity=l->objects[object].binding;
    if(!object_id(l,identity,&to))return 0;
    fragment=ref(l,object,relocation,4);*addend=ref(l,object,relocation,6);
    if(fragment>=l->counts[to]||*addend>=field(l,to,fragment,2))return fail(l,CF_TARGET);
    *id=l->bases[to]+fragment;return !l->error;
}
static uint8_t validate(Link *l,uint8_t resolve) {
    uint8_t i,j;uint16_t n,r,p,f,first,count,size,at,previous,id,addend;
    uint32_t end;
    l->bases[0]=0;
    for(i=0;i<l->count;++i) {
        if(!poll(l))return 0;
        if(l->objects[i].bytes<CC_FRAGMENT_HEADER||l->objects[i].bytes>PAGE_SIZE||
           (uint32_t)l->objects[i].offset+l->objects[i].bytes>PAGE_SIZE)return fail(l,CF_OBJECT);
        if(!live(l->objects+i))return fail(l,CF_OWNERSHIP);
        if((uint32_t)l->objects[i].bindings+2u*l->objects[i].binding_count>PAGE_SIZE)
            return fail(l,CF_OBJECT);
        for(j=0;j<i;++j)if(l->objects[i].page==l->objects[j].page)return fail(l,CF_OBJECT);
        if(byte(l,i,0)!='Z'||byte(l,i,1)!='P'||byte(l,i,2)!='F'||byte(l,i,3)!='1'||
           word(l,i,12)||word(l,i,14))return fail(l,CF_OBJECT);
        l->ids[i]=word(l,i,4);n=word(l,i,6);r=word(l,i,8);p=word(l,i,10);
        if(!l->ids[i])return fail(l,CF_OBJECT);
        if(l->objects[i].binding)l->ids[i]=l->objects[i].binding;
        for(j=0;j<i;++j)if(l->ids[i]==l->ids[j])return fail(l,CF_OBJECT);
        end=CC_FRAGMENT_HEADER+(uint32_t)n*CC_FRAGMENT_RECORD+(uint32_t)r*CC_FRAGMENT_RELOCATION+p;
        if(end!=l->objects[i].bytes)return fail(l,CF_OBJECT);
        if((uint32_t)l->bases[i]+n>CC_FRAGMENT_LIMIT)return fail(l,CF_CAPACITY);
        l->counts[i]=n;l->relocations[i]=r;l->payloads[i]=(uint16_t)(end-p);l->bases[i+1]=l->bases[i]+n;
    }
    for(i=0;i<l->count;++i) {
        n=0;r=0;
        for(f=0;f<l->counts[i];++f) {
            if(!poll(l))return 0;
            size=field(l,i,f,2);first=field(l,i,f,4);count=field(l,i,f,6);
            if(!size||field(l,i,f,0)!=n||first!=r||
               (uint32_t)n+size>l->objects[i].bytes-l->payloads[i]||
               (uint32_t)r+count>l->relocations[i])return fail(l,CF_OBJECT);
            previous=0;
            for(at=0;at<count;++at) {
                if(!poll(l))return 0;
                p=ref(l,i,r+at,0);
                if((uint32_t)p+2>size||(at&&p<previous))return fail(l,CF_OBJECT);
                previous=p+2;
                if(!ref(l,i,r+at,2))return fail(l,CF_TARGET);
                if(resolve&&!target(l,i,r+at,&id,&addend))return 0;
            }
            n+=size;r+=count;
        }
        if(n!=l->objects[i].bytes-l->payloads[i]||r!=l->relocations[i])return fail(l,CF_OBJECT);
    }
    return !l->error;
}
void cc_fragment_image_init(CompilerFragmentImage *image) CC_FRAGMENT_CALL {
    memset(image,0,sizeof(*image));image->pages[0]=image->pages[1]=image->workspace=255;
}
static uint8_t drop(uint8_t *page,uint32_t *lifetime,uint8_t role) {
    if(*page==255)return 1;
    if(page_role(*page)!=role||page_lifetime(*page)!=*lifetime||!page_drop(*page))return 0;
    *page=255;*lifetime=0;return 1;
}
uint8_t cc_fragment_image_close(CompilerFragmentImage *image) CC_FRAGMENT_CALL {
    uint8_t ok=1,i;
    image->valid=0;
    for(i=0;i<2;++i)if(!drop(image->pages+i,image->lifetimes+i,PAGE_IMAGE))ok=0;
    if(!drop(&image->workspace,&image->workspace_lifetime,PAGE_COMPILER))ok=0;
    if(ok)cc_fragment_image_init(image);
    return ok;
}
static uint8_t clear_page(Link *l,uint8_t page) {
    uint16_t at;
    for(at=0;at<PAGE_SIZE;at+=FAR_FILL_LIMIT)
        if(!poll(l)||!far_fill(page,at,0,FAR_FILL_LIMIT))return fail(l,l->error?l->error:CF_OWNERSHIP);
    page_initialized(page,PAGE_SIZE);return 1;
}
static void emit_byte(Link *l,uint16_t address,uint8_t value) {
    far_set(l->image->pages[(address-0x8000u)/PAGE_SIZE],address%PAGE_SIZE,value);
}
static uint8_t emit(Link *l) {
    uint8_t i,buffer[64];uint16_t f,at,size,n,j,start,source,r,count,to,addend,value,patch;
    for(i=0;i<l->count;++i)for(f=0;f<l->counts[i];++f) {
        if(!poll(l))return 0;
        if(!catalog(l,l->bases[i]+f,0))continue;
        size=field(l,i,f,2);source=l->objects[i].offset+l->payloads[i]+field(l,i,f,0);start=catalog(l,l->bases[i]+f,2);
        if(l->error)return 0;
        for(at=0;at<size;at+=n) {
            n=size-at;if(n>sizeof(buffer))n=sizeof(buffer);
            if(!poll(l))return 0;
            if(!live(l->objects+i)||!far_read(l->objects[i].page,source+at,buffer,n))return fail(l,CF_OWNERSHIP);
            for(j=0;j<n;++j)emit_byte(l,start+at+j,buffer[j]);
        }
        r=field(l,i,f,4);count=field(l,i,f,6);
        for(j=0;j<count;++j) {
            if(!poll(l)||!target(l,i,r+j,&to,&addend))return 0;
            patch=ref(l,i,r+j,0);value=catalog(l,to,2)+addend;
            emit_byte(l,start+patch,(uint8_t)value);emit_byte(l,start+patch+1,(uint8_t)(value>>8));
        }
    }
    return !l->error;
}
static uint8_t link(const CompilerFragmentObject *objects,uint8_t count,
                        const CompilerFragmentRoot *roots,uint16_t root_count,
                        uint16_t origin,uint16_t limit,CompilerFragmentImage *image,CompilerFragmentLayout *layout,uint8_t planning) {
    Link l;uint16_t i,f,r,n,j,to,addend,entry=0,retained=0;uint32_t end=origin;
    uint8_t o,first,last;
    if(!image||!objects||!count||count>CC_FRAGMENT_OBJECTS||!roots||!root_count||
       root_count>CC_FRAGMENT_LIMIT||origin<0x8000u||limit>0xf400u||origin>=limit)return CF_ARGUMENT;
    if((layout&&layout->page!=255)||image->valid||image->pages[0]!=255||image->pages[1]!=255||image->workspace!=255)return CF_ARGUMENT;
    memset(&l,0,sizeof(l));l.objects=objects;l.image=image;l.count=count;
    if(!validate(&l,1))return l.error;
    image->workspace=page_new(PAGE_COMPILER);
    if(image->workspace==255)return CF_MEMORY;
    image->workspace_lifetime=page_lifetime(image->workspace);
    if(!clear_page(&l,image->workspace))goto failed;
    n=0; /* Catalog address words double as the pending worklist until layout. */
    for(i=0;i<root_count;++i) {
        if(!poll(&l)||!object_id(&l,roots[i].identity,&o))goto failed;
        if(roots[i].fragment>=l.counts[o]){fail(&l,CF_TARGET);goto failed;}
        to=l.bases[o]+roots[i].fragment;
        if(!catalog(&l,to,0)){put(&l,to,0,1);put(&l,to,2,n);n=to+1;}
    }
    while(n) {
        to=n-1;n=catalog(&l,to,2);
        for(o=0;o<count&&to>=l.bases[o+1];++o) {}
        f=to-l.bases[o];r=field(&l,o,f,4);j=field(&l,o,f,6);
        if(l.error||!poll(&l))goto failed;
        for(i=0;i<j;++i) {
            if(!poll(&l)||!target(&l,o,r+i,&to,&addend))goto failed;
            if(!catalog(&l,to,0)){put(&l,to,0,1);put(&l,to,2,n);n=to+1;}
        }
    }
    for(o=0;o<count;++o)for(f=0;f<l.counts[o];++f) {
        if(!poll(&l))goto failed;
        if(!catalog(&l,l.bases[o]+f,0))continue;
        n=field(&l,o,f,2);if(l.error)goto failed;
        put(&l,l.bases[o]+f,2,(uint16_t)end);end+=n;++retained;
        if(end>limit){fail(&l,CF_CAPACITY);goto failed;}
    }
    if(!object_id(&l,roots[0].identity,&o))goto failed;
    entry=catalog(&l,l.bases[o]+roots[0].fragment,2);
    /* Allocate only the slots touched by this validated high span. This also
     * avoids mixed16/32-bit rounded arithmetic under SDCC4.4. */
    first=origin>=0xc000u?1:0;last=end>0xc000UL?1:0;
    for(o=first;!planning&&o<=last;++o) {
        image->pages[o]=page_new(PAGE_IMAGE);
        if(image->pages[o]==255){fail(&l,CF_MEMORY);goto failed;}
        image->lifetimes[o]=page_lifetime(image->pages[o]);
        if(!clear_page(&l,image->pages[o]))goto failed;
    }
    if(!planning&&!emit(&l))goto failed;
    for(o=0;o<count;++o)if(!live(objects+o)){fail(&l,CF_OWNERSHIP);goto failed;}
    if(layout) {
        layout->objects=count;layout->fragments=l.bases[count];
        for(i=0;i<count;++i){layout->identities[i]=l.ids[i];layout->bases[i]=l.bases[i];}
        layout->bases[count]=l.bases[count];layout->page=image->workspace;
        layout->lifetime=image->workspace_lifetime;layout->version=page_versions[layout->page];
        image->workspace=255;image->workspace_lifetime=0;
    } else if(!drop(&image->workspace,&image->workspace_lifetime,PAGE_COMPILER)){fail(&l,CF_OWNERSHIP);goto failed;}
    image->origin=origin;image->bytes=(uint16_t)(end-origin);image->entry=entry;
    image->fragments=retained;image->valid=1;return CF_OK;
failed:
    if(!cc_fragment_image_close(image)&&!l.error)l.error=CF_OWNERSHIP;
    return l.error?l.error:CF_OWNERSHIP;
}

void cc_fragment_layout_init(CompilerFragmentLayout *layout) CC_FRAGMENT_CALL {
    memset(layout,0,sizeof(*layout));layout->page=255;
}
uint8_t cc_fragment_layout_close(CompilerFragmentLayout *layout) CC_FRAGMENT_CALL {
    if(!drop(&layout->page,&layout->lifetime,PAGE_COMPILER))return 0;
    cc_fragment_layout_init(layout);return 1;
}
uint8_t cc_fragment_layout_get(const CompilerFragmentLayout *layout,uint16_t identity,
                               uint16_t fragment,uint16_t *address) CC_FRAGMENT_CALL {
    uint8_t i;uint16_t at,value;
    if(!address||layout->page==255)return CF_ARGUMENT;
    if(page_role(layout->page)!=PAGE_COMPILER||page_lifetime(layout->page)!=layout->lifetime||
       page_versions[layout->page]!=layout->version)return CF_OWNERSHIP;
    for(i=0;i<layout->objects;++i)if(layout->identities[i]==identity) {
        if(fragment>=layout->bases[i+1]-layout->bases[i])return CF_TARGET;
        at=(layout->bases[i]+fragment)*4u;
        value=far_get(layout->page,at)|((uint16_t)far_get(layout->page,at+1)<<8);
        if(!value)return CF_TARGET;
        *address=far_get(layout->page,at+2)|((uint16_t)far_get(layout->page,at+3)<<8);return CF_OK;
    }
    return CF_TARGET;
}
uint8_t cc_fragment_link(const CompilerFragmentObject *objects,uint8_t count,
                        const CompilerFragmentRoot *roots,uint16_t root_count,
                        uint16_t origin,uint16_t limit,CompilerFragmentImage *image) CC_FRAGMENT_CALL {
    return link(objects,count,roots,root_count,origin,limit,image,0,0);
}
uint8_t cc_fragment_link_with_layout(const CompilerFragmentObject *objects,uint8_t count,
                        const CompilerFragmentRoot *roots,uint16_t root_count,
                        uint16_t origin,uint16_t limit,CompilerFragmentImage *image,
                        CompilerFragmentLayout *layout) CC_FRAGMENT_CALL {
    if(!layout)return CF_ARGUMENT;
    return link(objects,count,roots,root_count,origin,limit,image,layout,0);
}

uint8_t cc_fragment_validate(const CompilerFragmentObject *objects,uint8_t count) CC_FRAGMENT_CALL {
    Link l;
    if(!objects||!count||count>CC_FRAGMENT_OBJECTS)return CF_ARGUMENT;
    memset(&l,0,sizeof(l));l.objects=objects;l.count=count;
    if(!validate(&l,1))return l.error;
    return CF_OK;
}

uint8_t cc_fragment_plan(const CompilerFragmentObject *objects,uint8_t count,
    const CompilerFragmentRoot *roots,uint16_t root_count,uint16_t origin,uint16_t limit,
    CompilerFragmentImage *image,CompilerFragmentLayout *layout) CC_FRAGMENT_CALL {
    if(!layout)return CF_ARGUMENT;
    return link(objects,count,roots,root_count,origin,limit,image,layout,1);
}

uint8_t cc_fragment_validate_source(const CompilerFragmentObject *object) CC_FRAGMENT_CALL {
    Link l;if(!object)return CF_ARGUMENT;
    memset(&l,0,sizeof(l));l.objects=object;l.count=1;
    return validate(&l,0)?CF_OK:l.error;
}
