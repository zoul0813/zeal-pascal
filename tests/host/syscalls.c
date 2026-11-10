#include "runtime/syscalls/abi.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
uint8_t zs_memory[65536] __attribute__((aligned(16)));
static unsigned calls,fail_call;static uint8_t operation;
static uint16_t short_count,sent_bytes;static uint8_t short_read,verify_write;
static uint16_t get(uint16_t at){return zs_memory[at]|zs_memory[at+1]<<8;}
static void put(uint16_t at,uint16_t n){zs_memory[at]=n;zs_memory[at+1]=n>>8;}
void zs_invoke(ZOSRegisters *r,uint16_t op) {
    ++calls;operation=op;
    if(calls==fail_call){r->a=13;return;}
    switch(op){
    case 0:if(short_read)r->bc=short_count;memset(zs_memory+r->de,0x5a,r->bc);r->a=0;break;
    case 1:{uint16_t i;if(verify_write)for(i=0;i<r->bc;++i)assert(zs_memory[r->de+i]==((sent_bytes+i)%251)+1);
        if(short_read)r->bc=short_count;sent_bytes+=r->bc;r->a=0;break;}
    case 2:assert(!strcmp((char*)zs_memory+r->bc,"H:/DATA"));assert(r->h==2);r->a=0xfc;break;
    case 5:assert(!strcmp((char*)zs_memory+r->bc,"H:/DATA"));memset(zs_memory+r->de,0x32,29);r->a=0;break;
    case 6:assert(r->h==3&&r->bc==0x1234&&r->de==0x5678&&r->a==2);r->bc=0x2233;r->de=0x4455;r->a=0;break;
    case 7:assert(r->h==3&&r->bc==9);r->de=456;r->a=0;break;
    case 10:memcpy(zs_memory+r->de,"H:/DIR",7);r->a=0;break;
    case 14:assert(r->de==0x4801);r->a=0;break;
    case 16:assert(!strcmp((char*)zs_memory+r->bc,"H:/DATA"));assert(!strcmp((char*)zs_memory+r->de,"--test"));r->de=0x4200;r->a=0;break;
    case 20:r->de=65535;r->a=0;break;
    case 25:r->bc=0x2a00;r->a=0;break;
    case 26:assert(r->bc==0x2a00);r->a=0;break;
    default:r->a=0;break;
    }
}
static ZOSJob fresh(uint8_t op){
    ZOSJob j;memset(&j,0,sizeof(j));j.call=op;j.data_base=0x8000;j.static_end=0x9000;
    j.heap_floor=j.heap_frontier=0x9000;j.stack_pointer=0xd000;j.scratch=0x6000;
    calls=fail_call=0;short_read=verify_write=0;short_count=sent_bytes=0;memset(zs_memory+0x6000,0xa5,256);return j;
}
static void run(ZOSJob *j){unsigned i;zs_dispatch(j);for(i=0;i<256;++i)assert(zs_memory[0x6000+i]==0xa5);}
static void path(uint16_t at,const char*s){zs_memory[at]=strlen(s);memcpy(zs_memory+at+1,s,strlen(s));}
int main(void){ZOSJob j;ZOSRegisters r;unsigned i,k;
    const uint16_t sizes[]={0,1,127,128,129,256,1024};
    for(k=0;k<2;++k)for(i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i){
        j=fresh(k);j.args[0]=3;j.args[1]=0x8000;j.sizes[1]=1024;j.args[2]=0x8500;put(0x8500,sizes[i]);
        if(k){unsigned at;verify_write=1;for(at=0;at<1024;++at)zs_memory[0x8000+at]=(at%251)+1;}
        run(&j);assert(!j.result&&calls==(sizes[i]+127u)/128u&&get(0x8500)==sizes[i]);
    }
    for(k=0;k<2;++k){
        j=fresh(k);j.args[1]=0x8000;j.sizes[1]=300;j.args[2]=0x8500;put(0x8500,300);fail_call=2;
        run(&j);assert(j.result==13&&calls==2&&get(0x8500)==128);
        j=fresh(k);j.args[1]=0x8000;j.sizes[1]=300;j.args[2]=0x8500;put(0x8500,300);short_read=1;short_count=17;
        run(&j);assert(!j.result&&calls==1&&get(0x8500)==17);
    }
    j=fresh(0);j.args[0]=1;j.args[1]=0xd100;j.sizes[1]=300;j.args[2]=0xd000;put(0xd000,300);
    run(&j);assert(!j.result&&calls==3&&get(0xd000)==300&&zs_memory[0xd22b]==0x5a);
    j=fresh(0);j.args[1]=0x8000;j.sizes[1]=300;j.args[2]=0x8500;put(0x8500,300);fail_call=2;
    run(&j);assert(j.result==13&&calls==2&&get(0x8500)==128);
    j=fresh(0);j.args[1]=0x8000;j.sizes[1]=2;j.args[2]=0x8500;put(0x8500,3);run(&j);assert(j.result==6&&!calls&&get(0x8500)==3);
    j=fresh(2);j.args[0]=0x8000;j.args[1]=2;path(0x8000,"H:/DATA");run(&j);assert((int16_t)j.result==-4);
    j=fresh(5);j.args[0]=0x8000;j.args[1]=0xd000;j.sizes[1]=29;run(&j);assert(!j.result&&zs_memory[0xd01c]==0x32);
    j=fresh(6);j.args[0]=3;j.args[1]=0x8500;j.args[2]=2;put(0x8500,0x5678);put(0x8502,0x1234);run(&j);assert(!j.result&&get(0x8500)==0x4455&&get(0x8502)==0x2233);
    j=fresh(7);j.args[0]=3;j.args[1]=9;j.args[2]=123;j.args[3]=0xd000;run(&j);assert(!j.result&&get(0xd000)==456);
    j=fresh(7);j.mode=1;j.args[0]=3;j.args[1]=9;j.args[2]=8;j.args[3]=0xd000;j.args[4]=0xd100;j.sizes[4]=8;run(&j);assert(!j.result&&get(0xd000)==456);
    j=fresh(10);j.args[0]=0xd000;j.sizes[0]=127;run(&j);assert(!j.result&&zs_memory[0xd000]==6);
    j=fresh(10);j.args[0]=0xd000;j.sizes[0]=1;zs_memory[0xd000]=99;run(&j);assert(j.result==6&&zs_memory[0xd000]==99);
    j=fresh(14);j.args[0]=3;j.args[1]='H';j.args[2]=1;run(&j);assert(!j.result);
    j=fresh(16);j.args[0]=1;j.args[1]=0x8000;j.args[2]=0xd000;j.args[3]=0xd100;path(0xd000,"--test");run(&j);assert(!j.result&&zs_memory[0xd100]==0x42);
    j=fresh(20);j.args[1]=0xd000;run(&j);assert(!j.result&&get(0xd000)==65535);
    j=fresh(25);j.args[0]=0xd000;run(&j);assert(!j.result&&zs_memory[0xd000]==42);
    j=fresh(26);j.args[0]=42;run(&j);assert(!j.result);
    j=fresh(25);j.mode=2;j.args[0]=0xd000;j.sizes[0]=6;memset(&r,0,sizeof(r));memcpy(zs_memory+0xd000,&r,6);run(&j);assert(!j.result&&get(0xd002)==0x2a00);
    j=fresh(3);j.args[0]=256;run(&j);assert(j.result==6&&!calls);
    j=fresh(2);j.args[0]=0x8000;j.args[1]=2;path(0x8000,"a\0b");zs_memory[0x8000]=3;zs_memory[0x8002]=0;run(&j);assert((int16_t)j.result==-6&&!calls);

    j=fresh(2);j.scratch=0x7fff;run(&j);assert((int16_t)j.result==-6&&!calls);
    j=fresh(11);j.static_end=0x7000;run(&j);assert((int16_t)j.result==-6&&!calls);
    j=fresh(0);j.args[1]=0x8000;j.sizes[1]=300;j.args[2]=0x8500;put(0x8500,300);short_read=1;short_count=17;
    run(&j);assert(!j.result&&calls==1&&get(0x8500)==17);
    j=fresh(0);j.args[1]=0x8000;j.sizes[1]=300;j.args[2]=0x8500;put(0x8500,300);short_read=1;
    run(&j);assert(!j.result&&calls==1&&get(0x8500)==0);
    j=fresh(0);j.args[1]=0x8000;j.sizes[1]=300;j.args[2]=0x8500;put(0x8500,0);
    run(&j);assert(!j.result&&!calls&&get(0x8500)==0);
    j=fresh(0);j.static_end=j.heap_floor=j.heap_frontier=0xc020;j.args[1]=0xbff0;j.sizes[1]=32;j.args[2]=0x8500;put(0x8500,32);
    run(&j);assert(!j.result&&calls==1&&get(0x8500)==32);
    j=fresh(0);j.heap_frontier=0x9014;put(0x9000,8);put(0x900a,8);j.args[1]=0x9005;j.sizes[1]=10;j.args[2]=0x8500;put(0x8500,10);
    run(&j);assert(j.result==6&&!calls);
    j=fresh(0);j.heap_frontier=0x900a;put(0x9000,0x8008);j.args[1]=0x9002;j.sizes[1]=8;j.args[2]=0x8500;put(0x8500,8);
    run(&j);assert(j.result==6&&!calls);
    /* A failing kernel call must not publish a partially filled output. */
    for(k=0;k<3;++k){
        j=fresh(k==0?5:k==1?12:22);fail_call=1;
        j.args[0]=k==0?0x8000:k==1?3:0x8600;
        j.args[1]=0x8600;j.sizes[k==2?0:1]=29;
        path(0x8000,"H:/DATA");memset(zs_memory+0x8600,0xa7,29);
        run(&j);assert(j.result==13&&calls==1);
        for(i=0;i<29;++i)assert(zs_memory[0x8600+i]==0xa7);
    }
    j=fresh(7);j.mode=1;j.args[0]=3;j.args[1]=9;j.args[2]=128;
    j.args[3]=0x8500;j.args[4]=0x8600;j.sizes[4]=128;fail_call=1;
    put(0x8500,999);memset(zs_memory+0x8600,0x37,128);
    run(&j);assert(j.result==13&&get(0x8500)==999);
    for(i=0;i<128;++i)assert(zs_memory[0x8600+i]==0x37);
    for(k=0;k<2;++k){
        j=fresh(7);j.mode=1;j.args[0]=3;j.args[1]=9;j.args[2]=k?129:0;
        j.args[3]=0x8500;j.args[4]=0x8600;j.sizes[4]=256;put(0x8500,999);
        run(&j);assert(j.result==6&&!calls&&get(0x8500)==999);
    }
    /* Normal/buffer calls reject page mapping and unknown selectors before
     * reaching the kernel; raw register mode deliberately has a wider API. */
    for(k=0;k<2;++k)for(i=23;i<256;++i)if(i==23||i>=27){
        j=fresh(i);j.mode=k;run(&j);assert(j.result==6&&!calls);
    }
    j=fresh(0);j.call=256;run(&j);assert(j.result==6&&!calls);
    j=fresh(0);j.call=65535;run(&j);assert(j.result==6&&!calls);
    puts("syscall transport: passed");return 0;
}
