/* Host structural/failure tests for the exact C front end used on Zeal.
 * These are deliberately not counted as native execution tests. */
#include "compiler.h"
#include "image.h"
#include "transfer_asset.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static uint8_t memory[64][PAGE_SIZE],owned[64];
static uint32_t lifetimes[64],next_lifetime;
volatile uint32_t page_versions[64];
static int fail_after=-1,fail_drop=-1;
static unsigned checks,allocation_calls;
static uint8_t cancel_stage,cancel_result;
static unsigned poll_calls;
uint8_t key_interrupt(void) {
    uint8_t reached;
    ++poll_calls;
    reached=cancel_stage==1||
        (cancel_stage==2&&cc.image->pages[0]!=255)||
        (cancel_stage==3&&cc.emitted>CC_CODE_BASE)||
        (cancel_stage==4&&cc.image->map_length!=0)||
        (cancel_stage==5&&cc.scan>512);
    return reached?cancel_result:0;
}
uint8_t page_new(uint8_t role) {
    uint8_t i;
    ++allocation_calls;
    if(fail_after==0) return 255;
    if(fail_after>0) --fail_after;
    for(i=32;i<64;++i) if(!owned[i]) { owned[i]=role;lifetimes[i]=++next_lifetime;return i; }
    return 255;
}
uint8_t page_drop(uint8_t p) { assert(p<64&&owned[p]);if(p==fail_drop)return 0;owned[p]=0;return 1; }
uint8_t page_role(uint8_t p) { return p<64?owned[p]:0; }
uint32_t page_lifetime(uint8_t p) { return p<64&&owned[p]?lifetimes[p]:0; }
typedef struct {const char *path,*text;} HostSource;
static const HostSource *disk_sources;
static unsigned disk_source_count,disk_reads;
uint8_t read_block(Source *source,const char *name,Diagnostic *diagnostic) {
    char path[128];unsigned i;
    strcpy(path,name);memset(diagnostic,0,sizeof(*diagnostic));++disk_reads;
    for(i=0;i<disk_source_count;++i)if(!strcmp(path,disk_sources[i].path)) {
        if(!source_new(source,path)){diagnostic->code=98;return 0;}
        if(source_import(source,(const uint8_t*)disk_sources[i].text,(uint16_t)strlen(disk_sources[i].text),diagnostic))return 1;
        source_close(source);return 0;
    }
    return 0;
}
uint8_t pages_used(void) { uint8_t n=0,i; for(i=32;i<64;++i) n+=owned[i]!=0; return n; }
void page_initialized(uint8_t p,uint16_t n) { assert(p<64&&owned[p]&&n<=PAGE_SIZE); }
uint8_t far_get(uint8_t p,uint16_t at) { assert(p<64&&owned[p]&&at<PAGE_SIZE); return memory[p][at]; }
void far_set(uint8_t p,uint16_t at,uint8_t v) { assert(p<64&&owned[p]&&at<PAGE_SIZE); ++page_versions[p]; memory[p][at]=v; }
static Source s;
static Build b;
static Diagnostic d;
static void checked(int value,unsigned line) {
    ++checks;
    if(!value) {
        fprintf(stderr,"check %u, line %u, diagnostic %u at %u, emitted=%u static=%u temporaries=%u\n",
                checks,line,d.code,d.start,cc.emitted,b.globals_bytes,cc.temporaries);
        abort();
    }
}
#define check(value) checked((value),__LINE__)
static void load(const char *text) { check(source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d)); }
static void invalid(const char *text,uint8_t code) {
    uint8_t ok;
    load(text); ok=compile(&s,&b,&d);
    if(ok||d.code!=code) fprintf(stderr,"expected diagnostic %u: %s\n",code,text);
    check(!ok&&d.code==code); check(pages_used()==1);
}
static void compile_cancellation(void) {
    Source before;uint8_t content[128],stage,result,hash[32];unsigned calls;
    load("{unsaved}\nprogram Cancel;\nvar n:integer;\nbegin n:=7;writeln(n) end.\n");
    s.cursor=9;s.dirty=1;before=s;
    memcpy(content,memory[s.page],s.length);
    for(stage=1;stage<=4;++stage)for(result=1;result<=2;++result) {
        cancel_stage=stage;cancel_result=result;poll_calls=0;
        check(!compile_project(&s,0,&b,&d,1,0));
        check(d.code==COMPILE_INTERRUPTED&&compile_interrupt_reason()==result);
        check(poll_calls>0&&!cc.polling&&!b.valid&&pages_used()==1);
        if(stage==4&&result==1)printf("Compile cancellation seal checkpoint: %u\n",poll_calls);
        check(!memcmp(&s,&before,sizeof(s))&&!memcmp(content,memory[s.page],s.length));
        check(b.pages[0]==255&&b.pages[1]==255&&b.pages[2]==255&&b.map_page==255&&b.sources.page==255);
        cancel_stage=0;
        check(compile_project(&s,0,&b,&d,1,0));check(!cc.polling&&compile_interrupt_reason()==0);
        calls=poll_calls;
        /* Completed-build verification must never use the expired job's
         * diagnostic or consume input, even when the next poll would abort. */
        cancel_stage=1;cc_hash_image(&b,hash);
        check(!memcmp(hash,b.id,32)&&build_check(&s,&b)&&poll_calls==calls);
        build_close(&b);check(pages_used()==1);
    }
    calls=poll_calls;
    check(compile(&s,&b,&d)&&poll_calls==calls);build_close(&b);
    cancel_stage=0;
    {
        char text[4096];unsigned i;
        strcpy(text,"{\n");
        for(i=0;i<100;++i)strcat(text,"a long comment must remain cancellable\n");
        strcat(text,"}\nbegin end.\n");load(text);before=s;
        cancel_stage=5;cancel_result=1;
        check(!compile_project(&s,0,&b,&d,1,0)&&d.code==COMPILE_INTERRUPTED);
        check(cc.scan>512&&cc.scan<1024&&!memcmp(&s,&before,sizeof(s)));
        check(!b.valid&&!cc.polling&&pages_used()==1);
        cancel_stage=0;check(compile(&s,&b,&d));build_close(&b);
    }
}
static void compilation_options(void) {
    const uint16_t starts[]={0,0x8000,0x8202,0x9000,0xbfff,0xc000,0xc001,0xd000,0xef00,0xf400,0xffff};
    const uint16_t ends[]={0,0xcffe,0xcfff,0xd000,0xe123,0xfbfe,0xfbff,0xfc00,0xffff};
    CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};CompileLimits limits;
    uint8_t identity[32];uint16_t bytes,globals,i,j,at,typed;
    uint32_t start,heap;uint8_t expected,ok,gap_zero;
    load("program Layout;\nvar pad:array[1..513]of byte;marker:integer;\n"
         "const typed:integer=17;\nprocedure p;begin pad[1]:=7 end;\n"
         "begin p;marker:=typed;writeln(marker,pad[1])end.\n");
    check(compile(&s,&b,&d));bytes=b.code_bytes;globals=b.globals_bytes;
    check(globals==515);memcpy(identity,b.id,32);build_close(&b);
    for(i=0;i<sizeof(starts)/sizeof(starts[0]);++i)for(j=0;j<sizeof(ends)/sizeof(ends[0]);++j) {
        options.start=starts[i];options.end=ends[j];
        start=options.start?options.start:GLOBAL_BASE+globals;
        heap=(start+bytes+1)&~1UL;
        expected=options.end>=0xcfffu&&options.end<=0xfbffu&&
            start>=(uint32_t)GLOBAL_BASE+globals&&heap+2048u<=(uint32_t)options.end+1;
        check(compile_limits(globals,bytes,&options,&limits)==expected);
        ok=compile_project(&s,0,&b,&d,0,&options);check(ok==expected);
        if(ok) {
            check(b.code_origin==start&&b.code_bytes==bytes&&b.globals_bytes==globals);
            check(image_get(&b,30)==(uint8_t)options.end&&image_get(&b,31)==(uint8_t)(options.end>>8));
            check(b.length==start-0x4000u+bytes&&build_check(&s,&b));
            gap_zero=1;
            for(at=GLOBAL_BASE+globals-0x4000u;at<start-0x4000u;++at)if(image_get(&b,at))gap_zero=0;
            check(gap_zero);
            typed=0;
            for(at=0;at<cc.symbol_count;++at)if(cc.symbols[at].role==S_TYPED) {
                typed=cc.symbols[at].address+b.code_origin-CC_CODE_BASE-0x4000u;
                check(image_get(&b,typed)==17&&image_get(&b,typed+1)==0);
            }
            check(typed!=0);
            check((!memcmp(identity,b.id,32))==(start==GLOBAL_BASE+globals&&options.end==0xfbffu));
            build_close(&b);
        } else check(d.code==99&&!b.valid);
        check(pages_used()==1);
    }
    options.start=0xd001;options.end=(uint16_t)(((0xd001UL+bytes+1)&~1UL)+2048u-1);
    check(compile_project(&s,0,&b,&d,0,&options));build_close(&b);
    --options.end;check(!compile_project(&s,0,&b,&d,0,&options)&&d.code==99&&pages_used()==1);
    options.start=0;options.end=0xfbff;
    check(!compile_limits(0xffff,1,&options,&limits));
    check(!compile_limits(0,0xffff,&options,&limits));
}
/* Execute only the generated activation instruction subset. Ordinary globals
 * are caller-owned sentinels; all other memory starts as an earlier program's
 * unrelated bytes. Native execution independently checks the same handoff. */
static void chain_cases(void) {
    static uint8_t caller[65536],writes[65536];
    const char *programs[]={"{$C-}var shared:integer;const value:integer=17;begin shared:=value end.",
        "{$C-}var shared:integer;begin if ConOutPtr=0 then halt;writeln(shared)end.",
        ("{$C-}var shared:integer;begin if ConStPtr=0 then halt;if ConOutPtr=0 then halt;\n"
        "if LstOutPtr=0 then halt;if AuxOutPtr=0 then halt;if UsrOutPtr=0 then halt;writeln(shared)end.")};
    const uint16_t origins[]={0,0x9000,0xc001,0xd100};
    CompileOptions options={0,0xfbffu,COMPILE_CHAIN};
    NativeImage identity;uint8_t header[160],a=0,op,expected[3],zero;
    uint16_t i,j,k,ip,hl=0,address,target,sites[5],first;unsigned scenario;
    for(scenario=0;scenario<3;++scenario)for(k=0;k<(scenario?1:4);++k) {
        load(programs[scenario]);options.start=origins[k];
        check(compile_project(&s,0,&b,&d,0,&options));check(build_is_chain(&b));
        check(b.length==96+b.code_bytes&&b.globals_bytes>=2&&build_check(&s,&b));
        for(i=0;i<96;++i)header[i]=image_get(&b,i);
        check(native_header(header,1,&identity)==0&&identity.origin==b.code_origin&&identity.bytes==b.code_bytes);
        check(!memcmp(identity.id,b.id,32)&&identity.length==b.length);
        memset(caller,0xa5,sizeof(caller));memset(writes,0,sizeof(writes));
        for(i=0;i<b.code_bytes;++i)caller[b.code_origin+i]=image_get(&b,96+i);
        ip=b.code_origin;first=ip;
        while(ip<first+CC_ACTIVATION_BYTES) {
            op=caller[ip++];
            if(op==0)continue;
            if(op==0x3e){a=caller[ip++];continue;}
            if(op==0x21){hl=caller[ip]|((uint16_t)caller[ip+1]<<8);ip+=2;continue;}
            if(op==0xc3){check(ip==first+CHAIN_ENTRY_BYTES-2);target=caller[ip]|((uint16_t)caller[ip+1]<<8);check(target==first+CC_ACTIVATION_BYTES);ip=target;break;}
            check(op==0x32||op==0x22);address=caller[ip]|((uint16_t)caller[ip+1]<<8);ip+=2;
            check(address<GLOBAL_BASE||address>=GLOBAL_BASE+2);
            caller[address]=op==0x32?a:(uint8_t)hl;writes[address]=1;
            if(op==0x22){caller[address+1]=(uint8_t)(hl>>8);writes[address+1]=1;}
        }
        check(ip==first+CC_ACTIVATION_BYTES&&caller[GLOBAL_BASE]==0xa5&&caller[GLOBAL_BASE+1]==0xa5);
        for(i=0;i<CHAIN_PROFILE_BYTES;++i)check(caller[first+CHAIN_ENTRY_BYTES+i]==cc_runtime_profile_get(i)&&!writes[first+CHAIN_ENTRY_BYTES+i]);
        sites[0]=cc_runtime_private(RP_INPUT_KEY);sites[1]=cc_runtime_get(RT_CHAR-0x4000u+1)|((uint16_t)cc_runtime_get(RT_CHAR-0x4000u+2)<<8);
        sites[2]=cc_runtime_private(RP_INPUT_ECHO);sites[3]=cc_runtime_private(RP_INPUT_OPEN);sites[4]=cc_runtime_private(RP_NORMAL_ERROR);
        for(i=0;i<5;++i) {
            address=sites[i];target=i==0?cc.keyboard_entry:i==1?cc.hook_character:i==2?cc.hook_echo:i==3?cc.hook_open:cc.error_entry;
            if(target){target+=b.code_origin-CC_CODE_BASE;expected[0]=0xc3;expected[1]=(uint8_t)target;expected[2]=(uint8_t)(target>>8);}
            else for(j=0;j<3;++j)expected[j]=cc_runtime_get(address-0x4000u+j);
            for(j=0;j<3;++j)check(caller[address+j]==expected[j]&&writes[address+j]);
        }
        check(caller[RT_CBREAK]==0&&writes[RT_CBREAK]);
        for(i=0;i<HOOK_COUNT;++i)if(cc.hook_variables[i]) {
            address=cc.hook_variables[i];target=cc.hook_defaults[i]+b.code_origin-CC_CODE_BASE;
            check(caller[address]==(uint8_t)target&&caller[address+1]==(uint8_t)(target>>8));
        }
        for(i=0;i<cc.symbol_count;++i)if(cc.symbols[i].role==S_TYPED) {
            address=96+cc.symbols[i].address-CC_CODE_BASE;
            check(image_get(&b,address)==17&&image_get(&b,address+1)==0);
        }
        zero=image_get(&b,97);far_set(b.pages[0],97,zero^1);check(!build_check(&s,&b));
        far_set(b.pages[0],97,zero);check(build_check(&s,&b));
        build_close(&b);check(pages_used()==1);
    }
    options.kind=2;load("begin end.");check(!compile_project(&s,0,&b,&d,0,&options)&&d.code==99&&!b.valid&&pages_used()==1);
}

