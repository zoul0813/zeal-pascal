#include "compiler_fragments.h"
#ifdef __SDCC
#include <zos_sys.h>
#include <zos_vfs.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#endif

volatile uint16_t test_passed,test_failed,test_index,test_failures[64];
volatile uint8_t test_done,fragment_status;
static uint8_t roles[64],used,peak_used,fail_alloc,alloc_calls,fail_free,free_page,poison_alloc_at;
static uint32_t lives[64],next_life;
static uint16_t polls,cancel_at,mutate_at;
static uint8_t mutate_page;
static CompilerFragmentObject objects[2];
static CompilerFragmentImage image;
static CompilerFragmentRoot roots[4];
#ifndef __SDCC
volatile uint32_t page_versions[64];
static uint8_t memory[64][PAGE_SIZE];
uint8_t far_get(uint8_t p,uint16_t at) {assert(p<64&&at<PAGE_SIZE&&roles[p]);return memory[p][at];}
void far_set(uint8_t p,uint16_t at,uint8_t value) {assert(p<64&&at<PAGE_SIZE&&roles[p]);memory[p][at]=value;++page_versions[p];}
uint8_t far_read(uint8_t p,uint16_t at,uint8_t *out,uint16_t n) {
    assert(p<64&&roles[p]&&n<=FAR_READ_LIMIT&&(uint32_t)at+n<=PAGE_SIZE);memcpy(out,memory[p]+at,n);return 1;
}
uint8_t far_fill(uint8_t p,uint16_t at,uint8_t value,uint16_t n) {
    assert(p<64&&roles[p]&&n<=FAR_FILL_LIMIT&&(uint32_t)at+n<=PAGE_SIZE);
    memset(memory[p]+at,value,n);page_versions[p]+=n;return 1;
}
#else
uint8_t fragment_before[7],fragment_after[7];uint16_t fragment_value;
uint8_t code_pages[16]; /* Unused worker path in the real far-access assembly. */
void worker_dispatch(void) {}
void test_complete(void);
void test_machine(uint8_t *out) NATIVE;
uint16_t fragment_run(uint8_t page,uint16_t entry) NATIVE;
#endif
static void check(uint8_t ok) {
    ++test_index;if(ok)++test_passed;else {if(test_failed<64)test_failures[test_failed]=test_index;++test_failed;}
}
uint8_t page_new(uint8_t role) {
    uint8_t p=255;
    if(++alloc_calls==fail_alloc)return 255;
#ifdef __SDCC
    if(palloc(&p))return 255;
#else
    for(p=32;p<64&&roles[p];++p) {}
    if(p==64)return 255;
#endif
    if(p<32||p>=64||roles[p])return 255;
    roles[p]=role;lives[p]=++next_life;++used;if(used>peak_used)peak_used=used;
    if(alloc_calls==poison_alloc_at)free_page=p;
    return p;
}
uint8_t page_drop(uint8_t p) {
    if(p<32||p>=64||!roles[p]||(fail_free&&p==free_page))return 0;
#ifdef __SDCC
    if(pfree(p))return 0;
#endif
    roles[p]=0;lives[p]=0;--used;return 1;
}
uint8_t page_role(uint8_t p) {return p<64?roles[p]:0;}
uint32_t page_lifetime(uint8_t p) {return p<64?lives[p]:0;}
void page_initialized(uint8_t p,uint16_t n) {(void)p;(void)n;}
uint8_t cc_poll(void) {
    ++polls;
    if(polls==mutate_at&&mutate_page!=255)far_set(mutate_page,0,far_get(mutate_page,0));
    return polls!=cancel_at;
}
static void poke(CompilerFragmentObject *o,uint16_t at,uint16_t value) {
    far_set(o->page,at,(uint8_t)value);far_set(o->page,at+1,(uint8_t)(value>>8));
}
static void seal(CompilerFragmentObject *o) {o->lifetime=page_lifetime(o->page);o->version=page_versions[o->page];}
static void object(CompilerFragmentObject *o,uint16_t id,uint8_t count,const uint16_t *sizes,
                   uint8_t relocations,const uint16_t *refs,const uint16_t *owners) {
    uint16_t i,j,offset=0,r=0,start=16u+8u*count+8u*relocations;
    o->page=page_new(PAGE_COMPILER);check(o->page!=255);if(o->page==255)return;
    o->bytes=start;for(i=0;i<count;++i)o->bytes+=sizes[i];
    for(i=0;i<o->bytes;++i)far_set(o->page,i,0);
    far_set(o->page,0,'Z');far_set(o->page,1,'P');far_set(o->page,2,'F');far_set(o->page,3,'1');
    poke(o,4,id);poke(o,6,count);poke(o,8,relocations);poke(o,10,o->bytes-start);
    for(i=0;i<count;++i) {
        poke(o,16+8*i,offset);poke(o,18+8*i,sizes[i]);poke(o,20+8*i,r);
        j=r;while(r<relocations&&owners[r]==i)++r;poke(o,22+8*i,r-j);
        for(j=0;j<sizes[i];++j)far_set(o->page,start+offset+j,(uint8_t)(0x90+i));
        offset+=sizes[i];
    }
    for(i=0;i<4u*relocations;++i)poke(o,16+8*count+2*i,refs[i]);
    seal(o);
}
static void build_objects(void) {
    static const uint16_t a_sizes[]={5,9000,2,3},b_sizes[]={4,9000};
    static const uint16_t a_owners[]={0,1,3},b_owners[]={0,1};
    static const uint16_t a_refs[]={1,2,0,0, 1,1,3,0, 1,1,1,0};
    static const uint16_t b_refs[]={1,1,2,0, 1,2,1,0};
    uint16_t a,b;
    object(objects,1,4,a_sizes,3,a_refs,a_owners);object(objects+1,2,2,b_sizes,2,b_refs,b_owners);
    a=16+4*8+3*8;b=16+2*8+2*8;
    far_set(objects[0].page,a,0xcd);far_set(objects[0].page,a+3,0x23);far_set(objects[0].page,a+4,0xc9);
    poke(objects,a+5+9000,41);
    far_set(objects[1].page,b,0x2a);far_set(objects[1].page,b+3,0xc9);
    seal(objects);seal(objects+1);
    roots[0].identity=1;roots[0].fragment=0;
    roots[1].identity=1;roots[1].fragment=1;
    roots[2].identity=1;roots[2].fragment=3;
    roots[3].identity=2;roots[3].fragment=1;
}
static uint8_t link(uint16_t root_count,uint16_t origin,uint16_t limit) {
    polls=0;alloc_calls=0;peak_used=used;
    fragment_status=cc_fragment_link(objects,2,roots,root_count,origin,limit,&image);return fragment_status;
}
static uint8_t get(uint16_t address) {if(!image.valid)return 0;return far_get(image.pages[(address-0x8000u)/PAGE_SIZE],address%PAGE_SIZE);}
static uint16_t getword(uint16_t address) {uint16_t lo=get(address);return lo|((uint16_t)get(address+1)<<8);}
static void positive(void) {
    uint16_t origin,at;
    for(origin=0x8000;origin<=0x8100;origin+=0x100) {
        check(link(1,origin,0xf400)==CF_OK);if(!image.valid)return;check(image.valid&&image.bytes==11&&image.fragments==3&&image.entry==origin);
        check(used==3&&peak_used==4&&image.pages[1]==255&&image.workspace==255);
        check(get(origin)==0xcd&&getword(origin+1)==origin+7&&get(origin+3)==0x23&&get(origin+4)==0xc9);
        check(getword(origin+5)==41&&get(origin+7)==0x2a&&getword(origin+8)==origin+5&&get(origin+10)==0xc9);
        for(at=0x8000;at<origin;++at)check(get(at)==0);
        check(get(origin+11)==0&&get(0xbfff)==0);
#ifdef __SDCC
        {uint16_t result=fragment_run(image.pages[0],image.entry);
        check(result==42);check(!memcmp(fragment_before,fragment_after,7));}
#endif
        check(cc_fragment_link(objects,2,roots,1,origin,0xf400,&image)==CF_ARGUMENT&&image.valid);
        check(cc_fragment_image_close(&image)&&used==2);
    }
    check(link(1,0xbffe,0xf400)==CF_OK);if(!image.valid)return;check(image.pages[1]!=255&&image.bytes==11&&used==4);
    check(getword(0xbfff)==0xc005&&getword(0xc003)==41&&getword(0xc006)==0xc003);
    check(cc_fragment_image_close(&image)&&used==2);
    check(link(4,0x8000,0xf400)==CF_OK);check(image.bytes==18014&&image.fragments==6&&image.pages[1]!=255&&used==4&&peak_used==5);
    check(getword(0x8001)==0xa332&&getword(0xa32d)==41&&getword(0x8006)==0xa32f);
    check(getword(0xa330)==0x8005&&getword(0xa333)==0xa32d&&getword(0xa337)==0xa336);
    check(get(0xbfff)==0x91&&get(0xc000)==0x91&&get(0xc65d)==0x91);
    check(cc_fragment_image_close(&image)&&used==2);
    check(link(4,0x8000,0xc000)==CF_CAPACITY&&used==2&&!image.valid);
    check(link(1,0x8000,0xc000)==CF_OK&&used==3);check(cc_fragment_image_close(&image)&&used==2);
}
static void negative(void) {
    CompilerFragmentObject saved;uint16_t old;uint8_t error;
    static const uint16_t locations[]={0,4,6,8,10,12,14,16,18,20,22,48,50,52,54,56,6,8,26,30};
    static const uint16_t values[]={0,0,65535,65535,0,1,1,1,0,1,65535,4,3,4,4,8999,8196,8195,65535,65535};
    static const uint8_t errors[]={CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_TARGET,CF_TARGET,CF_TARGET,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT,CF_OBJECT};
    uint8_t i;saved=objects[0];
    for(i=0;i<sizeof(locations)/sizeof(locations[0]);++i) {
        old=far_get(saved.page,locations[i])|((uint16_t)far_get(saved.page,locations[i]+1)<<8);
        poke(objects,locations[i],values[i]);seal(objects);error=link(1,0x8000,0xf400);
        check(error==errors[i]);check(!image.valid&&used==2&&image.workspace==255&&alloc_calls==0);
        poke(objects,locations[i],old);seal(objects);
    }
    poke(objects,22,2);seal(objects);check(link(1,0x8000,0xf400)==CF_OBJECT&&alloc_calls==0&&used==2);
    poke(objects,22,1);seal(objects);
    roles[objects[0].page]=PAGE_IMAGE;check(link(1,0x8000,0xf400)==CF_OWNERSHIP&&alloc_calls==0&&used==2);
    roles[objects[0].page]=PAGE_COMPILER;
    saved=objects[0];--objects[0].bytes;check(link(1,0x8000,0xf400)==CF_OBJECT&&alloc_calls==0&&used==2);objects[0]=saved;
    --objects[0].version;check(link(1,0x8000,0xf400)==CF_OWNERSHIP&&alloc_calls==0&&used==2);objects[0]=saved;
    --objects[0].lifetime;check(link(1,0x8000,0xf400)==CF_OWNERSHIP&&alloc_calls==0&&used==2);objects[0]=saved;
    objects[0].page=255;check(link(1,0x8000,0xf400)==CF_OWNERSHIP&&alloc_calls==0&&used==2);objects[0]=saved;
    objects[0]=objects[1];check(link(1,0x8000,0xf400)==CF_OBJECT&&alloc_calls==0&&used==2);objects[0]=saved;
    poke(objects+1,4,1);seal(objects+1);check(link(1,0x8000,0xf400)==CF_OBJECT&&alloc_calls==0&&used==2);poke(objects+1,4,2);seal(objects+1);
    roots[0].identity=3;check(link(1,0x8000,0xf400)==CF_TARGET&&used==2);roots[0].identity=1;
    roots[0].fragment=4;check(link(1,0x8000,0xf400)==CF_TARGET&&used==2);roots[0].fragment=0;
    check(link(0,0x8000,0xf400)==CF_ARGUMENT&&used==2);
    check(link(1,0x7fff,0xf400)==CF_ARGUMENT&&used==2);
    check(link(1,0x8000,0xffff)==CF_ARGUMENT&&used==2);
    check(link(1,0xf3ff,0xf400)==CF_CAPACITY&&used==2);
    check(cc_fragment_link(objects,33,roots,1,0x8000,0xf400,&image)==CF_ARGUMENT&&used==2);
}
static void faults(void) {
    uint16_t total,i;uint8_t p;
    check(link(1,0x8000,0xf400)==CF_OK);total=polls;check(cc_fragment_image_close(&image)&&used==2);
    for(i=1;i<=total;++i) {
        cancel_at=i;check(link(1,0x8000,0xf400)==CF_CANCELLED);check(!image.valid&&used==2&&image.workspace==255);
    }
    cancel_at=0;
    for(i=1;i<=3;++i) {
        fail_alloc=(uint8_t)i;check(link(4,0x8000,0xf400)==CF_MEMORY);check(!image.valid&&used==2);
    }
    fail_alloc=0;
    mutate_page=objects[1].page;
    for(i=0;i<3;++i) {
        mutate_at=i==0?10:i==1?total/2:total;
        check(link(1,0x8000,0xf400)==CF_OWNERSHIP&&!image.valid&&used==2);seal(objects+1);
    }
    mutate_at=0;
    /* Force the workspace release to fail, preserving its ownership handle. */
    p=page_new(PAGE_COMPILER);check(p!=255);check(page_drop(p));poison_alloc_at=1;fail_free=1;
    check(link(1,0x8000,0xf400)==CF_OWNERSHIP&&!image.valid&&used==3&&image.workspace==free_page);
    check(!cc_fragment_image_close(&image)&&used==3&&image.workspace==free_page);
    fail_free=0;poison_alloc_at=0;check(cc_fragment_image_close(&image)&&used==2);
}
/* Disk is exercised on target with the real SDK. The host runs the same bytes
 * through stdio; this private fixture loader is not the future unit importer. */
