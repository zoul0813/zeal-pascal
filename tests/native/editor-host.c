#include "editor/editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

static uint8_t bytes[16][PAGE_SIZE],owned[16];
volatile uint32_t page_versions[64];
uint8_t platform_error;
static unsigned assertions,allocations,releases;
static int drop_failure=-1;
static uint32_t lifetimes[16];
uint8_t page_new(uint8_t role) {
    uint8_t i;
    ++allocations;
    for(i=0;i<16;++i) if(!owned[i]) { owned[i]=role; ++lifetimes[i]; return 32+i; }
    return 255;
}
uint32_t page_lifetime(uint8_t p) { return p>=32&&p<48&&owned[p-32]?lifetimes[p-32]:0; }
uint8_t page_drop(uint8_t p) { assert(p>=32&&p<48&&owned[p-32]); if(p==drop_failure){platform_error=1;return 0;}owned[p-32]=0;++releases;return 1; }
uint8_t pages_used(void) {uint8_t i,n=0;for(i=0;i<16;++i)n+=owned[i]!=0;return n;}
uint8_t page_role(uint8_t p) {return p>=32&&p<48?owned[p-32]:0;}
void page_initialized(uint8_t p,uint16_t n) { assert(p>=32&&p<48&&owned[p-32]&&n<=PAGE_SIZE); }
uint8_t far_get(uint8_t p,uint16_t at) { assert(p>=32&&p<48&&owned[p-32]&&at<PAGE_SIZE); return bytes[p-32][at]; }
void far_set(uint8_t p,uint16_t at,uint8_t b) { assert(p>=32&&p<48&&owned[p-32]&&at<PAGE_SIZE); bytes[p-32][at]=b; ++page_versions[p]; }
#include "far-move-model.h"
uint8_t far_move(uint8_t p,uint16_t to,uint16_t from,uint16_t n) {return model_far_move(p,to,from,n);}
void check(uint8_t ok) { ++assertions; if(!ok) { fprintf(stderr,"editor assertion %u failed: cursor=%u column=%u top=%u\n",assertions,editor.source->cursor,editor.column,editor.top); abort(); } }
void editor_checks(Source *s);
/* A failed native free must never turn a committed source into a truncated
 * chain, or lose an aborted staging owner outside the active text extent. */