/* Compare every embedded byte, rebasing only the caller's declared operands.
 * Module/gate bytes are private fixed-address assets, never Pascal relocations. */
static void chain_call_cases(void) {
    CompileOptions options={0x8100,0xfbffu,COMPILE_PROGRAM};
    uint16_t i,j,entry,module,offset,actual,expected,at,one_bytes=0;
    uint8_t kind,repeated,expected_byte;
    for(kind=0;kind<2;++kind)for(repeated=0;repeated<2;++repeated) {
        options.kind=kind?COMPILE_CHAIN:COMPILE_PROGRAM;
        options.start=kind?0x8200:0x8100;
        load(repeated?"var f:file;begin Chain(f);Chain(f)end.":"var f:file;begin Chain(f)end.");
        check(compile_project(&s,0,&b,&d,0,&options));
        check(cc.chain_entry&&cc.file_entry&&cc.temporaries==0&&build_check(&s,&b));
        at=kind?20:36;expected_byte=image_get(&b,at);
        far_set(b.pages[0],at,expected_byte^1);check(!build_check(&s,&b));
        far_set(b.pages[0],at,expected_byte);check(build_check(&s,&b));
        expected_byte=far_get(b.map_page,20);
        far_set(b.map_page,20,expected_byte^1);check(!build_check(&s,&b));
        far_set(b.map_page,20,expected_byte);check(build_check(&s,&b));
        entry=cc.chain_entry;module=entry-TRANSFER_MODULE_BYTES-TRANSFER_GATE_BYTES;
        offset=kind?96:b.code_origin-0x4000;
        for(i=0;i<TRANSFER_MODULE_BYTES+TRANSFER_GATE_BYTES;++i) {
            expected_byte=i<TRANSFER_MODULE_BYTES?cc_transfer_asset_get(0,i):
                cc_transfer_asset_get(1,i-TRANSFER_MODULE_BYTES);
            check(image_get(&b,offset+module-CC_CODE_BASE+i)==expected_byte);
        }
        actual=entry+b.code_origin-CC_CODE_BASE;
        for(i=0;i<TRANSFER_CALL_BYTES;++i) {
            expected_byte=cc_transfer_asset_get(2,i);
            for(j=0;j<TRANSFER_CALL_RELOC_COUNT+3;++j) {
                if(j<TRANSFER_CALL_RELOC_COUNT) {
                    at=TRANSFER_CALL_RELOC_OFFSET+2*j;
                    at=(cc_transfer_asset_get(2,at)|((uint16_t)cc_transfer_asset_get(2,at+1)<<8))-CC_CODE_BASE;
                    expected=cc_transfer_asset_get(2,at)|((uint16_t)cc_transfer_asset_get(2,at+1)<<8);
                    expected+=actual-CC_CODE_BASE;
                } else if(j==TRANSFER_CALL_RELOC_COUNT) {
                    at=TRANSFER_CALL_MODULE_OPERAND;expected=module+b.code_origin-CC_CODE_BASE;
                } else if(j==TRANSFER_CALL_RELOC_COUNT+1) {
                    at=TRANSFER_CALL_GATE_OPERAND;expected=actual-TRANSFER_GATE_BYTES;
                } else {
                    at=TRANSFER_CALL_FILE_OPERAND;expected=cc.file_entry+b.code_origin-CC_CODE_BASE;
                }
                if(i==at)expected_byte=(uint8_t)expected;
                if(i==at+1)expected_byte=(uint8_t)(expected>>8);
            }
            check(image_get(&b,offset+entry-CC_CODE_BASE+i)==expected_byte);
        }
        if(!repeated)one_bytes=b.code_bytes;
        else check(b.code_bytes>one_bytes&&b.code_bytes-one_bytes<256);
        build_close(&b);check(pages_used()==1);
    }
    load("procedure Chain(n:integer);begin end;begin Chain(1)end.");
    check(compile(&s,&b,&d)&&!cc.chain_entry&&!cc.file_entry);build_close(&b);
    invalid("var n:integer;begin Chain(n)end.",21);
    invalid("begin Chain('NEXT.CHN')end.",21);
    for(kind=0;kind<3;++kind) {
        char text[4096];size_t length;
        FILE *file=fopen(kind==1?"tests/native/chain-next.pas":kind==2?"tests/native/chain-cleanup.pas":"tests/native/chain-call.pas","rb");
        check(file!=NULL);length=fread(text,1,sizeof(text),file);
        check(feof(file)&&!ferror(file)&&fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        options.kind=kind==1?COMPILE_CHAIN:COMPILE_PROGRAM;options.start=0x8040;
        check(compile_project(&s,0,&b,&d,0,&options));build_close(&b);
    }
}

static void startup_cases(void) {
    extern const uint8_t transfer_start[];
    CompileOptions options={0,0xfbffu,COMPILE_PROGRAM};
    uint16_t bytes,entry,at,i;uint8_t header[160];NativeImage identity;
    static const uint16_t entries[]={0xbfff,0xc000,0xc001,0xf100};
    load("var s:string[80];begin s:='entry' end.");
    check(compile(&s,&b,&d));bytes=b.code_bytes;build_close(&b);
    for(at=0;at<sizeof(entries)/sizeof(*entries);++at) {
        options.start=entries[at]-(bytes-EXECUTE_ENTRY_BYTES);
        check(compile_project(&s,0,&b,&d,0,&options)&&b.code_bytes==bytes&&build_check(&s,&b));
        for(i=0;i<160;++i)header[i]=image_get(&b,i);
        entry=header[12]|((uint16_t)header[13]<<8);
        check(entry==entries[at]&&header[1]==header[12]&&header[2]==header[13]);
        check((header[14]|((uint16_t)header[15]<<8))==0x40a3);
        check(native_header(header,0,&identity)==0&&identity.origin==options.start);
        for(i=0;i<EXECUTE_ENTRY_BYTES;++i)
            check(image_get(&b,entry-0x4000u+i)==transfer_start[TRANSFER_MODULE_BYTES+TRANSFER_GATE_BYTES+TRANSFER_CALL_BYTES+i]);
        build_close(&b);check(pages_used()==1);
    }
    options.start=0;options.kind=COMPILE_CHAIN;
    check(compile_project(&s,0,&b,&d,0,&options)&&build_check(&s,&b));
    check(b.code_bytes==bytes-EXECUTE_ENTRY_BYTES+CC_ACTIVATION_BYTES);
    build_close(&b);check(pages_used()==1);
}

static void keyboard_corpus(void) {
    const char *names[]={"kbdread","kbdbind","kbdlife","kbdupper","kbdfail","conread","conbind","conoutput","conupper","conoutfail","confail","trmread","trmfail","devoutput","devfail"};
    char path[80],text[16384];
    unsigned i;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        FILE *file;
        size_t length;
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);
        file=fopen(path,"rb");check(file!=NULL);
        length=fread(text,1,sizeof(text),file);check(feof(file)&&!ferror(file));check(fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        check(compile(&s,&b,&d));check(cc.file_entry&&cc.temporaries==0);build_close(&b);
    }
}
/* Keep the regression's classifier at the placement where its old inline
 * four-byte result would cross BFFF/C000. Native execution checks the actual
 * output and neighboring data; this only establishes that layout precondition. */