static void disk(CompilerFragmentObject *o,const char *path) {
    uint8_t buffer[64],p=o->page;uint16_t at,n,j;uint8_t ok=1;
#ifdef __SDCC
    int8_t fd=open(path,O_WRONLY|O_CREAT|O_TRUNC);check(fd>=0);if(fd<0)return;
#else
    FILE *fd=fopen(path,"wb");check(fd!=0);if(!fd)return;
#endif
    for(at=0;at<o->bytes;at+=n) {
        n=o->bytes-at;if(n>sizeof(buffer))n=sizeof(buffer);
        for(j=0;j<n;++j)buffer[j]=far_get(p,at+j);
#ifdef __SDCC
        {uint16_t size=n;if(write(fd,buffer,&size)||size!=n)ok=0;}
#else
        if(fwrite(buffer,1,n,fd)!=n)ok=0;
#endif
    }
#ifdef __SDCC
    if(close(fd))ok=0;
#else
    if(fclose(fd))ok=0;
#endif
    check(ok);check(page_drop(p));o->page=page_new(PAGE_COMPILER);check(o->page!=255);
#ifdef __SDCC
    fd=open(path,O_RDONLY);check(fd>=0);if(fd<0)return;
#else
    fd=fopen(path,"rb");check(fd!=0);if(!fd)return;
#endif
    for(at=0;at<o->bytes;at+=n) {
        n=o->bytes-at;if(n>sizeof(buffer))n=sizeof(buffer);
#ifdef __SDCC
        {uint16_t size=n;if(read(fd,buffer,&size)||size!=n)ok=0;}
#else
        if(fread(buffer,1,n,fd)!=n)ok=0;
#endif
        for(j=0;j<n;++j)far_set(o->page,at+j,buffer[j]);
    }
#ifdef __SDCC
    if(close(fd))ok=0;
#else
    if(fclose(fd))ok=0;
#endif
    check(ok);seal(o);
}
static void ownership_and_edges(void) {
    CompilerFragmentObject tiny;CompilerFragmentRoot root;
    static const uint16_t sizes[]={2},owners[]={0},refs[]={0,7,0,0};
    uint8_t p;uint32_t life;
    object(&tiny,7,1,sizes,1,refs,owners);root.identity=7;root.fragment=0;
    check(cc_fragment_link(&tiny,1,&root,1,0xf3fe,0xf400,&image)==CF_OK);
    check(image.bytes==2&&image.entry==0xf3fe&&getword(0xf3fe)==0xf3fe&&image.pages[0]==255&&image.pages[1]!=255&&used==4);
    check(cc_fragment_image_close(&image));check(page_drop(tiny.page)&&used==2);
    check(link(1,0x8000,0xf400)==CF_OK);p=image.pages[0];life=lives[p];++lives[p];
    check(!cc_fragment_image_close(&image)&&!image.valid&&image.pages[0]==p&&used==3);
    lives[p]=life;check(cc_fragment_image_close(&image)&&used==2);
    check(link(1,0x8000,0xf400)==CF_OK);free_page=image.pages[0];fail_free=1;
    check(!cc_fragment_image_close(&image)&&!image.valid&&image.pages[0]==free_page&&used==3);
    fail_free=0;check(cc_fragment_image_close(&image)&&used==2);
}
static void patches_and_cycles(void) {
    CompilerFragmentObject extra,inputs[3];CompilerFragmentRoot root;
    static const uint16_t sizes[]={4,3},owners[]={0,0};
    static const uint16_t refs[]={0,7,1,1, 2,2,0,3};
    uint8_t i;uint16_t origin;
    object(&extra,7,2,sizes,2,refs,owners);inputs[0]=objects[0];inputs[1]=objects[1];inputs[2]=extra;
    root.identity=7;root.fragment=0;
    alloc_calls=0;check(cc_fragment_link(inputs,3,&root,1,0x8000,0xf400,&image)==CF_OK);
    check(image.fragments==4&&image.bytes==13&&image.entry==0x8006);
    check(getword(0x8006)==0x800b&&getword(0x8008)==0x8005&&getword(0x8003)==0x8000);
    check(cc_fragment_image_close(&image)&&used==3);
    for(i=0;i<3;++i) {
        poke(&extra,32,i==2?2:0);poke(&extra,40,i==0?1:0);seal(&extra);inputs[2]=extra;alloc_calls=0;
        check(cc_fragment_link(inputs,3,&root,1,0x8000,0xf400,&image)==CF_OBJECT&&alloc_calls==0&&used==3);
    }
    poke(&extra,32,0);poke(&extra,40,2);poke(&extra,46,4);seal(&extra);inputs[2]=extra;alloc_calls=0;
    check(cc_fragment_link(inputs,3,&root,1,0x8000,0xf400,&image)==CF_TARGET&&alloc_calls==0&&used==3);
    poke(&extra,46,3);poke(&extra,38,3);seal(&extra);inputs[2]=extra;alloc_calls=0;
    check(cc_fragment_link(inputs,3,&root,1,0x8000,0xf400,&image)==CF_TARGET&&alloc_calls==0&&used==3);
    poke(&extra,34,1);poke(&extra,36,1);poke(&extra,38,0);seal(&extra);inputs[2]=extra;
    check(cc_fragment_link(inputs,3,&root,1,0x8000,0xf400,&image)==CF_OK);
    check(image.fragments==5&&image.bytes==9013&&image.entry==0xa331);
    check(getword(0xa331)==0x8000&&getword(0x8001)==0xa32a&&getword(0xa32b)==0x8000);
    check(cc_fragment_image_close(&image)&&used==3);check(page_drop(extra.page)&&used==2);
    roots[0].fragment=1;check(link(1,0x8000,0xf400)==CF_OK);
    check(image.fragments==2&&image.bytes==9003&&getword(0x8001)==0xa328&&getword(0xa329)==0x8000);
    check(get(0x8000)==0x91&&get(0xa327)==0x91&&get(0xa328)==0x93);
    check(cc_fragment_image_close(&image)&&used==2);roots[0].fragment=0;
    for(origin=0xbff5;origin<=0xbff6;++origin) {
        check(link(1,origin,0xf400)==CF_OK);
        check(image.pages[0]!=255&&(origin==0xbff5?image.pages[1]==255:image.pages[1]!=255));
        check(used==(origin==0xbff5?3:4)&&image.bytes==11&&getword(origin+1)==origin+7);
        check(cc_fragment_image_close(&image)&&used==2);
    }
    check(link(1,0xc000,0xf400)==CF_OK);
    check(image.pages[0]==255&&image.pages[1]!=255&&used==3&&getword(0xc001)==0xc007);
    check(cc_fragment_image_close(&image)&&used==2);
}
static void dense(CompilerFragmentObject *o,uint16_t identity,uint16_t count) {
    uint16_t i,start=16+8*count;
    o->page=page_new(PAGE_COMPILER);check(o->page!=255);if(o->page==255)return;
    o->bytes=start+count;
    for(i=0;i<o->bytes;++i)far_set(o->page,i,0);
    far_set(o->page,0,'Z');far_set(o->page,1,'P');far_set(o->page,2,'F');far_set(o->page,3,'1');
    poke(o,4,identity);poke(o,6,count);poke(o,10,count);
    for(i=0;i<count;++i){poke(o,16+8*i,i);poke(o,18+8*i,1);far_set(o->page,start+i,0xc9);}
    seal(o);
}
static void capacity(void) {
    CompilerFragmentObject dense_objects[3],full;CompilerFragmentRoot root;
    uint16_t i;uint8_t j;
    dense(dense_objects,10,1818);dense(dense_objects+1,11,1818);dense(dense_objects+2,12,460);
    root.identity=12;root.fragment=459;alloc_calls=0;
    check(cc_fragment_link(dense_objects,3,&root,1,0x8000,0xf400,&image)==CF_OK);
    check(image.fragments==1&&image.bytes==1&&image.entry==0x8000&&get(0x8000)==0xc9);
    check(cc_fragment_image_close(&image)&&used==5);
    check(page_drop(dense_objects[2].page));dense(dense_objects+2,12,461);alloc_calls=0;
    check(cc_fragment_link(dense_objects,3,&root,1,0x8000,0xf400,&image)==CF_CAPACITY&&alloc_calls==0&&used==5);
    for(j=0;j<3;++j)check(page_drop(dense_objects[j].page));check(used==2);
    full.page=page_new(PAGE_COMPILER);check(full.page!=255);full.bytes=PAGE_SIZE;
    for(i=0;i<PAGE_SIZE;++i)far_set(full.page,i,0);
    far_set(full.page,0,'Z');far_set(full.page,1,'P');far_set(full.page,2,'F');far_set(full.page,3,'1');
    poke(&full,4,13);poke(&full,6,1);poke(&full,10,PAGE_SIZE-24);poke(&full,18,PAGE_SIZE-24);
    far_set(full.page,PAGE_SIZE-1,0xa7);seal(&full);root.identity=13;root.fragment=0;
    check(cc_fragment_link(&full,1,&root,1,0x8000,0xf400,&image)==CF_OK);
    check(image.bytes==PAGE_SIZE-24&&image.pages[1]==255&&get(0xbfe7)==0xa7);
    check(cc_fragment_image_close(&image));check(page_drop(full.page)&&used==2);
}
#ifndef __SDCC
static void maximum_objects(void) {
    CompilerFragmentObject all[32];CompilerFragmentRoot root;uint8_t i;
    for(i=0;i<32;++i)dense(all+i,i+1,i==31?1:0);
    root.identity=32;root.fragment=0;alloc_calls=0;
    /* All32 physical RAM slots hold valid source objects. Header validation
     * must reach bases[32], then workspace allocation must fail, not overrun. */
    check(cc_fragment_link(all,32,&root,1,0x8000,0xf400,&image)==CF_MEMORY&&alloc_calls==1&&used==32);
    check(cc_fragment_image_close(&image)&&used==32);
    for(i=0;i<32;++i)check(page_drop(all[i].page));check(!used);
}
#endif
static void independent(void) {
    uint8_t i,ok=1;uint16_t at;
    check(link(1,0x8000,0xf400)==CF_OK);if(!image.valid)return;
    /* Overwrite the actual source pages before releasing them; no assumption
     * about the OS allocator's next page-selection order is needed. */
    for(i=0;i<2;++i)for(at=0;at<PAGE_SIZE;at+=FAR_FILL_LIMIT)
        if(!far_fill(objects[i].page,at,0xcc,FAR_FILL_LIMIT))ok=0;
    check(ok);for(i=0;i<2;++i)check(page_drop(objects[i].page));
    check(used==1&&image.valid&&getword(0x8001)==0x8007&&getword(0x8005)==41&&getword(0x8008)==0x8005);
#ifdef __SDCC
    {uint16_t result=fragment_run(image.pages[0],image.entry);
    check(result==42);check(!memcmp(fragment_before,fragment_after,7));}
#endif
}

int main(void) {
    free_page=255;mutate_page=255;cc_fragment_image_init(&image);build_objects();
#ifdef __SDCC
    disk(objects,"H:/FRAGA.ZPF");disk(objects+1,"H:/FRAGB.ZPF");
#else
    disk(objects,"FRAGA.ZPF");disk(objects+1,"FRAGB.ZPF");
#endif
    positive();
    if(fragment_status) {
        cc_fragment_image_close(&image);page_drop(objects[0].page);page_drop(objects[1].page);goto done;
    }
    negative();faults();ownership_and_edges();patches_and_cycles();capacity();independent();
    check(cc_fragment_image_close(&image));check(!used);
#ifndef __SDCC
    maximum_objects();
#endif
done:
#ifdef __SDCC
    test_done=255;test_complete();
#else
    printf("%u fragment assertions passed, %u failed\n",test_passed,test_failed);
    if(test_failed){uint16_t i;for(i=0;i<test_failed&&i<64;++i)printf("failure %u\n",test_failures[i]);}
#endif
    return test_failed?1:0;
}
