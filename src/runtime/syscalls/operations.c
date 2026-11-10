#include "runtime/syscalls/abi.h"
#include <stddef.h>
#include <string.h>
#include <zos_errors.h>
/* A synchronous call owns this state. No Pascal callback occurs inside it. */
typedef char zs_job_layout[sizeof(ZOSJob)==ZS_JOB_BYTES?1:-1];
typedef char zs_register_layout[sizeof(ZOSRegisters)==6?1:-1];
static uint8_t *buffers[2];
#ifdef ZS_HOST
extern uint8_t zs_memory[65536];
#define PTR(at) (zs_memory+(at))
#define ADDRESS(p) ((uint16_t)((uint8_t *)(p)-zs_memory))
#else
#define PTR(at) ((uint8_t *)(uintptr_t)(at))
#define ADDRESS(p) ((uint16_t)(uintptr_t)(p))
#endif
static uint16_t word(uint16_t at) {return PTR(at)[0]|((uint16_t)PTR(at)[1]<<8);}
static void put(uint16_t at,uint16_t value) {PTR(at)[0]=value;PTR(at)[1]=value>>8;}
static uint8_t within(uint16_t at,uint16_t n,uint16_t lo,uint16_t hi) {
    return at>=lo&&at<=hi&&n<=hi-at;
}
static uint8_t span(ZOSJob *j,uint16_t at,uint16_t n) {
    uint16_t cursor,header,size,next;uint8_t found=0;
    if(within(at,n,j->data_base,j->static_end)||within(at,n,j->stack_pointer,0xfc00))return 1;
    cursor=j->heap_floor;
    while(cursor<j->heap_frontier) {
        if(j->heap_frontier-cursor<2)return 0;
        header=word(cursor);size=header&0x7fff;
        if((size&1)||size>j->heap_frontier-cursor-2)return 0;
        next=cursor+2+size;
        if(!(header&0x8000)&&within(at,n,cursor+2,next))found=1;
        cursor=next;
    }
    return cursor==j->heap_frontier&&found;
}
static uint8_t string(ZOSJob *j,uint8_t arg,uint8_t which,uint16_t limit) {
    uint16_t at=j->args[arg],n;
    if(!span(j,at,1))return 0;n=PTR(at)[0];
    if(n>=limit||!span(j,at,n+1)||memchr(PTR(at)+1,0,n))return 0;
    memcpy(buffers[which],PTR(at)+1,n);buffers[which][n]=0;return 1;
}
static uint8_t byte(uint16_t n) {return n<=255u;}
static uint8_t ready(uint8_t which,uint16_t n) {
    uint16_t address=ADDRESS(buffers[which]);
    /* Staging is never handed to the kernel in page3 or across a page. */
    return address>=0x4000u&&(uint32_t)address+n<=0xc000u&&
        (address&0x3fff)+n<=0x4000u;
}
static void normal(ZOSJob *j) {
    ZOSRegisters r;uint16_t a,b,c,d,e,n,done,amount,at;uint8_t op;
    j->result=j->mode==0&&(j->call==2||j->call==11)?(uint16_t)-(int16_t)ERR_INVALID_PARAMETER:ERR_INVALID_PARAMETER;
    if(j->data_base<0x40a0u||j->static_end<j->data_base||j->static_end>j->heap_floor||
       j->heap_floor>j->heap_frontier||j->heap_frontier>j->stack_pointer||
       j->stack_pointer<0xc800u||j->stack_pointer>0xfc00u)return;
    if(!byte(j->call))return;op=j->call;
    a=j->args[0];b=j->args[1];c=j->args[2];d=j->args[3];e=j->args[4];
    memset(&r,0,sizeof(r));
    if(j->mode==2) {
        if(j->sizes[0]!=6||!span(j,a,6))return;
        memcpy(&r,PTR(a),6);zs_invoke(&r,op);memcpy(PTR(a),&r,6);j->result=r.a;return;
    }
    if(!ready(0,128)||!ready(1,128))return;
    if(j->mode==1) {
        if(op!=7||!byte(a)||!byte(b)||!c||c>128||c>j->sizes[4]||
           !span(j,d,2)||!span(j,e,c))return;
        memcpy(buffers[0],PTR(e),c);r.h=a;r.bc=b;r.de=ADDRESS(buffers[0]);
        zs_invoke(&r,op);j->result=r.a;
        if(!r.a){memcpy(PTR(e),buffers[0],c);put(d,r.de);}return;
    }
    switch(op) {
    case 0:case 1:
        if(!byte(a)||!span(j,c,2))return;n=word(c);
        if(n>j->sizes[1]||!span(j,b,n))return;
        done=0;
        while(done<n) {
            amount=n-done;if(amount>128)amount=128;
            if(op==1)memcpy(buffers[0],PTR(b+done),amount);
            r.h=a;r.bc=amount;r.de=ADDRESS(buffers[0]);zs_invoke(&r,op);
            if(r.a)break;
            if(r.bc>amount){r.a=ERR_INVALID_PARAMETER;break;}
            if(op==0&&r.bc)memcpy(PTR(b+done),buffers[0],r.bc);
            done+=r.bc;if(r.a||r.bc<amount)break;
        }
        put(c,done);j->result=n?r.a:0;return;
    case 2:
        if(!byte(b)||!string(j,0,0,128))return;r.h=b;r.bc=ADDRESS(buffers[0]);break;
    case 3:case 26:
        if(!byte(a))return;if(op==26)r.bc=a<<8;else r.h=a;break;
    case 4:case 12:case 21:case 22:
        n=op==4?29:op==12?17:8;at=op==4||op==12?b:a;
        if((op==4||op==12)&&!byte(a))return;
        if(n>j->sizes[op==4||op==12?1:0]||!span(j,at,n))return;
        r.h=a;r.de=ADDRESS(buffers[0]);
        if(op==21)memcpy(buffers[0],PTR(at),n);
        zs_invoke(&r,op);j->result=r.a;if(!r.a&&op!=21)memcpy(PTR(at),buffers[0],n);return;
    case 5:
        if(j->sizes[1]<29||!span(j,b,29)||!string(j,0,0,128))return;
        r.bc=ADDRESS(buffers[0]);r.de=ADDRESS(buffers[1]);
        zs_invoke(&r,op);j->result=r.a;if(!r.a)memcpy(PTR(b),buffers[1],29);return;
    case 6:
        if(!byte(a)||c>2||!span(j,b,4))return;
        r.h=a;r.a=c;r.de=word(b);r.bc=word(b+2);zs_invoke(&r,op);j->result=r.a;
        if(!r.a){put(b,r.de);put(b+2,r.bc);}return;
    case 7:
        if(!byte(a)||!byte(b)||!span(j,d,2))return;
        r.h=a;r.bc=b;r.de=c;zs_invoke(&r,op);j->result=r.a;if(!r.a)put(d,r.de);return;
    case 8:case 9:case 11:case 13:
        if(!string(j,0,0,128))return;r.de=ADDRESS(buffers[0]);break;
    case 10:
        if(!span(j,a,j->sizes[0]+1))return;
        memset(buffers[0],0xff,128);r.de=ADDRESS(buffers[0]);zs_invoke(&r,op);j->result=r.a;
        if(!r.a){for(n=0;n<128&&buffers[0][n];++n){}if(n==128||n>j->sizes[0])j->result=ERR_INVALID_PARAMETER;
            else {PTR(a)[0]=n;memcpy(PTR(a)+1,buffers[0],n);}}return;
    case 14:
        if(!byte(a)||!byte(b)||!byte(c))return;r.h=a;r.de=(b<<8)|c;break;
    case 15:
        /* Compiler routes normal EXIT through the runtime's terminal owner. */
        return;
    case 16:
        if(a>1||!span(j,d,1)||!string(j,1,0,128)||!string(j,2,1,128))return;
        r.h=a;r.bc=ADDRESS(buffers[0]);r.de=ADDRESS(buffers[1]);
        zs_invoke(&r,op);j->result=r.a;if(!r.a)PTR(d)[0]=r.de>>8;return;
    case 17:case 24:
        if(!byte(a)||!byte(b))return;r.h=a;r.de=b;break;
    case 18:r.de=a;break;
    case 19:if(!byte(a))return;r.h=a;r.de=b;break;
    case 20:
        if(!byte(a)||!span(j,b,2))return;r.h=a;zs_invoke(&r,op);j->result=r.a;if(!r.a)put(b,r.de);return;
    case 25:
        if(!span(j,a,1))return;zs_invoke(&r,op);j->result=r.a;if(!r.a)PTR(a)[0]=r.bc>>8;return;
    default: return; /* Unsupported calls, including direct page mapping (23). */
    }
    zs_invoke(&r,op);j->result=(op==2||op==11)?(int16_t)(int8_t)r.a:r.a;
}

/* Borrow existing private runtime data only while Pascal is suspended. Saving
 * it on this invocation's guarded page3 stack preserves buffered input and
 * numeric state without adding a permanently reserved memory page. */
void zs_dispatch(ZOSJob *j) ZS_CALL {
    uint8_t saved[256];
    if(j->mode==2){normal(j);return;}
    buffers[0]=PTR(j->scratch);buffers[1]=PTR(j->scratch)+128;
    if(!ready(0,128)||!ready(1,128)){j->result=j->call==2||j->call==11?(uint16_t)-(int16_t)ERR_INVALID_PARAMETER:ERR_INVALID_PARAMETER;return;}
    memcpy(saved,buffers[0],256);normal(j);memcpy(buffers[0],saved,256);
}
