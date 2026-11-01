/* Differential edit oracle and validity-certificate invalidation checks. */
#include "zp.h"
#include "source_move.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t memory[PAGE_SIZE],owned;
static uint32_t lifetime;
volatile uint32_t page_versions[64];
static unsigned reads,checks;
uint8_t page_new(uint8_t role) { if(owned)return 255;owned=role;++lifetime;return 32; }
uint8_t page_drop(uint8_t p) {assert(p==32&&owned);owned=0;return 1;}
uint32_t page_lifetime(uint8_t p) {return p==32&&owned?lifetime:0;}
void page_initialized(uint8_t p,uint16_t n) {assert(p==32&&owned&&n<=PAGE_SIZE);}
uint8_t far_get(uint8_t p,uint16_t at) {assert(p==32&&owned&&at<PAGE_SIZE);++reads;return memory[at];}
void far_set(uint8_t p,uint16_t at,uint8_t c) {assert(p==32&&owned&&at<PAGE_SIZE);memory[at]=c;++page_versions[p];}
uint8_t far_move(uint8_t p,uint16_t to,uint16_t from,uint16_t n) {
    assert(p==32&&owned);if(!n)return 1;
    if(n>FAR_MOVE_LIMIT||to>=PAGE_SIZE||from>=PAGE_SIZE||n>PAGE_SIZE-to||n>PAGE_SIZE-from)return 0;
    memmove(memory+to,memory+from,n);page_versions[p]+=n;return 1;
}
static void check(uint8_t condition) {++checks;if(!condition){fprintf(stderr,"source validity check %u failed\n",checks);abort();}}
static void load(Source *s,const uint8_t *text,uint16_t n) {
    uint16_t i;if(owned)source_close(s);check(source_new(s,"H:/VALID.PAS"));
    for(i=0;i<n;++i)far_set(s->page,i,text[i]);s->length=n;page_initialized(s->page,n);
}
/* Independent whole-proposed-buffer oracle, including existing no-op behavior. */
static uint8_t edit(Source *s,uint16_t a,uint16_t z,const uint8_t *text,uint16_t n) {
    uint8_t expected[PAGE_SIZE],before_bytes[PAGE_SIZE],ok=1,result;
    uint32_t size=(uint32_t)s->length-(z-a)+n,version=page_versions[32];
    uint16_t i,column=0;Source before=*s,wanted=*s;
    memcpy(before_bytes,memory,PAGE_SIZE);
    if(a>z||z>s->length||size>SOURCE_LIMIT)ok=0;
    else {
        memcpy(expected,memory,a);if(n)memcpy(expected+a,text,n);memcpy(expected+a+n,memory+z,s->length-z);
        if(a!=z||n)for(i=0;i<size;++i) {
            if(expected[i]==26||expected[i]=='\r'){ok=0;break;}
            if(expected[i]=='\n')column=0;else if(++column>127){ok=0;break;}
        }
    }
    result=source_replace(s,a,z,text,n);check(result==ok);
    if(!ok||(a==z&&!n)) {
        check(!memcmp(&before,s,sizeof(*s))&&!memcmp(before_bytes,memory,PAGE_SIZE)&&version==page_versions[32]);
    } else {
        uint16_t p=before.previous>before.length?before.length:before.previous;
        if(p>=z)p-=z-a;else if(p>=a)p=a;if(p>=a)p+=n;
        wanted.length=(uint16_t)size;wanted.cursor=a+n;wanted.previous=p;wanted.dirty=1;++wanted.revision;
        check(!memcmp(&wanted,s,sizeof(*s))&&!memcmp(expected,memory,size));
        check(page_versions[32]-version==(uint32_t)n+(n==z-a?0:before.length-z));
    }
    return result;
}
static uint32_t seed=0x17a5e1u;
static uint32_t random_word(void) {seed=seed*1664525u+1013904223u;return seed;}
int main(void) {
    Source s;uint8_t large[16001],text[260],bad[3]={'\r',26,'x'};unsigned i,j;uint32_t version;
    for(i=0;i<16000;++i)large[i]=i%100==99?'\n':'a';
    load(&s,large,16000);reads=0;check(edit(&s,8000,8001,(const uint8_t*)"z",1));check(reads==15999);
    reads=0;check(edit(&s,8001,8002,(const uint8_t*)"z",1));check(reads<=508&&reads<16000/10);
    printf("PERF17 warm 16KB overwrite validation reads=%u (cold15999)\n",reads);
    /* Both join boundaries must be included even on a certified page. */
    load(&s,large,16000);check(edit(&s,8000,8001,(const uint8_t*)"z",1));reads=0;
    check(!edit(&s,7999,8000,0,0));check(reads<=508);
    memset(large,'a',63);large[63]='\n';memset(large+64,'a',64);large[128]='\n';
    load(&s,large,16000);check(edit(&s,0,1,(const uint8_t*)"z",1));reads=0;
    check(edit(&s,63,64,0,0));check(reads<=508);
    for(i=0;i<16000;++i)large[i]=i%100==99?'\n':'a';
    load(&s,large,16000);check(edit(&s,8000,8001,(const uint8_t*)"z",1));
    /* Same-value stores invalidate just like changed stores. */
    far_set(32,15000,memory[15000]);reads=0;check(edit(&s,8002,8003,(const uint8_t*)"z",1));check(reads==15999);
    for(i=0;i<3;++i) {
        load(&s,large,16000);check(edit(&s,8000,8001,(const uint8_t*)"z",1));
        if(i<2)far_set(32,15000,bad[i]);else far_set(32,15099,'x');
        check(!edit(&s,8001,8002,(const uint8_t*)"z",1));
    }
    /* The cached extent cannot authenticate hidden bytes on the same page. */
    load(&s,large,16000);far_set(32,16000,'\r');check(edit(&s,8000,8001,(const uint8_t*)"z",1));
    version=page_versions[32];s.length=16001;check(!edit(&s,8001,8002,(const uint8_t*)"y",1));check(page_versions[32]==version);
    /* Reuse with unchanged content/version must miss solely on lifetime. */
    load(&s,large,16000);check(edit(&s,8000,8001,(const uint8_t*)"z",1));version=page_versions[32];
    source_close(&s);check(source_new(&s,"H:/REUSED.PAS"));s.length=16000;reads=0;
    check(edit(&s,8001,8002,(const uint8_t*)"z",1));check(reads==15999&&page_versions[32]==version+1);
    /* An invalid deleted region may be repaired, matching the legacy oracle. */
    load(&s,large,16000);far_set(32,15000,'\r');check(edit(&s,15000,15001,(const uint8_t*)"a",1));
    reads=0;check(edit(&s,8000,8001,(const uint8_t*)"Z",1));check(reads<=508);
    /* Unknown invalid text plus no-op must not create a validity proof. */
    load(&s,large,16000);far_set(32,15000,'\r');check(edit(&s,0,0,0,0));check(!edit(&s,8000,8001,(const uint8_t*)"z",1));
    /* Join/split, capacity, line127/128 and random failures remain atomic. */
    for(i=0;i<20000;++i) {
        uint16_t a,z,n;
        if(i%101==0)load(&s,large,(uint16_t)(random_word()%16001));
        if(i%113==0&&s.length)far_set(32,(uint16_t)(random_word()%s.length),bad[random_word()%3]);
        a=(uint16_t)(random_word()%(s.length+1u));z=a+(uint16_t)(random_word()%(s.length-a+1u));
        n=(uint16_t)(random_word()%sizeof(text));
        for(j=0;j<n;++j){uint32_t r=random_word()%151;text[j]=r==0?'\r':r==1?26:r<5?'\n':'a'+r%26;}
        (void)edit(&s,a,z,text,n);
    }
    memset(text,'a',127);load(&s,text,127);check(edit(&s,0,1,(const uint8_t*)"z",1));reads=0;
    check(edit(&s,1,2,(const uint8_t*)"y",1));
    printf("PERF17 warm single-line validation reads=%u (legacy126)\n",reads);
    check(reads==126);
    /* Independent libc search oracle for the relocated position reader. */
    load(&s,large,16000);
    for(i=0;i<=16100;i+=37) {
        uint16_t line,col,alias,stop=i<s.length?(uint16_t)i:s.length;
        uint16_t expected_line=1,expected_col;
        const uint8_t *at=memory,*found,*last=0;
        Source before=s;uint32_t before_version=page_versions[32];
        while(at<memory+stop&&(found=memchr(at,'\n',(size_t)(memory+stop-at)))!=0) {
            ++expected_line;last=found;at=found+1;
        }
        expected_col=last?(uint16_t)(memory+stop-last):(uint16_t)(stop+1);
        reads=0;source_location(&s,(uint16_t)i,&line,&col);
        check(line==expected_line&&col==expected_col&&reads==stop);
        source_location(&s,(uint16_t)i,&alias,&alias);check(alias==expected_col);
        check(!memcmp(&s,&before,sizeof(s))&&page_versions[32]==before_version);
    }
    source_close(&s);printf("PASS: %u source validity, oracle, lifecycle and atomicity checks\n",checks);return 0;
}