static void console_boundary(void) {
    char text[2048];FILE *file=fopen("tests/native/ctlboundary.pas","rb");
    uint16_t at,entry=0,area=cc_runtime_private(RP_INPUT_CONSOLE_AREA);
    size_t length;
    check(file!=NULL);length=fread(text,1,sizeof(text),file);
    check(feof(file)&&!ferror(file));check(fclose(file)==0);
    check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
    check(compile(&s,&b,&d));
    for(at=b.code_origin-0x4000u;at+8<b.length;++at)
        if(image_get(&b,at)==0x11&&image_get(&b,at+1)==(uint8_t)area&&
           image_get(&b,at+2)==(uint8_t)(area>>8)&&image_get(&b,at+3)==0x0e&&
           image_get(&b,at+4)==1&&image_get(&b,at+5)==0x2a&&
           image_get(&b,at+6)==(uint8_t)RT_PC&&image_get(&b,at+7)==(uint8_t)(RT_PC>>8)) {
            check(!entry);entry=at+0x4000u-7;
        }
    if(entry!=0xbffeu)fprintf(stderr,"console classifier at %04x; adjust fixture padding by %d\n",entry,0xbffe - (int)entry);
    check(entry==0xbffeu);check((area&0x3fffu)<=0x3ffcu);
    build_close(&b);check(pages_used()==1);
}
static void interruption_corpus(void) {
    const char *names[]={"keyuraw","keyuabi","keyuframe","keyuqueue","keyucached","keyurep","keyuwhile","keyufor","keyugoto","keyufail","keyuignore"};
    char path[80],text[4096];unsigned i;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        FILE *file;size_t length;
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);
        file=fopen(path,"rb");check(file!=NULL);
        length=fread(text,1,sizeof(text),file);check(feof(file)&&!ferror(file));check(fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        check(compile(&s,&b,&d)&&cc.temporaries==0);
        check((cc.interrupt_entry!=0)==(i!=0));
        build_close(&b);check(pages_used()==1);
    }
    invalid("{$U}begin end.",93);invalid("{$U1}begin end.",93);
    invalid("{$U+ ,R+}begin end.",93);
    load("{$C-}begin end.");check(compile(&s,&b,&d));check(image_get(&b,RT_CBREAK-0x4000u)==0);build_close(&b);
    load("{$C+}{$C-}begin end.");check(compile(&s,&b,&d));check(image_get(&b,RT_CBREAK-0x4000u)==0);build_close(&b);
    load("begin {$C-}end.");check(compile(&s,&b,&d));check(image_get(&b,RT_CBREAK-0x4000u)==1);build_close(&b);
    load("var n:integer;{$C-}begin end.");check(compile(&s,&b,&d));check(image_get(&b,RT_CBREAK-0x4000u)==1);build_close(&b);
    load("{$C+}procedure p;{$C-}begin end;begin p end.");check(compile(&s,&b,&d));check(image_get(&b,RT_CBREAK-0x4000u)==1);build_close(&b);
    invalid("{$C}begin end.",93);invalid("{$C1}begin end.",93);
}
static void bridge_corpus(void) {
    const char *names[]={"bridgeframe","bridgeargs","bridgereloc","bridgefile","bridgeerror","bridgeguard"};
    char path[80],text[16384];
    unsigned i;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        FILE *file; size_t length;
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);
        file=fopen(path,"rb");check(file!=NULL);
        length=fread(text,1,sizeof(text),file);check(feof(file)&&!ferror(file));check(fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        check(compile(&s,&b,&d));check(cc.temporaries==0);
        if(!strcmp(names[i],"bridgereloc")) check(b.globals_bytes==3&&b.code_origin==0x8003);
        else check(b.globals_bytes==0&&image_get(&b,0x4003)==3&&image_get(&b,0x4004)==195);
        build_close(&b);
    }
}
static void hook_corpus(void) {
    const char *names[]={"hookout","hookmulti","hookaux","hookstatus","hookusr","hookecho","hookpure","hookerror","hookioerr"};
    char path[80],text[16384]; unsigned i;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        FILE *file; size_t length;
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);file=fopen(path,"rb");check(file!=NULL);
        length=fread(text,1,sizeof(text),file);check(feof(file)&&!ferror(file));check(fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        check(compile(&s,&b,&d));check(cc.temporaries==0);build_close(&b);
    }
}
static uint16_t map_word(uint16_t at) { return far_get(b.map_page,at)|((uint16_t)far_get(b.map_page,at+1)<<8); }
static void ordered_map(void) {
    uint16_t at=96+map_word(84),last=0,begin,end;
    for(;at<b.map_length;at+=16) {
        begin=map_word(at); end=map_word(at+2);
        check(begin>=last&&begin<end&&end<=b.code_bytes);
        check(map_word(at+8)<=map_word(at+12)&&map_word(at+12)<=s.length);
        last=end;
    }
}
static unsigned calls(uint16_t entry,int mode) {
    uint16_t i,at=b.code_origin-0x4000u;
    unsigned count=0;
    for(i=0;i+5<b.code_bytes;++i) if(image_get(&b,at+i)==0xcd&&
        image_get(&b,at+i+1)==(uint8_t)entry&&image_get(&b,at+i+2)==(uint8_t)(entry>>8)&&
        (mode<0||image_get(&b,at+i+5)==mode)) ++count;
    return count;
}
static void index_modes(void) {
    static const uint16_t strides[]={0,1,2,3,4,5,6,7,8,15,16,17,31,32,63,64,
        127,128,129,254,255,256,257,511,512,1023,1024,2047,4095,4096,8191,8192,16383};
    char text[256],element[80];
    uint16_t bytes,i;
    uint8_t mode;
    for(i=0;i<sizeof(strides)/sizeof(strides[0]);++i) {
        if(strides[i])snprintf(element,sizeof(element),"array[1..%u]of byte",strides[i]);
        else strcpy(element,"record end");
        bytes=0;
        for(mode=0;mode<2;++mode) {
            snprintf(text,sizeof(text),"{$X%c}type item=%s;var a:array[0..0]of item absolute $9000;i,p:integer;begin p:=addr(a[i])end.",mode?'-':'+',element);
            load(text);check(compile(&s,&b,&d));check(cc.index_speed==!mode);
            check(cc.temporaries==0);
            if(!mode){check(calls(RT_MUL,-1)==0);bytes=b.code_bytes;}
            else {check(b.code_bytes<=bytes);if(strides[i]==255)check(calls(RT_MUL,-1)==1);}
            build_close(&b);
        }
    }
    load("(*$x-*)type item=array[1..255]of byte;var a:array[1..2]of item;i,p:integer;begin p:=addr(a[i]);{$R+,X+}p:=addr(a[i])end.");
    check(compile(&s,&b,&d));check(calls(RT_MUL,-1)==1);check(calls(RT_INDEX,-1)==1);build_close(&b);
    load("begin end.");check(compile(&s,&b,&d));check(cc.index_speed==1);build_close(&b);
    invalid("{$X}begin end.",93);invalid("{$X*}begin end.",93);
    {
        const char *names[]={"indexmodes","indexwrap","indexrange","indexrsmall"};
        char source[4096],path[80];
        FILE *fixture;size_t length;
        for(i=0;i<4;++i) {
            snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);fixture=fopen(path,"rb");check(fixture!=NULL);
            length=fread(source,1,sizeof(source),fixture);check(feof(fixture)&&!ferror(fixture));check(fclose(fixture)==0);
            check(source_import(&s,(const uint8_t*)source,(uint16_t)length,&d));
            check(compile(&s,&b,&d));check(cc.temporaries==0);build_close(&b);
        }
    }
}
static void source_catalog_cases(void) {
    Sources set,stale,next;Source origin,snapshot,view,other;
    uint8_t before=pages_used(),old,hash[32],expected[32];
    check(source_new(&origin,"H:/origin.pas"));
    check(source_import(&origin,(const uint8_t*)"begin end.",10,&d));
    check(sources_new(&set));check(source_snapshot(&origin,&snapshot));
    check(sources_take(&set,&snapshot,&origin)==0);check(set.count==1);
    check(sources_ready(&set,0)&&sources_resident(&set,0));
    check(sources_find(&set,"H:/origin.pas")==0&&sources_find(&set,"H:/ORIGIN.PAS")==-1);
    check(sources_get(&set,0,&view)&&view.page==snapshot.page&&view.length==10);
    source_hash(&origin,expected);check(sources_hash(&set,0,hash)&&!memcmp(hash,expected,32));
    check(sources_byte(&set,0,0)=='b'&&sources_byte(&set,0,10)==0);
    old=far_get(origin.page,0);far_set(origin.page,0,'B');check(!sources_ready(&set,0));
    far_set(origin.page,0,old);check(!sources_ready(&set,0));
    check(sources_take(&set,&snapshot,&origin)==0&&sources_ready(&set,0));
    fail_drop=snapshot.page;check(!sources_remove(&set,0)&&set.count==1);
    check(!sources_close(&set)&&set.count==1&&page_role(snapshot.page)==PAGE_SNAPSHOT);fail_drop=-1;
    check(source_new(&other,"H:/second.pas"));
    check(source_import(&other,(const uint8_t*)"var n:integer;",14,&d));
    check(sources_take(&set,&other,&other)==1);
    strcpy(view.path,"H:/second.pas");fail_drop=other.page;
    check(sources_take(&set,&view,&origin)<0&&set.count==2&&sources_ready(&set,1));
    fail_drop=-1;check(sources_take(&set,&view,&origin)==0&&set.count==1);
    check(!page_role(other.page)&&sources_find(&set,"H:/origin.pas")==-1);
    strcpy(view.path,"H:/renamed.pas");check(sources_take(&set,&view,&origin)==0&&set.count==1);
    check(sources_find(&set,"H:/origin.pas")==-1&&sources_find(&set,"H:/renamed.pas")==0);
    check(source_new(&other,"H:/second.pas"));check(sources_take(&set,&other,&other)==1);
    view=other;strcpy(view.path,"H:/renamed.pas");fail_drop=snapshot.page;
    check(sources_take(&set,&view,&view)<0&&set.count==2&&sources_ready(&set,0));
    fail_drop=-1;check(sources_take(&set,&view,&view)==0&&set.count==1);
    check(sources_get(&set,0,&view)&&view.page==other.page&&!page_role(snapshot.page));
    stale=set;check(sources_close(&set)&&set.page==255);check(!sources_valid(&stale));
    check(sources_new(&next));check(next.page==stale.page&&!sources_valid(&stale));check(sources_close(&next));
    source_close(&origin);check(pages_used()==before);
}
static void replaced_source_cases(void) {
    Sources set;Source work,view,other;
    uint8_t before=pages_used(),prior,foreign,scenario;
    uint32_t lifetime;
    for(scenario=0;scenario<3;++scenario) {
        check(source_new(&work,"H:/edited.pas"));
        check(source_import(&work,(const uint8_t*)"abc",3,&d));
        check(sources_new(&set)&&sources_take(&set,&work,&work)==0);
        prior=work.page;
        if(scenario==1) check(source_move(&work,0,1,3,0));
        else check(source_insert_from(&work,3,&work,0,1,0));
        check(work.page!=prior&&!page_role(prior)&&!sources_ready(&set,0));
        foreign=255;
        if(scenario==2) {
            foreign=page_new(PAGE_COMPILER);check(foreign==prior);
            lifetime=page_lifetime(foreign);far_set(foreign,0,0xa5);
            check(sources_byte(&set,0,0)==0); /* never expose the new owner */
        }
        check(sources_take(&set,&work,&work)==0&&sources_ready(&set,0));
        check(sources_get(&set,0,&view)&&view.page==work.page&&view.length==work.length);
        check(sources_byte(&set,0,0)==(scenario==1?'b':'a'));
        check(sources_close(&set));
        if(foreign!=255) {
            check(page_role(foreign)==PAGE_COMPILER&&page_lifetime(foreign)==lifetime&&far_get(foreign,0)==0xa5);
            check(page_drop(foreign));
        }
        check(pages_used()==before);
    }
    /* A reused page number cannot rename an unrelated stale catalog record. */
    check(source_new(&work,"H:/stale.pas"));check(sources_new(&set));
    check(sources_take(&set,&work,&work)==0);prior=work.page;source_close(&work);
    check(source_new(&other,"H:/fresh.pas")&&other.page==prior);
    check(sources_take(&set,&other,&other)==1&&set.count==2);
    check(sources_remove(&set,0)&&sources_ready(&set,0));
    check(sources_find(&set,"H:/fresh.pas")==0&&sources_find(&set,"H:/stale.pas")<0);
    check(sources_close(&set)&&pages_used()==before);

    /* Reopening the same path on the same page is a new allocation too. */
    check(source_new(&work,"H:/reopened.pas"));check(sources_new(&set));
    check(sources_take(&set,&work,&work)==0);prior=work.page;source_close(&work);
    check(source_new(&other,"H:/reopened.pas")&&other.page==prior);
    lifetime=page_lifetime(other.page);
    check(sources_take(&set,&other,&other)==0&&set.count==1&&sources_ready(&set,0));
    check(page_lifetime(other.page)==lifetime&&sources_close(&set)&&pages_used()==before);

    /* Packed metadata stays authenticated while a stale include is removed. */
    check(source_new(&work,"H:/packed.pas"));check(source_snapshot(&work,&view));
    check(source_new(&other,"H:/packed.inc"));check(sources_new(&set));
    check(sources_take(&set,&view,&work)==0&&sources_take(&set,&other,0)==1);
    check(sources_pack(&set)&&set.base&&sources_valid(&set));
    prior=other.page;source_close(&other);foreign=page_new(PAGE_RUN);check(foreign==prior);
    lifetime=page_lifetime(foreign);
    check(!sources_ready(&set,1)&&sources_remove(&set,1)&&sources_ready(&set,0));
    check(sources_close(&set)&&page_lifetime(foreign)==lifetime&&page_drop(foreign));
    source_close(&work);check(pages_used()==before);

    /* Removal/close may forget a released record, but never free its reuse. */
    for(scenario=0;scenario<2;++scenario) {
        check(source_new(&work,"H:/removed.pas"));check(sources_new(&set));
        check(sources_take(&set,&work,&work)==0);prior=work.page;source_close(&work);
        foreign=255;
        if(scenario) {foreign=page_new(PAGE_RUN);check(foreign==prior);lifetime=page_lifetime(foreign);}
        check(sources_remove(&set,0)&&set.count==0&&sources_close(&set));
        if(foreign!=255) {check(page_role(foreign)==PAGE_RUN&&page_lifetime(foreign)==lifetime);check(page_drop(foreign));}
        check(pages_used()==before);
    }
    /* Save As also authenticates the destination owner's lifetime. */
    check(source_new(&work,"H:/destination.pas"));check(sources_new(&set));
    check(sources_take(&set,&work,&work)==0);prior=work.page;source_close(&work);
    foreign=page_new(PAGE_RUN);check(foreign==prior);lifetime=page_lifetime(foreign);
    check(source_new(&other,"H:/renamed.pas"));check(sources_take(&set,&other,&other)==1);
    view=other;strcpy(view.path,"H:/destination.pas");
    check(sources_take(&set,&view,&view)==0&&set.count==1&&sources_ready(&set,0));
    check(page_role(foreign)==PAGE_RUN&&page_lifetime(foreign)==lifetime);
    check(sources_close(&set)&&page_drop(foreign)&&pages_used()==before);
}
static void packed_catalog_cases(void) {
    static uint8_t text[PAGE_SIZE];
    const uint16_t lengths[]={0,10,PAGE_SIZE-190,PAGE_SIZE-189,SOURCE_LIMIT};
    Sources set,stale;Source origin,snapshot,include,other,view;
    uint8_t before=pages_used(),metadata,hash[32],expected[32],packed;
    uint16_t i,n;unsigned scenario;
    for(i=0;i<PAGE_SIZE;++i)text[i]=i%80==79?'\n':'x';
    for(scenario=0;scenario<sizeof(lengths)/sizeof(lengths[0]);++scenario) {
        n=lengths[scenario];
        check(source_new(&origin,"H:/root.pas"));
        check(source_import(&origin,text,n,&d)&&origin.length==n);
        check(source_snapshot(&origin,&snapshot)&&snapshot.length==n);
        check(sources_new(&set));check(sources_take(&set,&snapshot,&origin)==0);
        metadata=set.page;stale=set;source_hash(&snapshot,expected);
        check(sources_pack(&set));packed=n<PAGE_SIZE-189;
        check((set.base!=0)==packed&&sources_valid(&set)&&sources_ready(&set,0));
        check(pages_used()==before+3-packed);
        check(sources_get(&set,0,&view)&&view.page==snapshot.page&&view.length==n);
        source_hash(&view,hash);check(!memcmp(hash,expected,32));
        check(sources_hash(&set,0,hash)&&!memcmp(hash,expected,32));
        check(sources_find(&set,"H:/root.pas")==0&&sources_origin(&set,0)==origin.page);
        check(sources_byte(&set,0,n)==0);
        if(packed) {
            check(set.page==snapshot.page&&set.base==n+1&&!page_role(metadata));
            check(!sources_valid(&stale));check(sources_pack(&set));
            fail_drop=snapshot.page;check(!sources_close(&set)&&set.count==1&&sources_valid(&set));
            fail_drop=-1;
            stale=set;far_set(set.page,n?0:set.base,far_get(set.page,n?0:set.base));
            check(!sources_valid(&set)&&!sources_ready(&set,0)&&!sources_close(&set));
            /* Restore the test observer only, to retire the deliberately
             * corrupted owner without weakening production authentication. */
            set.version=page_versions[set.page];
            check(!sources_valid(&stale));
        }
        if(n==PAGE_SIZE-190) {
            check(source_snapshot(&origin,&other));strcpy(other.path,"H:/grow.inc");
            fail_after=0;check(sources_take(&set,&other,0)<0);
            check(set.base&&set.count==1&&sources_ready(&set,0)&&page_role(other.page));
            fail_after=-1;check(sources_take(&set,&other,0)==1&&!set.base);
            check(sources_ready(&set,0)&&sources_ready(&set,1));
            check(sources_pack(&set)&&!set.base); /* Two records cannot fit. */
            check(sources_get(&set,0,&view));source_hash(&view,hash);check(!memcmp(hash,expected,32));
        }
        check(sources_close(&set));source_close(&origin);check(pages_used()==before);
    }
    check(source_new(&origin,"H:/root.pas"));
    check(source_import(&origin,(const uint8_t*)"begin end.",10,&d));
    check(source_snapshot(&origin,&snapshot));check(sources_new(&set));
    check(sources_take(&set,&snapshot,&origin)==0);
    check(source_snapshot(&origin,&include));strcpy(include.path,"H:/part.inc");
    check(sources_take(&set,&include,0)==1);
    check(source_snapshot(&origin,&other));strcpy(other.path,"H:/other.inc");
    check(sources_take(&set,&other,0)==2);metadata=set.page;
    fail_drop=metadata;check(!sources_pack(&set));
    check(set.page==metadata&&!set.base&&sources_valid(&set));
    check(sources_ready(&set,0)&&sources_ready(&set,1)&&pages_used()==before+5);
    fail_drop=-1;check(sources_pack(&set)&&pages_used()==before+4);
    check(sources_ready(&set,0)&&sources_ready(&set,1)&&!sources_resident(&set,1));
    check(!sources_remove(&set,0)&&set.count==3);
    check(sources_remove(&set,1)&&set.count==2&&sources_ready(&set,0)&&sources_ready(&set,1));
    check(sources_get(&set,1,&view)&&view.page==other.page&&sources_find(&set,"H:/other.inc")==1);
    fail_drop=other.page;check(!sources_close(&set)&&set.count==2&&sources_ready(&set,0));
    fail_drop=-1;check(sources_remove(&set,1)&&set.count==1&&sources_ready(&set,0));
    check(!page_role(include.page)&&sources_find(&set,"H:/part.inc")==-1);
    fail_after=0;check(sources_take(&set,&snapshot,&origin)<0&&set.base&&sources_ready(&set,0));
    fail_after=-1;check(sources_take(&set,&snapshot,&origin)==0&&!set.base&&sources_ready(&set,0));
    check(sources_pack(&set)&&set.base&&sources_ready(&set,0));
    check(source_snapshot(&origin,&other));fail_drop=snapshot.page;
    check(sources_take(&set,&other,&origin)<0&&!set.base&&sources_ready(&set,0));
    check(page_role(other.page)==PAGE_SNAPSHOT&&page_role(snapshot.page)==PAGE_SNAPSHOT);
    fail_drop=-1;check(sources_take(&set,&other,&origin)==0&&!page_role(snapshot.page));
    check(sources_get(&set,0,&view)&&view.page==other.page&&sources_ready(&set,0));
    check(sources_close(&set));source_close(&origin);check(pages_used()==before);
}
static void include_cases(void) {
    HostSource files[]={
        {"H:/PART.INC","const greeting='included';number=-1.25;\ntype link=^node;node=record value:integer;next:link end;\nconst table:array[1..2]of integer=(1,2);\nvar n:integer;\nprocedure part;begin n:=table[1];writeln(greeting,number)end;"},
        {"H:/more.inc","procedure more;begin part;n:=n+table[2]end;"},
        {"H:/values.inc","11,22"},
        {"H:/arg.inc","Input"},
        {"H:/bad.inc","const broken=32768;"},
        {"H:/scope.PAS","{$A-,I-,R+,V-,W0,X-,B-,U+}const local=1;"},
        {"H:/nested.inc","{$I values.inc}"},
        {"H:/runtime.inc","n:=7 div 0;"}
    };
    uint8_t i,file,found=0,digest[32];uint16_t at,n,position;unsigned reads;Sha256 hash;
    Sources buffers;Source root,override,snapshot;
    disk_sources=files;disk_source_count=sizeof(files)/sizeof(files[0]);disk_reads=0;
    files[2].text="const across:array[1..2]of integer=(11,";
    files[1].text="const later=1;";
    load("{$I values.inc}\n22);\n{$I more.inc}\nbegin writeln(across[1],across[2])end.");
    check(compile(&s,&b,&d));build_close(&b);
    files[2].text="11,22";
    files[1].text="procedure more;begin part;n:=n+table[2]end;";
    source_catalog_cases();
    replaced_source_cases();
    packed_catalog_cases();
    load("program Main;\n{$IPART.INC}\n{$I more.inc}\nbegin more;writeln(greeting,number,table[2])end.");
    check(compile(&s,&b,&d));check(b.sources.count==3);check(map_word(8)==1);check(map_word(14)==3);
    check(sources_find(&b.sources,"H:/PART.INC")==1&&sources_find(&b.sources,"H:/more.inc")==2);
    at=96;
    for(i=0;i<3;++i){check(map_word(at)==i);n=map_word(at+2);check(n&&n<=127);at+=40+n;}
    check(at==96+map_word(84));
    for(at=96+map_word(84);at<b.map_length;at+=16) {
        n=map_word(at+4);check(n<3);found|=(uint8_t)(1u<<n);
        check(build_location(&s,&b,map_word(at),&file,&position)&&file==n&&position==map_word(at+8));
        if(n)check(!build_locate(&s,&b,map_word(at),&position));
    }
    check(found==7&&build_ready(&s,&b));reads=disk_reads;
    files[1].text="procedure more;begin n:=99 end;";check(!build_ready(&s,&b)&&disk_reads>reads);
    files[1].text="procedure more;begin part;n:=n+table[2]end;";check(build_check(&s,&b));
    disk_source_count=0;check(!build_ready(&s,&b));disk_source_count=sizeof(files)/sizeof(files[0]);
    build_close(&b);check(pages_used()==1);
    load("{$R-,X+}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d));
    check(cc.absolute_mode&&cc.io_check&&!cc.range_check&&cc.var_strict&&cc.with_limit==4&&cc.index_speed&&!cc.input_fresh&&!cc.user_interrupt);
    build_close(&b);
    load("{$U+}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d)&&cc.user_interrupt&&cc.interrupt_entry);build_close(&b);
    files[5].text="{$U-}const local=1;";
    load("{$U+}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d)&&cc.user_interrupt&&cc.interrupt_entry);build_close(&b);
    load("{$U+}begin {$U-}end.");check(compile(&s,&b,&d)&&!cc.user_interrupt&&cc.interrupt_entry);build_close(&b);
    files[5].text="{$C-}";
    load("{$C+}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d));
    check(cc.console_control&&image_get(&b,RT_CBREAK-0x4000u)==1);build_close(&b);
    files[5].text="{$C-}var local:integer;";
    load("{$C+}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d));
    check(cc.console_control&&image_get(&b,RT_CBREAK-0x4000u)==1);build_close(&b);
    files[5].text="{$C+}";
    load("{$C-}\n{$I scope}\nbegin end.");check(compile(&s,&b,&d));
    check(!cc.console_control&&image_get(&b,RT_CBREAK-0x4000u)==0);build_close(&b);
    load("{$C+}\n{$I scope}\n{$C-}begin end.");check(compile(&s,&b,&d));
    check(!cc.console_control&&image_get(&b,RT_CBREAK-0x4000u)==0);build_close(&b);
    load("const data:array[1..2]of integer=(\n{$I values.inc}\n);\nbegin writeln(data[1])end.");
    check(compile(&s,&b,&d)&&b.sources.count==2);build_close(&b);
    load("var n:integer;begin read(\n{$I arg.inc}\n,n)end.");
    check(compile(&s,&b,&d)&&b.sources.count==2&&cc.temporaries==0);build_close(&b);
    load("program P;\n{$I bad.inc}\nbegin end.");check(!compile(&s,&b,&d)&&d.code==56);
    check(!strcmp(compile_error_path(),"H:/bad.inc")&&d.start==13&&pages_used()==1);
    sha_init(&hash);for(at=0;files[4].text[at];++at)sha_byte(&hash,(uint8_t)files[4].text[at]);sha_end(&hash,digest);
    check(compile_error_hash()&&!memcmp(compile_error_hash(),digest,32));
    load("{$I missing}\nbegin end.");check(!compile(&s,&b,&d)&&d.code==90);
    check(!strcmp(compile_error_path(),"H:/missing.PAS")&&!compile_error_hash()&&pages_used()==1);
    invalid("{$I nested.inc}\nbegin end.",93);check(!strcmp(compile_error_path(),"H:/nested.inc"));
    invalid("{$I SLICE.PAS}\nbegin end.",93);
    invalid("begin {$I values.inc}end.",93);
    check(source_new(&root,"H:/project/main.pas"));
    check(source_import(&root,(const uint8_t*)"{$I ../unsaved.inc}\nbegin writeln(answer)end.",45,&d));
    check(source_new(&override,"H:/unsaved.inc"));
    check(source_import(&override,(const uint8_t*)"const answer=42;",16,&d));
    check(sources_new(&buffers));check(sources_take(&buffers,&root,&root)==0);
    check(sources_take(&buffers,&override,&override)==1);reads=disk_reads;
    check(compile_project(&root,&buffers,&b,&d,0,0));check(b.sources.count==2&&disk_reads==reads);
    check(build_ready(&root,&b));check(sources_get(&b.sources,1,&snapshot));
    check(snapshot.page!=override.page&&snapshot.length==override.length);
    check(source_replace(&override,override.length,override.length,(const uint8_t*)" ",1));
    check(!build_ready(&root,&b));build_close(&b);
    check(sources_take(&buffers,&override,&override)==1);
    check(compile_project(&root,&buffers,&b,&d,0,0)&&build_ready(&root,&b));build_close(&b);
    check(sources_close(&buffers)&&pages_used()==1);
    disk_sources=0;disk_source_count=0;
}
static uint16_t inline_word(uint16_t at) { return image_get(&b,at)|((uint16_t)image_get(&b,at+1)<<8); }
static uint16_t inline_marker(void) {
    uint16_t at,found=0;
    for(at=0x4000;at+4<b.length;++at)
        if(image_get(&b,at)==0xde&&image_get(&b,at+1)==0xad&&image_get(&b,at+2)==0xbe&&image_get(&b,at+3)==0xef) {
            check(!found);found=at+4;
        }
    check(found!=0);return found;
}
static void inline_corpus(void) {
    const char *names[]={"inlbytes","inladdr","inlframe","inlstd","inlerror"};
    char text[16384],path[128];unsigned i;FILE *file;size_t length;
    for(i=0;i<sizeof(names)/sizeof(names[0]);++i) {
        snprintf(path,sizeof(path),"tests/native/%s.pas",names[i]);file=fopen(path,"rb");check(file!=NULL);
        length=fread(text,1,sizeof(text),file);check(feof(file)&&!ferror(file));check(fclose(file)==0);
        check(source_import(&s,(const uint8_t*)text,(uint16_t)length,&d));
        check(compile(&s,&b,&d));check(cc.temporaries==0);build_close(&b);
    }
    {
        HostSource part={"H:/source-parts/inlnops.inc",text};
        char root[1024];
        file=fopen("tests/native/source-parts/inlnops.inc","rb");check(file!=NULL);
        length=fread(text,1,sizeof(text)-1,file);text[length]=0;check(feof(file)&&!ferror(file));check(fclose(file)==0);
        file=fopen("tests/native/inlpages.pas","rb");check(file!=NULL);
        length=fread(root,1,sizeof(root)-1,file);root[length]=0;check(feof(file)&&!ferror(file));check(fclose(file)==0);
        disk_sources=&part;disk_source_count=1;
        load(root);check(compile(&s,&b,&d));check(b.code_bytes>0x4000&&b.code_origin>GLOBAL_BASE);
        check(build_check(&s,&b)&&cc.stage_page==255&&b.sources.count==2);build_close(&b);
        disk_sources=NULL;disk_source_count=0;
    }
}
static void inline_cases(void) {
    static const uint8_t constants[]={0,255,0,1,255,255,255,255,0,1,0x34,0x44,0,0,0,0,0x80,0,0x80};
    const char *bad[]={"", "'A'", "1.25", "true", "Integer", "WriteLn", "--1", "(1)", "1*2", "missing", "1/", "65535", "+32768", "-32769", "$10000", "<$10000", "1 div 2", "^A", "+'A'", "nil"};
    const uint8_t errors[]={74,22,22,22,74,74,25,74,5,74,74,56,56,56,56,56,5,22,25,74};
    uint16_t at,i,j,typed=0,routine=0,delta,value;int coefficient;char text[1024],expression[160];
    load("var pad:array[1..301]of byte;begin\ninline($DE/$AD/$BE/$EF/0/255/256/-1/$FFFF/1+255/<$1234/>$44/1-1/$FFFF+1/32767+1/-32768/$CA/$FE/$BA/$BE)end.");
    check(compile(&s,&b,&d));at=inline_marker();
    for(i=0;i<sizeof(constants);++i)check(image_get(&b,at+i)==constants[i]);
    check(image_get(&b,at+i)==0xca&&image_get(&b,at+i+3)==0xbe&&build_check(&s,&b));build_close(&b);
    load("var g:array[1..301]of byte;const t:integer=17;\nprocedure p;begin end;procedure ext;external $2345;\nbegin inline($DE/$AD/$BE/$EF/g/g-g/t/t-t/p/p-p/ext/0-*/*+*/0-*-*/*+*-*/<*/<0-*/>1/$CA/$FE/$BA/$BE)end.");
    check(compile(&s,&b,&d));at=inline_marker();delta=b.code_origin-CC_CODE_BASE;
    for(i=0;i<cc.symbol_count;++i) {
        if(cc.symbols[i].role==S_TYPED)typed=cc.symbols[i].address+delta;
        if(cc.symbols[i].role==S_PROCEDURE&&!routine)routine=cc.symbols[i].address+delta;
    }
    check(typed&&routine);check(inline_word(at)==GLOBAL_BASE&&inline_word(at+2)==0);at+=4;
    check(inline_word(at)==typed&&inline_word(at+2)==0);at+=4;
    check(inline_word(at)==routine&&inline_word(at+2)==0&&inline_word(at+4)==0x2345);at+=6;
    check(inline_word(at)==(uint16_t)(0u-(at+0x4000u)));at+=2;
    check(inline_word(at)==(uint16_t)(2u*(at+0x4000u)));at+=2;
    check(inline_word(at)==(uint16_t)(0u-2u*(at+0x4000u)));at+=2;
    check(inline_word(at)==at+0x4000u);at+=2;
    check(image_get(&b,at)==(uint8_t)(at+0x4000u));++at;
    check(image_get(&b,at)==(uint8_t)(0u-(at+0x4000u)));++at;
    check(inline_word(at)==1&&image_get(&b,at+2)==0xca);check(build_check(&s,&b));build_close(&b);
    load("begin inline($DE/$AD/$BE/$EF/1+-1/1--1/+$FFFF/-$FFFF/-$8000/$CA/$FE/$BA/$BE)end.");
    check(compile(&s,&b,&d));at=inline_marker();
    check(image_get(&b,at)==0&&image_get(&b,at+1)==2&&inline_word(at+2)==65535);
    check(image_get(&b,at+4)==1&&inline_word(at+5)==0x8000&&image_get(&b,at+7)==0xca);build_close(&b);
    for(coefficient=-20;coefficient<=20;++coefficient)for(j=0;j<2;++j) {
        strcpy(expression,"0");
        for(i=0;i<(uint16_t)(coefficient<0?-coefficient:coefficient);++i)strcat(expression,coefficient<0?"-*":"+*");
        snprintf(text,sizeof(text),"var pad:array[1..511]of byte;begin inline($DE/$AD/$BE/$EF/%c%s/$CA/$FE/$BA/$BE)end.",j?'<':'>',expression);
        load(text);check(compile(&s,&b,&d));at=inline_marker();value=(uint16_t)((uint32_t)(uint16_t)coefficient*(at+0x4000u));
        check(j?image_get(&b,at)==(uint8_t)value:inline_word(at)==value);
        check(image_get(&b,at+(j?1:2))==0xca&&build_check(&s,&b));build_close(&b);
    }
    for(i=0;i<sizeof(errors);++i) {
        snprintf(text,sizeof(text),"begin inline(%s)end.",bad[i]);invalid(text,errors[i]);
    }
    invalid("var n:integer;begin inline(-n)end.",25);
    invalid("type t=(one,two);begin inline(one)end.",22);
    invalid("var r:record n:integer end;begin with r do inline(n)end.",74);
    invalid("procedure p;forward;procedure p;external $1234;begin end.",12);
    invalid("begin inline end.",4);
    {
        char large[10000];uint16_t n=0;
        strcpy(large,"begin inline(\n");n=(uint16_t)strlen(large);
        for(i=0;i<4200;++i) {
            large[n++]='*';if(i!=4199)large[n++]='/';
            if(i%32==31)large[n++]='\n';
        }
        strcpy(large+n,")end.");invalid(large,99);
    }
    check(pages_used()==1);
}
int main(int argc,char **argv) {
    unsigned i;
    uint16_t position;
    uint8_t old;
    FILE *file;
    check(argc==2);
    check(source_new(&s,"H:/SLICE.PAS"));
    keyboard_corpus();
    interruption_corpus();
    compile_cancellation();
    compilation_options();
    chain_cases();
    chain_call_cases();
    startup_cases();
    console_boundary();
    bridge_corpus();
    hook_corpus();
    index_modes();
    include_cases();
    inline_cases();
    inline_corpus();
    load("begin  \r\nend. \r\n\032padding"); check(s.length==11);
    check(source_replace(&s,0,0,NULL,0)&&!s.dirty);
    {
        const uint8_t embedded[]="begin write('a\0b') end.";
        check(source_import(&s,embedded,sizeof(embedded)-1,&d));
        check(compile(&s,&b,&d)); build_close(&b);
    }
    invalid("var r:real;begin r:=true end.",44);
    invalid("begin Input:=Output end.",54);
    invalid("begin read(Output,true)end.",41);
    invalid("var i:integer;begin rename(i,'x')end.",21);
    invalid("var f:text;begin rename(f)end.",3);
    invalid("var f,g:text;\nbegin f:=g end.",54);
    invalid("var f:text;\nbegin f:=1 end.",54);
    invalid("var f:text;\nbegin writeln(1+f)end.",54);
    invalid("type f=file;\nprocedure p(a:f);\nbegin end;\nbegin end.",67);
    invalid("function f:text;\nbegin end;\nbegin end.",67);
    invalid("type a=file;\nf=file of a;\nbegin end.",68);
    invalid("const f:text='X';\nbegin end.",61);
    invalid("type r=record f:text end;\nconst a:r=(f:'X');\nbegin end.",61);
    invalid("type a=file of integer;\nb=file of real;\nvar f:b;\nprocedure p(var g:a);\nbegin end;\nbegin p(f)end.",44);
    invalid("type a=file of integer;\nb=file of byte;\nvar f:b;\nprocedure p(var g:a);\nbegin end;\nbegin p(f)end.",44);
    invalid("type a=file of integer;\nb=file;\nvar f:b;\nprocedure p(var g:a);\nbegin end;\nbegin p(f)end.",44);
    invalid("type b=file;\nvar f:text;\nprocedure p(var g:b);\nbegin end;\nbegin p(f)end.",44);
    invalid("type a=file of integer;\nc=array[1..2]of byte;\nb=file of c;\nvar f:b;\nprocedure p(var g:a);\nbegin end;\nbegin p(f)end.",44);
    invalid("type a=file of string[10];\nb=file of string[10];\nvar f:b;\nprocedure p(var g:a);\nbegin end;\nbegin p(f)end.",44);
    load("type plain=file;\nints=file of integer;\nother=file of integer;\nz=record end;\nzero=file of z;\nvar a:text;\nb:plain;\nc:ints;\nd:other;\ne:zero;\nprocedure p(var f:ints);\nbegin end;\nbegin p(d);\nwriteln(sizeof(text),sizeof(a),sizeof(b),sizeof(c),sizeof(e))end.");
    check(compile(&s,&b,&d)); check(b.globals_bytes==82);
    check(cc.text_type!=255&&cc.types[cc.text_type].family==K_FILE&&cc.types[cc.text_type].width==16);
    check(cc.types[cc.text_type].capacity==FILE_TEXT); build_close(&b);
    load("type r=record f:text end;\nar=array[1..2]of r;\nff=file of ar;\nvar a,b:r;\nc:ar;\nprocedure p(x:ar);\nbegin end;\nbegin a:=b;\np(c);\nwriteln(sizeof(ff))end.");
    check(compile(&s,&b,&d)); check(b.globals_bytes==96); build_close(&b);
    load("var Input,Output:integer;begin Input:=7;Output:=Input;if Output<>7 then halt end.");
    check(compile(&s,&b,&d)); check(b.globals_bytes==4); build_close(&b);
    load("var kbd:integer;begin kbd:=7;writeln(kbd)end.");
    check(compile(&s,&b,&d));check(b.globals_bytes==2&&!cc.file_entry);build_close(&b);
    load("var c:char;procedure p(var f:text);begin read(f,c)end;begin p(Kbd)end.");
    check(compile(&s,&b,&d));check(cc.file_entry&&cc.temporaries==0);build_close(&b);
    load("var c:char;procedure p(var f:text);begin read(f,c)end;begin p(Con);p(Trm);write(Con,c);write(Trm,c)end.");
    check(compile(&s,&b,&d));check(cc.file_entry&&cc.temporaries==0);build_close(&b);
    load("var Con,Trm:integer;begin Con:=1;Trm:=Con end.");
    check(compile(&s,&b,&d));check(b.globals_bytes==4&&!cc.file_entry);build_close(&b);
    load("var Lst,Aux:integer;begin Lst:=1;Aux:=Lst end.");
    check(compile(&s,&b,&d));check(b.globals_bytes==4&&!cc.file_entry);build_close(&b);
    load("procedure p(var f:text);begin writeln(f,'x')end;begin p(Lst);p(Aux)end.");
    check(compile(&s,&b,&d));check(cc.file_entry&&cc.temporaries==0);build_close(&b);
    load("type text=integer;\nvar n:text;\nbegin n:=7;\nwriteln(n)end.");
    check(compile(&s,&b,&d)); check(cc.text_type==255); build_close(&b);
    load("type ints=file of integer;raw=file;z=record end;zero=file of z;\nblock=array[1..300]of byte;wide=file of block;\nvar t:text;f:ints;u:raw;e:zero;w:wide;begin end.");
    check(compile(&s,&b,&d));
    for(i=0;i<5;++i) {
        unsigned j;
        const uint8_t family[]={FILE_TEXT,FILE_TYPED,FILE_UNTYPED,FILE_TYPED,FILE_TYPED};
        const uint16_t width[]={0,2,128,0,300};
        for(j=0;j<16;++j) {
            uint8_t expected=j==0?0x5a:j==1?0x46:j==8?family[i]:j==10?(uint8_t)width[i]:j==11?(uint8_t)(width[i]>>8):0;
            check(image_get(&b,0x4000u+i*16+j)==expected);
        }
    }
    check(cc.types[cc.text_type].high==0); build_close(&b);
    load("var p:^text;begin getmem(p,16)end.");
    check(compile(&s,&b,&d)); check(cc.types[cc.text_type].high==0); build_close(&b);
    load("var p:^text;begin new(p)end.");
    check(compile(&s,&b,&d)); check(cc.types[cc.text_type].high!=0); build_close(&b);
    invalid("var n:integer;begin n:=1/2 end.",44);
    invalid("var r:real;begin r:=1e38 end.",57);
    invalid("var r:real;begin r:=1e-39 end.",57);
    invalid("var r:real;begin r:=1e end.",57);
    invalid("var r:real;begin r:=1e001 end.",1);
    invalid("var r:real;begin r:=1.0e38 end.",57);
    invalid("type t=1.0..2.0;begin end.",57);
    invalid("begin writeln(real(1))end.",30);
    invalid("begin writeln(integer(1.5))end.",31);
    invalid("begin writeln(ord(1.0))end.",31);
    invalid("begin writeln(sqrt(true))end.",23);
    invalid("begin writeln(sqrt('x'))end.",23);
    invalid("begin writeln(sqrt('xx'))end.",23);
    invalid("begin writeln(ln(true))end.",23);
    invalid("begin writeln(exp('x'))end.",23);
    invalid("begin writeln(sin(true))end.",23);
    invalid("begin writeln(cos('x'))end.",23);
    invalid("begin writeln(arctan('xx'))end.",23);
    invalid("begin writeln(random())end.",41);
    invalid("begin writeln(random(1.5))end.",23);
    invalid("begin writeln(random(true))end.",23);
    invalid("begin writeln(random('A'))end.",23);
    invalid("type t=(a,b);begin writeln(random(a))end.",23);
    invalid("begin writeln(random(1,2))end.",5);
    invalid("begin randomize()end.",1);
    invalid("begin writeln(paramcount())end.",5);
    invalid("begin writeln(paramstr)end.",4);
    invalid("begin writeln(paramstr())end.",54);
    invalid("begin writeln(paramstr(1,2))end.",5);
    invalid("begin writeln(paramstr(1.5))end.",23);
    invalid("begin writeln(paramstr(true))end.",23);
    invalid("begin writeln(paramstr('A'))end.",23);
    invalid("begin writeln(paramstr('text'))end.",23);
    invalid("type t=(a,b);begin writeln(paramstr(a))end.",23);
    load("var a:array[0..1]of byte;begin writeln(sizeof(a[paramcount+length(paramstr(1))]))end.");
    check(compile(&s,&b,&d));check(!cc.parameter_entry);build_close(&b);
    load("var paramcount:integer;\nfunction paramstr(n:integer):integer;begin paramstr:=n end;\nbegin paramcount:=7;writeln(paramstr(paramcount))end.");
    check(compile(&s,&b,&d));check(!cc.parameter_entry);build_close(&b);
    load("begin writeln(paramcount,paramstr(1))end.");
    check(compile(&s,&b,&d));check(cc.parameter_entry&&cc.temporaries==0);ordered_map();build_close(&b);
    load("begin end.");check(compile(&s,&b,&d));check(!cc.parameter_entry);build_close(&b);
    load("var f:file of integer;a:array[0..1]of integer;begin writeln(sizeof(a[filepos(f)]))end.");
    check(compile(&s,&b,&d));check(!cc.file_entry&&cc.temporaries==0);build_close(&b);
    load("var f:integer;function filepos(n:integer):integer;begin filepos:=n end;\nbegin f:=7;writeln(filepos(f))end.");
    check(compile(&s,&b,&d));check(!cc.file_entry);build_close(&b);
    invalid("var f:file of integer;begin assign(f,1)end.",33);
    invalid("var f:integer;begin reset(f)end.",21);
    invalid("begin writeln(eof())end.",21);
    invalid("begin writeln(eoln(1))end.",21);
    invalid("var f:file of integer;begin writeln(seekeof(f))end.",66);
    invalid("begin writeln(filepos)end.",4);
    invalid("begin writeln(keypressed())end.",5);
    load("var a:array[0..1]of integer;begin writeln(sizeof(a[ord(keypressed)]))end.");
    check(compile(&s,&b,&d));check(cc.keyboard_entry&&!cc.file_entry&&cc.hook_character);build_close(&b);
    load("function keypressed:boolean;begin keypressed:=true end;begin writeln(keypressed)end.");
    check(compile(&s,&b,&d));check(cc.keyboard_entry&&!cc.file_entry&&cc.hook_character);build_close(&b);
    load("begin writeln(keypressed)end.");
    check(compile(&s,&b,&d));check(cc.keyboard_entry&&cc.keyboard_poll&&!cc.file_entry&&cc.temporaries==0);
    {
        uint16_t at=cc_runtime_private(RP_INPUT_KEY)-0x4000u+1;
        check((far_get(b.pages[0],at)|((uint16_t)far_get(b.pages[0],at+1)<<8))==
              cc.keyboard_entry+b.code_origin-CC_CODE_BASE);
    }
    build_close(&b);
    load("var a:array[0..1]of integer;begin writeln(sizeof(a[ord(eof)]))end.");
    check(compile(&s,&b,&d));check(!cc.file_entry&&cc.temporaries==0);build_close(&b);
    invalid("var f:file of integer;begin seek(f,true)end.",23);
    invalid("var f:file of integer;begin write(f,1)end.",41);
    invalid("var f:file of integer;b:byte;begin write(f,b)end.",44);
    invalid("var f:file;b:byte;begin blockread(f,b,1,b)end.",24);
    load("var a:array[0..1]of integer;begin writeln(sizeof(a[random(2)]))end.");
    check(compile(&s,&b,&d)); check(!cc.random_entry&&!cc.random_state);
    build_close(&b);
    invalid("begin writeln(succ(1.0))end.",31);
    invalid("begin writeln(1.0 div 2)end.",47);
    invalid("begin writeln(1.0 and 2)end.",47);
    invalid("var r:real;begin for r:=1 to 2 do end.",30);
    invalid("type a=array[real]of byte;begin end.",30);
    invalid("type a=set of real;begin end.",30);
    invalid("begin writeln(1:5:2)end.",5);
    invalid("begin writeln('x':5:2)end.",5);
    invalid("begin writeln(true:5:2)end.",5);
    invalid("begin writeln('xx':5:2)end.",5);
    invalid("begin writeln(1:true)end.",23);
    invalid("begin writeln(1.0:5:true)end.",23);
    invalid("var s:string[80];begin str(true,s)end.",26);
    invalid("var s:string[80];begin str('x',s)end.",26);
    invalid("var s:string[80];begin str('xx',s)end.",26);
    invalid("var s:string[80];begin str(1:5:2,s)end.",3);
    invalid("var c:char;begin str(1,c)end.",34);
    invalid("begin str(1,'x')end.",41);
    invalid("type t=(a,b);begin writeln(1:a)end.",23);
    invalid("var b:byte;c:integer;begin val('1',b,c)end.",27);
    invalid("type s=1..5;var n:s;c:integer;begin val('1',n,c)end.",27);
    invalid("var b:char;c:integer;begin val('1',b,c)end.",27);
    invalid("var n:integer;c:byte;begin val('1',n,c)end.",24);
    invalid("type s=0..5;var n:integer;c:s;begin val('1',n,c)end.",24);
    invalid("var n,c:real;begin val('1',n,c)end.",24);
    invalid("var c:integer;begin val('1',1,c)end.",41);
    invalid("var n:integer;begin val('1',n,0)end.",41);
    invalid("var n,c:integer;begin val(1,n,c)end.",33);
    invalid("begin goto missing end.",40);
    invalid("label l;begin goto l end.",40);
    invalid("label l,l;begin end.",43);
    invalid("label l;begin l:;l:end.",43);
    invalid("label l;var l:integer;begin end.",43);
    invalid("begin missing:end.",41);
    invalid("label l;var r:record l:integer end;begin with r do begin l:l:=1 end end.",7);
    invalid("label l;procedure p;begin goto l end;begin l:end.",72);
    invalid("label l;procedure p;begin l:end;begin l:end.",72);
    invalid("label l;var i:integer;begin goto l;for i:=1 to 1 do l:halt end.",71);
    invalid("label l;var i:integer;begin if false then for i:=1 to 1 do l:halt;goto l end.",71);
    invalid("label 000;begin goto 0;000:end.",40);
    invalid("label $1;begin end.",58);
    invalid("label 1;begin goto $1;1:end.",40);
    invalid("label 1;begin $1:end.",41);
    invalid("begin writeln(12345678901234567890)end.",56);
    invalid("const x=-12345678901234567890;begin end.",56);
    load("label unused,01,1;begin goto 01;01:goto 1;1:end.");
    check(compile(&s,&b,&d)); ordered_map(); build_close(&b);
    invalid("var a,b:byte absolute $8000;begin end.",75);
    invalid("var b:byte absolute 'A';begin end.",22);
    invalid("var b:byte absolute true;begin end.",22);
    invalid("var b:byte absolute missing;begin end.",41);
    invalid("var b:byte absolute b;begin end.",41);
    invalid("var a:array[1..2]of byte;b:byte absolute a[2];begin end.",1);
    invalid("var n:integer;b:byte absolute n+1;begin end.",1);
    invalid("var n:integer;b:byte absolute addr(n);begin end.",41);
    invalid("function f:byte;var b:byte absolute f;begin end;begin end.",41);
    invalid("var n:integer;begin mem['A']:=0 end.",23);
    invalid("var n:integer;begin mem[addr(n)]:='A' end.",44);
    invalid("var n:integer;begin mem[addr(n)]:=true end.",44);
    invalid("begin port['A']:=0 end.",23);
    invalid("begin port[0]:='A' end.",23);
    invalid("begin writeln(addr(port[0]))end.",41);
    invalid("begin writeln(sizeof(port[0]))end.",41);
    invalid("begin writeln(sizeof(mem))end.",8);
    invalid("procedure p(var b:byte);begin end;begin p(port[0])end.",41);
    invalid("var a:byte;begin fillchar(a,1,ptr(0))end.",31);
    invalid("var a:byte;begin fillchar(a,'A',0)end.",23);
    invalid("var a,b:byte;begin move(a,b,true)end.",23);
    invalid("var a:byte;begin move('AB',a,1)end.",41);
    invalid("function f:byte;begin fillchar(f,1,0)end;begin end.",41);
    invalid("var p:integer;begin new(p)end.",44);
    invalid("begin new(nil)end.",44);
    invalid("begin release(0)end.",44);
    invalid("type p=^integer;var x:p;begin getmem(x,true)end.",23);
    invalid("begin stackptr:=true end.",23);
    invalid("begin writeln(addr(stackptr))end.",44);
    invalid("begin writeln(memavail())end.",5);
    invalid("type n=record p:^n end;begin end.",42);
    invalid("type n=record p:^later end;later=integer;begin end.",42);
    load("var p:^integer;integer:char;begin integer:='A';writeln(sizeof(p^))end.");
    check(compile(&s,&b,&d)); build_close(&b);
    load("type n=integer;r=record p:^n end;var x:r;begin x.p:=nil end.");
    check(compile(&s,&b,&d)); build_close(&b);
    invalid("type p=^missing;begin end.",42);
    invalid("type p=^later;var later:integer;begin end.",42);
    invalid("type p=^integer;const x:p=nil;begin end.",61);
    invalid("const x=nil;begin end.",1);
    invalid("type p=^integer;var x:p;begin x:=p(0)end.",30);
    invalid("type p=^integer;var x:p;begin x:=0 end.",44);
    invalid("type p=^integer;q=^char;var x:p;y:q;begin x:=y end.",44);
    invalid("type p=^integer;var x:p;begin writeln(x<nil)end.",47);
    invalid("type p=^integer;var x:p;begin x:=x+1 end.",47);
    invalid("type p=^integer;function f:p;begin f^:=1 end;begin end.",7);
    invalid("type p=^integer;function f:p;begin f:=nil end;begin writeln(f^)end.",66);
    invalid("type p=^integer;q=^char;var x:q;procedure f(var a:p);begin end;begin f(x)end.",44);
    invalid("begin writeln(succ(nil))end.",31);
    invalid("begin writeln(pred(nil))end.",31);
    invalid("begin writeln(succ(ptr(0)))end.",31);
    invalid("begin writeln(pred(ptr(0)))end.",31);
    invalid("begin writeln(succ([]))end.",31);
    invalid("begin writeln(pred([1]))end.",31);
    invalid("begin writeln(32768) end.",56);
    invalid("var s:string[3]; begin s[true]:='x' end.",23);
    invalid("var s:string[3]; begin s[1][1]:='x' end.",7);
    invalid("var n:integer; begin n[1]:=2 end.",7);
    invalid("var s:string[3]; begin for s[1]:='a' to 'z' do end.",30);
    invalid("type t=string[3]; function f:t; begin f[1]:='x' end; begin end.",7);
    invalid("type t=string[3]; function f:t; begin f:='x' end; begin writeln(f[1]) end.",5);
    invalid("type t=string[3]; procedure p(var s:t); begin end; function f:t; begin p(f) end; begin end.",41);
    invalid("begin writeln(concat()) end.",41);
    invalid("begin writeln(copy(3,1,2)) end.",33);
    invalid("begin writeln(copy('abc',true,2)) end.",23);
    invalid("var s:string[3]; begin delete(s[1],1,2) end.",34);
    invalid("var s:string[3]; begin insert('x',s[1],1) end.",34);
    invalid("var s:string[3]; begin s[1:='x' end.",9);
    invalid("var a:array[boolean] of byte; begin a[1]:=0 end.",44);
    invalid("var a:array[1..3] of byte; begin a[true]:=0 end.",44);
    invalid("type t=array[1..2] of byte; function f:t; begin end; begin end.",48);
    invalid("type a=array[1..2] of byte; b=array[1..2] of byte; var x:a; y:b; begin x:=y end.",44);
    invalid("var a,b:array[1..2] of byte; begin writeln(a=b) end.",62);
    invalid("var a:array[1..256] of char; begin writeln(length(a)) end.",62);
    invalid("var a:array[boolean] of char; begin writeln(length(a)) end.",62);
    invalid("var a:array['a'..'c'] of char; begin writeln(length(a)) end.",62);
    invalid("var a:array[1..3] of char; s:string[3]; begin a:=s end.",44);
    invalid("var a:array[1..3] of char; begin a:='ab' end.",60);
    invalid("var a:array[1..1] of char; begin a:='x' end.",60);
    invalid("var a:array[1..3] of char; begin a:='abc'+'d' end.",1);
    invalid("var a:array[1..3] of char; begin a:=('abc') end.",41);
    invalid("type t=array[integer] of byte; begin end.",98);
    invalid("type t=array[string[3]] of byte; begin end.",30);
    invalid("const a:array[1..2] of integer=(1,2,3); begin end.",5);
    invalid("const a:array[1..3] of byte=(1,256,3); begin end.",44);
    invalid("type r=record a:integer end; var x:r; begin x.z:=1 end.",41);
    invalid("type r=record a,a:integer end; begin end.",43);
    invalid("type r=record case boolean of false:(n:integer);true:(n:char) end;begin end.",43);
    invalid("type a=record n:integer end; b=record n:integer end;var x:a;y:b;begin x:=y end.",44);
    invalid("type r=record n:integer end;function f:r;begin end;begin end.",48);
    invalid("type r=record n:integer end;var x,y:r;begin writeln(x=y) end.",62);
    invalid("type r=record a,b:integer end;const x:r=(b:2;a:1);begin end.",69);
    invalid("type r=record a,b,c:byte end;const x:r=(a:1;c:3);begin end.",69);
    invalid("type r=record case tag:boolean of false:(n:integer) end;const x:r=(n:1);begin end.",69);
    invalid("type r=record case missing of 0:(n:integer) end;begin end.",2);
    invalid("type r=record end;const x:r=();begin end.",41);
    invalid("var n:integer;begin with n do end.",29);
    invalid("{$W0}type r=record n:integer end;var x:r;begin with x do end.",97);
    invalid("{$W1}type r=record n:integer end;var x:r;begin with x,x do end.",97);
    invalid("type r=record n:integer end;var x:r;begin with x,x,x,x,x do end.",97);
    invalid("type r=record n:integer end;var x:r;begin {$W9}with x,x,x,x,x do end.",97);
    invalid("type r=record n:integer end;var x:r;begin with x do for n:=1 to 3 do end.",41);
    invalid("type r=record n:integer end;var x:r;begin for x.n:=1 to 3 do end.",30);
    invalid("type t=set of integer;begin end.",70);
    invalid("type t=set of -1..3;begin end.",70);
    invalid("type t=set of 0..256;begin end.",70);
    invalid("type t=set of byte;function f:t;begin end;begin end.",48);
    invalid("begin writeln(1 in [1,true])end.",44);
    invalid("begin writeln([1]=[true])end.",44);
    invalid("var a,b:set of byte;begin writeln(a<b)end.",47);
    invalid("type t=set of 3..5;const x:t=[0];begin end.",45);
    invalid("type t=set of byte;const x:t=[true];begin end.",44);
    invalid("const x=[1];begin end.",1);
    invalid("var s:set of byte;begin writeln(s)end.",66);
    invalid("type r=record n:integer end;var x:r;begin writeln(x)end.",62);
    invalid("type t=set of 3..5;u=set of 3..5;var x:t;procedure p(var v:u);begin end;begin p(x)end.",44);
    invalid("begin writeln(+32768) end.",56);
    invalid("begin writeln(-32769) end.",56);
    invalid("begin writeln(-(32768)) end.",56);
    invalid("begin writeln('unterminated) end.",55);
    invalid("begin {$Z+} end.",93);
    invalid("begin nope:=1 end.",41);
    invalid("var n,n: integer; begin end.",43);
    invalid("begin writeln(1) writeln(2) end.",1);
    invalid("program begin; begin end.",53);
    invalid("begin {unterminated",91);
    invalid("begin ! end.",58);
    invalid("var s:string[0]; begin end.",49);
    invalid("var s:string[256]; begin end.",49);
    invalid("var s:string; begin end.",8);
    invalid("var s:string[80; begin end.",9);
    invalid("var s:string[8]; begin s:=42 end.",44);
    invalid("var n:integer; begin n:='text' end.",44);
    invalid("var s:string[8]; begin writeln(s*2) end.",47);
    invalid("var s:string[8]; begin writeln(-s) end.",47);
    invalid("var n:boolean; begin readln(n) end.",66);
    invalid("type t=(a,b);var n:t;begin read(n)end.",66);
    invalid("var a:array[1..3]of char;begin readln(a)end.",66);
    invalid("begin readln(1)end.",41);
    invalid("begin readln()end.",41);
    invalid("begin read()end.",41);
    invalid("begin {$B?}end.",93);
    invalid("begin {$I?}end.",90);
    invalid("begin {$I}end.",90);
    invalid("begin {$I0}end.",90);
    invalid("begin {$J+}end.",93);
    load("var n:integer;begin {$I-}write(1);{$I+}write(2);read(n);n:=ioresult end.");
    check(compile(&s,&b,&d));ordered_map();
    check(calls(RT_IOCALL,0)==4&&calls(RT_IOCALL,1)==3&&calls(RT_IORESULT,-1)==1);
    check(cc.io_check==1&&cc.temporaries==0);build_close(&b);
    load("begin {$i-,b-,r+}writeln(ioresult)end.");
    check(compile(&s,&b,&d));check(!cc.io_check&&!cc.input_fresh&&cc.range_check);
    check(calls(RT_IOCALL,0)==6&&calls(RT_IOCALL,1)==0);build_close(&b);
    load("begin writeln(ioresult)end.");
    check(compile(&s,&b,&d));check(cc.io_check&&cc.input_fresh&&!cc.range_check);
    check(calls(RT_IOCALL,1)==3);build_close(&b);
    load("var ioresult:integer;begin ioresult:=7;writeln(ioresult)end.");
    check(compile(&s,&b,&d));check(!calls(RT_IORESULT,-1));build_close(&b);
    load("function ioresult:integer;begin ioresult:=7 end;begin writeln(ioresult)end.");
    check(compile(&s,&b,&d));check(!calls(RT_IORESULT,-1));build_close(&b);
    load("var a:array[0..1]of byte;begin writeln(sizeof(a[ioresult]))end.");
    check(compile(&s,&b,&d));check(!calls(RT_IORESULT,-1));build_close(&b);
    invalid("begin readln(missing) end.",41);
    invalid("begin if 1 then end.",20);
    invalid("begin while 'a' do end.",20);
    invalid("begin repeat until 1 end.",20);
    invalid("begin if true writeln(1) end.",17);
    invalid("begin while true writeln(1) end.",13);
    invalid("begin if true then writeln(1); else writeln(2) end.",1);
    invalid("var b:boolean; begin b:=1 end.",44);
    invalid("begin writeln(true+false) end.",47);
    invalid("begin writeln(1 and true) end.",47);
    invalid("begin writeln(not 'x') end.",47);
    invalid("begin writeln(1='x') end.",47);
    invalid("var i:integer; begin for i:=1 do end.",18);
    invalid("var i:integer; begin for i:=1 to 3 i:=2 end.",13);
    invalid("var i:integer; begin for i:=1 to 3 do i:=2 end.",44);
    invalid("var i:integer; begin for i:=1 to 3 do for i:=1 to 2 do end.",44);
    invalid("var i:integer; begin for i:=true to false do end.",44);
    invalid("const x=1+2; begin end.",1);
    invalid("const x=x; begin end.",41);
    invalid("const x=1; begin x:=2 end.",44);
    invalid("type t=integer; begin t:=2 end.",44);
    invalid("type t=5..1; begin end.",52);
    invalid("type t=1..true; begin end.",51);
    invalid("type t='ab'..'cd'; begin end.",51);
    invalid("type a=(red,green); b=(blue,yellow); var x:a; begin x:=blue end.",44);
    invalid("type a=(red,green); b=(blue,yellow); begin writeln(red=blue) end.",47);
    invalid("type a=(red,green); begin writeln(red) end.",44);
    invalid("const n:byte=256; begin end.",44);
    invalid("var n:integer; const x=n; begin end.",41);
    invalid("begin writeln(ord([])) end.",31);
    invalid("begin writeln(chr(true)) end.",23);
    invalid("begin writeln(#256) end.",56);
    invalid("begin writeln(#$100) end.",56);
    invalid("begin writeln($00000) end.",56);
    invalid("begin case 'text' of end end.",41);
    invalid("begin case 1 of end end.",41);
    invalid("begin case 1 of else end end.",41);
    invalid("begin case 1 end end.",15);
    invalid("begin case 1 of 1 end end.",3);
    invalid("begin case 1 of 'a': end end.",46);
    invalid("begin case 'a' of 1: end end.",46);
    invalid("begin case true of 1: end end.",46);
    invalid("begin case 1 of missing: end end.",41);
    invalid("var n:integer; begin case 1 of n: end end.",41);
    invalid("type t=(red,green); u=(blue,white); begin case red of blue: end end.",46);
    invalid("begin case 1 of 0..true: end end.",46);
    invalid("begin case 1 of 1:;",14);
    invalid("procedure p; forward; begin end.",73);
    invalid("procedure p; begin end; procedure p; begin end; begin end.",43);
    invalid("procedure p(a:integer); begin end; begin p end.",4);
    invalid("procedure p(a:integer); begin end; begin p(true) end.",44);
    invalid("procedure p(var a:integer); begin end; begin p(1) end.",44);
    invalid("var b:byte; procedure p(var a:integer); begin end; begin p(b) end.",44);
    invalid("procedure p(var a); begin a:=1 end; begin end.",44);
    invalid("procedure p(var a); begin writeln(a) end; begin end.",44);
    invalid("procedure p; var hidden:integer; begin end; begin hidden:=1 end.",41);
    invalid("begin writeln(length(1)) end.",44);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin writeln(f-1) end.",47);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin writeln(1+f) end.",47);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin writeln(f+true) end.",47);
    invalid("type text=string[3]; function f:text; begin f:='a' end; begin if f then end.",20);
    invalid("type text=string[3]; var c:char; function f:text; begin f:='a' end; begin for c:=f to 'z' do end.",44);
    invalid("type text=string[3]; var c:char; function f:text; begin f:='a' end; begin for c:='a' to f do end.",44);
    invalid("type text=string[3]; var s:string[4]; procedure p(var a:text); begin end; begin p(s) end.",44);
    load("type text=string[3]; function f:text; begin f:='abc' end; begin writeln(f) end.");
    check(compile(&s,&b,&d)); ordered_map(); build_close(&b);
    load("const x=-32768; y=x; z=-y; type octet=byte; text=string[3]; var n:octet; s:text; begin n:=257; s:='x'; writeln(x,y,z,n,s) end.");
    check(compile(&s,&b,&d)&&b.globals_bytes==5); build_close(&b);
    load("var b:boolean; n:integer; begin b:=not false; n:=1234; writeln(b,n) end.");
    check(compile(&s,&b,&d)&&b.globals_bytes==3); ordered_map(); build_close(&b);
    load("var i,n:integer; begin if true then if false then n:=1 else n:=2;\n"
         "for i:=1 to 3 do begin while false do n:=9; repeat n:=n-1 until n=0 end end.");
    check(compile(&s,&b,&d)); ordered_map(); build_close(&b);
    load("var a,b:string[80]; n:integer; c:string[255]; begin a:='Hello'; b:=a; c:=''; writeln(b,n,c); readln(a) end.");
    check(compile(&s,&b,&d)); check(b.globals_bytes==420&&b.code_origin==GLOBAL_BASE+420);
    build_close(&b);
    load("var writeln: integer; begin writeln:=1 end.");
    check(compile(&s,&b,&d)); build_close(&b);
    load("program Slice;\nvar n: integer;\nbegin n:=-32768; writeln('N=',n,',',7 div -3,',',7 mod -3) end.");
    {
    unsigned before_allocations=allocation_calls,required;
    check(compile(&s,&b,&d));required=allocation_calls-before_allocations;build_close(&b);
    check(required==8);
    for(i=0;i<required;++i) {
        fail_after=(int)i;
        check(!compile(&s,&b,&d)&&d.code==98); check(pages_used()==1);
    }
    fail_after=-1;
    }
    check(compile(&s,&b,&d)); check(build_check(&s,&b)); check(build_ready(&s,&b));
    check(b.length<=2u*PAGE_SIZE&&b.pages[2]==255&&b.versions[2]==0);
    old=b.pages[1];b.pages[1]=255;check(!build_check(&s,&b)&&!build_ready(&s,&b));b.pages[1]=old;
    b.pages[2]=old;check(!build_check(&s,&b)&&!build_ready(&s,&b));b.pages[2]=255;
    check(build_check(&s,&b)&&build_ready(&s,&b));
    old=far_get(b.map_page,b.map_length-1);
    far_set(b.map_page,b.map_length-1,old^1);
    check(!build_check(&s,&b));
    check(!build_ready(&s,&b));
    far_set(b.map_page,b.map_length-1,old); check(build_check(&s,&b));
    old=far_get(b.pages[1],0);
    far_set(b.pages[1],0,old^1); check(!build_check(&s,&b)); check(!build_ready(&s,&b));
    far_set(b.pages[1],0,old); check(build_check(&s,&b));
    check(!build_locate(&s,&b,b.code_bytes,&position));
    check(build_locate(&s,&b,0,&position));
    check(position==(uint16_t)(strstr("program Slice;\nvar n: integer;\nbegin n:=-32768;", "n:=-")-"program Slice;\nvar n: integer;\nbegin n:=-32768;"));
    file=fopen(argv[1],"wb"); check(file!=NULL);
    for(i=0;i<b.length;++i) fputc(image_get(&b,(uint16_t)i),file);
    check(!fclose(file));
    check(source_replace(&s,s.length,s.length,(const uint8_t*)" ",1));
    check(!build_check(&s,&b)); build_close(&b);
    load("begin\nwriteln(7 div 0)\nend.");
    check(source_replace(&s,5,5,(const uint8_t*)"   ",3));
    check(compile(&s,&b,&d));
    check(build_locate(&s,&b,0,&position)&&position==9);
    build_close(&b);
    {
        static uint8_t text[SOURCE_LIMIT];
        uint16_t n;
        file=fopen("tests/native/control.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        check(b.globals_bytes==29); ordered_map(); build_close(&b);
        file=fopen("tests/native/ordinals.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        check(b.globals_bytes==19); ordered_map(); build_close(&b);
        file=fopen("tests/native/cases.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/routines.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/stringfn.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/strlife.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        file=fopen("tests/native/strbound.pas","rb"); check(file!=NULL);
        n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
        check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
        ordered_map(); build_close(&b);
        {
            const char *fixtures[]={"strindex","strops","strop255","arrays","arrayarg","chararr","records","variants","withrec","zerorec","sets","setshape","setargs","setabi","pointers","ptrargs","ordchar","heapmark","heapfree","pointerabi","absolute","absframe","rawmemory","nativeio","gotos","reals","realabi","formatout","strformat","valparse","readinput","endchar","sqrt","sqrtshadow","logexp","trig","trigshadow","random","randshadow","randomseed","filetypes","filetokens","params","diskfiles","diskblocks","disklife","diskwide","textdisk","textseek","textnumber","textformat","textrecover","textnest","texteof","textedges","textargs","capacity","textpath","textrename","textrens","stdinput","stdoutput","stdpred","keyonly","keybuffer","keybreak","keyhigh"};
            unsigned fixture;
            char path[80];
            for(fixture=0;fixture<sizeof(fixtures)/sizeof(fixtures[0]);++fixture) {
                snprintf(path,sizeof(path),"tests/native/%s.pas",fixtures[fixture]);
                file=fopen(path,"rb"); check(file!=NULL);
                n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
                check(source_import(&s,text,n,&d));
                old=compile(&s,&b,&d); if(!old) fprintf(stderr,"fixture %s\n",fixtures[fixture]); check(old);
                if(!strcmp(fixtures[fixture],"absolute")) check(b.globals_bytes==8);
                if(!strcmp(fixtures[fixture],"realabi")) check(b.globals_bytes==0);
                if(!strcmp(fixtures[fixture],"textedges")||!strcmp(fixtures[fixture],"capacity")) {
                    check(b.code_bytes>PAGE_SIZE&&0x4000u+b.length<=CC_STATIC_LIMIT&&b.pages[2]!=255);
                    check(cc.stage_page==255);
                }
                ordered_map(); build_close(&b);
                if(!strcmp(fixtures[fixture],"textedges")) {
                    for(i=0;i<9;++i) {
                        fail_after=(int)i;
                        check(!compile(&s,&b,&d)&&d.code==98);
                        check(pages_used()==1&&cc.stage_page==255);
                    }
                    fail_after=-1;
                }
            }
        }
        {
            const char *names[]={"stringabi","setabi","pointerabi"};
            unsigned abi;
            char path[80];
            for(abi=0;abi<sizeof(names)/sizeof(names[0]);++abi) {
                snprintf(path,sizeof(path),"tests/native/%s.pas",names[abi]);
                file=fopen(path,"rb"); check(file!=NULL);
                n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
                check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
                check(b.globals_bytes==0&&b.code_origin==GLOBAL_BASE);
                snprintf(path,sizeof(path),"build/%s.zpl",names[abi]);
                file=fopen(path,"rb"); check(file!=NULL);
                n=(uint16_t)fread(text,1,sizeof(text),file); check(!ferror(file)&&!fclose(file));
                check(n==image_get(&b,0x4003));
                for(i=0;i<n;++i) check(image_get(&b,0x4004+(uint16_t)i)==text[i]);
                ordered_map(); build_close(&b);
            }
        }
        for(i=256;i<=257;++i) {
            unsigned member;
            n=(uint16_t)snprintf((char*)text,sizeof(text),"type t=(");
            for(member=0;member<i;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"v%u%s%s",member,member+1<i?",":"",member%10==9?"\n":"");
            n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"); var x:t; begin x:=v%u; writeln(ord(x),sizeof(t)) end.",i-1);
            check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
            check(b.globals_bytes==(i==256?1:2)); build_close(&b);
        }
        {
            unsigned member;
            n=(uint16_t)snprintf((char*)text,sizeof(text),"type t=(");
            for(member=0;member<513;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"v%u%s%s",member,member<512?",":"",member%10==9?"\n":"");
            snprintf((char*)text+n,sizeof(text)-n,"); begin end.");
            invalid((char*)text,98);
            n=(uint16_t)snprintf((char*)text,sizeof(text),"type\n");
            for(member=0;member<60;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"t%u=string[1];\n",member);
            snprintf((char*)text+n,sizeof(text)-n,"begin end.");
            invalid((char*)text,98);
        }
        {
            char left[128],right[128];
            memset(left,'a',126); left[126]='x'; left[127]=0;
            memcpy(right,left,128); right[126]='y';
            n=(uint16_t)snprintf((char*)text,sizeof(text),"var\n%s\n,\n%s\n:byte;\nbegin\n%s\n:=1;\n%s\n:=2 end.",left,right,left,right);
            check(source_import(&s,text,n,&d)); check(compile(&s,&b,&d));
            check(b.globals_bytes==2); build_close(&b);
        }
        {
            unsigned arm;
            n=(uint16_t)snprintf((char*)text,sizeof(text),"begin\n");
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"case 1 of 1:\n");
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"end\n");
            snprintf((char*)text+n,sizeof(text)-n,"end.");
            invalid((char*)text,98);
            n=(uint16_t)snprintf((char*)text,sizeof(text),"begin case 1 of\n");
            for(arm=0;arm<600;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"%u:;\n",arm);
            snprintf((char*)text+n,sizeof(text)-n,"end end.");
            invalid((char*)text,99);
            n=0;
            for(arm=0;arm<256;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"procedure p%u; forward;\n",arm);
            snprintf((char*)text+n,sizeof(text)-n,"begin end.");
            invalid((char*)text,98);
            n=0;
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"procedure p%u;\n",arm);
            for(arm=0;arm<25;++arm) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"begin end;\n");
            snprintf((char*)text+n,sizeof(text)-n,"begin end.");
            invalid((char*)text,98);
        }
    }
    {
        unsigned member;
        uint8_t text[1024];
        uint16_t n=(uint16_t)snprintf((char*)text,sizeof(text),"type b=array[1..127]of string[255];const x:b=(");
        for(member=0;member<127;++member) n+=(uint16_t)snprintf((char*)text+n,sizeof(text)-n,"%s''%s",member?",":"",member%10==9?"\n":"");
        snprintf((char*)text+n,sizeof(text)-n,");begin end.");
        invalid((char*)text,99); check(cc.stage_page==255);
    }
    source_close(&s); check(!pages_used());
    printf("PASS: %u host front-end, identity and allocation-failure checks\n",checks);
    return 0;
}
