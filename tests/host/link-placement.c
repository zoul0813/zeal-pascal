#include "compiler/link/link.h"
#include <assert.h>
#include <stdio.h>
Compiler cc;
static Build image;static Diagnostic diagnostic;
static uint8_t memory[32][PAGE_SIZE],roles[64],code[256];
static uint32_t lives[64];volatile uint32_t page_versions[64];
static unsigned assertions,polls,cancel;
static uint8_t drop_failure=255;
static void check(uint8_t ok){++assertions;assert(ok);}
void cc_fail(uint8_t error){if(!diagnostic.code)diagnostic.code=error;}
uint8_t cc_poll(void){if(++polls==cancel){cc_fail(COMPILE_INTERRUPTED);return 0;}return 1;}
uint8_t page_new(uint8_t role){uint8_t p;for(p=32;p<64;++p)if(!roles[p]){roles[p]=role;++lives[p];return p;}return 255;}
uint8_t page_drop(uint8_t p){assert(p>=32&&p<64&&roles[p]);if(p==drop_failure)return 0;roles[p]=0;return 1;}
uint8_t page_role(uint8_t p){return roles[p];}
uint32_t page_lifetime(uint8_t p){return lives[p];}
uint8_t far_get(uint8_t p,uint16_t at){assert(p>=32&&p<64&&roles[p]&&at<PAGE_SIZE);return memory[p-32][at];}
void far_set(uint8_t p,uint16_t at,uint8_t value){assert(p>=32&&p<64&&roles[p]&&at<PAGE_SIZE);memory[p-32][at]=value;++page_versions[p];}
uint8_t far_read(uint8_t p,uint16_t at,uint8_t *out,uint16_t count){uint16_t i;for(i=0;i<count;++i)out[i]=far_get(p,at+i);return 1;}
void cc_put(uint16_t site,uint8_t value){assert(site>=0x8000&&site<0x8100);code[site-0x8000]=value;}
uint8_t cc_code_get(uint16_t site){assert(site>=0x8000&&site<0x8100);return code[site-0x8000];}
void cc_word(uint16_t site,uint16_t value){cc_put(site,(uint8_t)value);cc_put(site+1,(uint8_t)(value>>8));}
static void reset(void){check(cc_tables_end());memset(&cc,0,sizeof(cc));memset(&diagnostic,0,sizeof(diagnostic));memset(code,0xa5,sizeof(code));cc.image=&image;cc.error=&diagnostic;image.globals_bytes=8;cc.emitted=0x8100;polls=cancel=0;}
int main(void){
 uint8_t raw[CC_LINK_BYTES];uint16_t i;
 reset();
 cc_link_reference(0x8000,CL_CODE,0,255,CF_WORD16);
 cc_link_reference(0x8002,CL_CODE,0,255,CF_LOW8);
 cc_link_reference(0x8003,CL_CODE,0,255,CF_HIGH8);
 cc_link_reference(0x8004,CL_DATA,0,3,CF_WORD16);
 cc_link_reference(0x8006,CL_DATA,0,3,CF_LOW8);
 cc_link_reference(0x8007,CL_DATA,0,3,CF_HIGH8);
 check(cc.links==6&&!diagnostic.code);check(cc_link_apply(0x8101,0x4ffe));
 check(!memcmp(code,"\0\202\0\202\1\120\1\120",8));check(code[8]==0xa5);
 reset();cc_link_reference(0x80ff,CL_CODE,0,0,CF_HIGH8);check(cc_link_apply(0x8101,0x8000));check(code[255]==0x81);
 reset();cc_link_reference(0x80ff,CL_CODE,0,0,CF_WORD16);check(!cc_link_apply(0x8101,0x8000)&&diagnostic.code==99);
 reset();cc_link_reference(0x8000,CL_CODE,0,256,CF_HIGH8);check(!cc_link_apply(0x8101,0x8000)&&diagnostic.code==99);
 reset();cc_link_reference(0x8000,CL_DATA,0,9,CF_LOW8);check(!cc_link_apply(0x8101,0x8000)&&diagnostic.code==99);
 reset();cc_link_reference(0x8000,CL_CODE,0,255,CF_WORD16);check(!cc_link_apply(0xff01,0x8000)&&diagnostic.code==99);
 reset();cc_link_reference(0x8000,CL_DATA,0,7,CF_WORD16);check(!cc_link_apply(0x8101,0xffff)&&diagnostic.code==99);
 reset();cc_link_reference(0x7fff,CL_CODE,0,0,CF_WORD16);check(diagnostic.code==99&&!cc.links);
 reset();cc_link_reference(0x8000,CL_CODE,1,0,CF_WORD16);check(diagnostic.code==99&&!cc.links);
 reset();cc_link_reference(0x8000,CL_RUNTIME+1,0,0,CF_WORD16);check(diagnostic.code==99&&!cc.links);
 reset();cc_link_reference(0x8000,CL_CODE,0,0,CF_HIGH8+1);check(diagnostic.code==99&&!cc.links);
 reset();cc.suppress=1;cc_link_reference(0x8000,CL_CODE,0,0,CF_WORD16);check(!cc.links&&!diagnostic.code);
 reset();cc_link_reference(0x8000,CL_CODE,0,0,CF_WORD16);cancel=1;check(!cc_link_apply(0x8101,0x8000)&&diagnostic.code==COMPILE_INTERRUPTED);check(code[0]==0xa5);
 reset();cc_word(0x8000,0x8201);cc_link_reference(0x8000,CL_DATA_ADJUST,2,0,CF_WORD16);
 cc_link_reference(0x8002,CL_DATA_ADJUST,65535u,0,CF_LOW8);code[2]=0x10;
 check(cc_link_apply(0x8101,0x5001));check((code[0]|((uint16_t)code[1]<<8))==0x2203);check(code[2]==0x0f);
 reset();cc_link_reference(0x8000,CL_DATA_ADJUST,1,0,CF_HIGH8);check(diagnostic.code==99&&!cc.links);
 reset();
 cc_link_reference(0x8000,CL_DATA,0,8,CF_WORD16);check(cc_link_apply(0x8101,0x5000));check((code[0]|((uint16_t)code[1]<<8))==0x5008);
 reset();
 /* Cross64 KiB without reducing ordinary four-byte relocation capacity. */
 for(i=0;i<9000;++i)cc_link_reference(0x8000,CL_CODE,0,i%256,CF_HIGH8);
 check(cc.links==9000&&!diagnostic.code);check(cc_link_read(8192,raw));check(raw[0]==0&&raw[1]==0x80&&raw[7]==CF_HIGH8);
 check(cc_link_read(8999,raw)&&raw[4]==8999%256);check(cc_link_apply(0x8101,0x8000));check(code[0]==(uint8_t)((0x8101+8999%256)>>8));
 /* A failed arena release keeps its last live owner for a safe retry. */
 for(i=63;i>=32&&!roles[i];--i){}
 drop_failure=(uint8_t)i;check(!cc_tables_end());check(roles[i]==PAGE_COMPILER);
 drop_failure=255;check(cc_tables_end());
 reset();for(i=32;i<64;++i)check(!roles[i]);
 printf("%u link placement assertions passed\n",assertions);return 0;
}