static void banked_text(const Source *s,const uint8_t *expected,uint16_t length) {
    uint16_t i;for(i=0;i<length;++i)if(buffer_get(s->page,i)!=expected[i])break;
    check(s->length==length&&i==length);
}
static uint8_t banked_owned(const Source *s) {
    uint8_t p=s->page,n=0,seen[16]={0};
    while(p!=255) {
        check(p>=32&&p<48&&owned[p-32]&&!seen[p-32]);seen[p-32]=1;++n;
        p=buffer_next(p);
    }
    return n;
}
static void banked_failure_checks(Source *s) {
    static uint8_t original[40000],replacement[20000],expected[56464];
    Source before,absent;SourceImport in;Diagnostic diagnostic;
    uint16_t i,length;uint8_t mode,failed,p,old;
    unsigned previous_allocations;uint32_t revision;
    source_close(s);check(!pages_used());
    for(i=0;i<40000;++i)original[i]=i%80==79?'\n':(uint8_t)('a'+i%26);
    for(i=0;i<20000;++i)replacement[i]=i%80==79?'\n':'z';
    /* Overlapping in-place shifts must cross both native bank boundaries in
     * each direction, preserving the head version used by source freshness. */
    for(mode=1;mode<=2;++mode) {
        uint16_t at=(uint16_t)mode*PAGE_SIZE-7;
        uint32_t version;
        check(source_new(s,"H:/SHIFT.PAS"));check(source_import(s,original,40000,&diagnostic));
        memcpy(expected,original,at);memcpy(expected+at,"TEST",4);
        memcpy(expected+at+4,original+at,40000-at);version=page_versions[s->page];
        check(source_replace(s,at,at,(const uint8_t*)"TEST",4));
        check(page_versions[s->page]!=version);banked_text(s,expected,40004);
        version=page_versions[s->page];check(source_replace(s,at,at+4,0,0));
        check(page_versions[s->page]!=version);banked_text(s,original,40000);
        source_close(s);check(!pages_used());
    }
    for(mode=0;mode<3;++mode)for(failed=0;failed<3;++failed) {
        check(source_new(s,"H:/BANKED.PAS"));check(source_import(s,original,40000,&diagnostic));
        old=s->page;p=old;for(i=0;i<failed;++i)p=buffer_next(p);
        check(p!=255);drop_failure=p;releases=0;revision=s->revision;platform_error=0;
        if(mode==0) {
            memcpy(expected,replacement,20000);length=20000;
            check(source_import(s,replacement,20000,&diagnostic)&&!diagnostic.code);
        } else if(mode==1) {
            memcpy(expected,original+80,39920);memcpy(expected+39920,original,80);length=40000;
            check(source_move(s,0,80,40000,0));
        } else {
            memcpy(expected,original,20000);memcpy(expected+20000,original,80);
            memcpy(expected+20080,original+20000,20000);length=40080;
            check(source_insert_from(s,20000,s,0,80,0));
        }
        check(s->page!=old&&s->revision==revision+1&&owned[p-32]&&!platform_error);
        if(failed<2)check(releases>0); /* Some old tail pages were freed first. */
        banked_text(s,expected,length);check(banked_owned(s)==pages_used());
        /* The retained old bank is usable spare capacity, not a hidden orphan. */
        previous_allocations=allocations;memcpy(expected+length,original,PAGE_SIZE);
        check(source_replace(s,length,length,original,PAGE_SIZE)&&allocations==previous_allocations);
        banked_text(s,expected,length+PAGE_SIZE);check(banked_owned(s)==pages_used());
        drop_failure=-1;source_close(s);check(!pages_used());
    }
    for(failed=0;failed<3;++failed) {
        check(source_new(s,"H:/BANKED.PAS"));check(source_import(s,original,40000,&diagnostic));before=*s;
        check(import_begin(&in,s->path,&diagnostic));
        for(i=0;i<40000&&import_byte(&in,original[i],&diagnostic);++i){}
        check(i==40000&&!diagnostic.code);
        p=in.staged.page;for(i=0;i<failed;++i)p=buffer_next(p);drop_failure=p;
        for(i=0;i<128&&import_byte(&in,'x',&diagnostic);++i){}
        check(diagnostic.code==100&&!import_finish(&in,s,&diagnostic));
        check(!memcmp(s,&before,sizeof(*s))&&in.staged.page==255&&owned[p-32]);
        banked_text(s,original,40000);check(banked_owned(s)==pages_used());
        drop_failure=-1;source_close(s);check(!pages_used());
    }
    /* Failed import into an absent descriptor has no owner to preserve. Normal
     * cleanup keeps it absent; failed cleanup returns a reachable empty owner. */
    memset(&absent,0,sizeof(absent));absent.page=255;strcpy(absent.path,"H:/ABSENT.PAS");
    memset(replacement,'x',128);check(!source_import(&absent,replacement,128,&diagnostic));
    check(absent.page==255&&!pages_used());
    check(import_begin(&in,absent.path,&diagnostic));
    for(i=0;i<40000&&import_byte(&in,original[i],&diagnostic);++i){}
    check(i==40000&&!diagnostic.code);
    drop_failure=buffer_next(in.staged.page);diagnostic.code=100;
    check(!import_finish(&in,&absent,&diagnostic)&&absent.page!=255&&!absent.length);
    check(banked_owned(&absent)==pages_used());drop_failure=-1;source_close(&absent);check(!pages_used());
    check(source_new(s,"H:/EDITOR.PAS"));
}

int main(void) {
    Source s;
    check(source_new(&s,"H:/EDITOR.PAS"));
    editor_checks(&s);banked_failure_checks(&s);source_close(&s); check(!pages_used());
    printf("PASS: %u editor buffer, command, viewport and failure assertions\n",assertions);
    return 0;
}
