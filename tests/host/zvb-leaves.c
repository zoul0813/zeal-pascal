/* Independent mapping/I/O oracle, running bytes extracted from Pascal sources.
 * This is not a replacement for native whole-unit hardware acceptance. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hw/z80.h"
#include "zvb-leaves.h"
static uint8_t memory[MEM_SPACE_SIZE],registers[8][16],selected;
static uint32_t crc_value,crc_bytes;static unsigned assertions;
static z80 cpu;static device_t ram,io;
static uint32_t watched,writes[4];static unsigned watched_size,write_count;
void vtimer_tick(uint64_t ignored){(void)ignored;}
uint64_t vtimer_stall_bus(uint64_t ignored,const bool *requested){(void)ignored;(void)requested;return 0;}
int mmu_init(mmu_t *m){memset(m,0,sizeof(*m));return 0;}
static void check(int value){++assertions;assert(value);}
static uint8_t rd(device_t *d,uint32_t at){(void)d;assert(at<sizeof(memory));return memory[at];}
static void wr(device_t *d,uint32_t at,uint8_t value){(void)d;assert(at<sizeof(memory));memory[at]=value;
    if(watched_size&&at>=watched&&at<watched+watched_size){
        if(write_count<4)writes[write_count]=at;++write_count;
    }
}
static uint8_t in(device_t *d,uint32_t at){
    if(at>=0xf0&&at<=0xf3)return cpu.mmu.pages[d->io_region.upper_addr>>6];
    if(at==0x8e)return selected;
    if(at>=0xa0&&at<=0xaf){
        if(selected==2&&at>=0xa4&&at<=0xa7)return (uint8_t)((~crc_value)>>(8*(at-0xa4)));
        return registers[selected][at&15];
    }return 0;
}
static void out(device_t *d,uint32_t at,uint8_t value){
    unsigned i;(void)d;
    if(at>=0xf0&&at<=0xf3){cpu.mmu.pages[at&3]=value;return;}
    if(at==0x8e){assert(value<8);selected=value;return;}
    if(at>=0xa0&&at<=0xaf){
        if(selected==2&&at==0xa1){crc_value^=value;for(i=0;i<8;++i)crc_value=(crc_value>>1)^((crc_value&1)?0xedb88320u:0);++crc_bytes;}
        else registers[selected][at&15]=value;
    }
}
static void put(uint16_t at,uint16_t v){memory[at]=(uint8_t)v;memory[at+1]=(uint8_t)(v>>8);}
static uint32_t get32(uint16_t at){uint32_t v=0;unsigned i;for(i=0;i<4;++i)v|=(uint32_t)memory[at+i]<<(8*i);return v;}
static void prepare(const uint8_t *code,unsigned size,uint8_t enabled){
    unsigned i;z80_reset(&cpu);
    cpu.pc=0x8000;cpu.sp=0xdfe0;cpu.ix=0xe000;cpu.iy=0x1234;cpu.i=0x73;
    cpu.iff1=cpu.iff2=enabled;
    for(i=0;i<4;++i){cpu.mmu.pages[i]=i?i:17;cpu.mmu.vpages[i].dev=&ram;cpu.mmu.vpages[i].base_phys=0;}
    memcpy(memory+0x8000,code,size);memory[0x8000+size]=0xc9;put(0xdfe0,0x7000);
    selected=5;
}
static void execute(uint8_t enabled){
    unsigned i;for(i=0;i<1000000&&cpu.pc!=0x7000;++i)z80_step(&cpu);
    check(cpu.pc==0x7000);check(cpu.sp==0xdfe2);check(cpu.ix==0xe000&&cpu.iy==0x1234);
    check(cpu.iff1==enabled&&cpu.iff2==enabled);
    check(cpu.mmu.pages[0]==17&&cpu.mmu.pages[1]==1&&cpu.mmu.pages[2]==2&&cpu.mmu.pages[3]==3);
}
int main(void){
    unsigned f,irq,o,n,a,i;uint16_t counts[]={0,1,31,512,16391},offsets[]={0,16377};uint8_t adds[]={0,32,240};
    device_init_mem(&ram,"physical memory",rd,wr,sizeof(memory));device_init_io(&io,"mapped ports",in,out,256);
    memset(&cpu,0,sizeof(cpu));for(i=0;i<256;++i){cpu.mmu.io_mapping[i].dev=&io;cpu.mmu.io_mapping[i].page_from=0;}
    for(f=0;f<4;++f)for(irq=0;irq<2;++irq)for(o=0;o<2;++o)for(n=0;n<5;++n)for(a=0;a<3;++a){
        uint16_t size=counts[n],offset=offsets[o];uint32_t target=68u*16384+offset;
        prepare(transfer_code,sizeof(transfer_code),irq);
        memset(memory+target-1,0x6d,size+2);
        for(i=0;i<size;++i)memory[0x9000+i]=(uint8_t)(i*17);
        put(0xe004,f);put(0xe006,adds[a]);put(0xe008,size);put(0xe00a,0x9000);put(0xe00c,offset);put(0xe00e,68);
        execute(irq);
        check(memory[target-1]==0x6d&&memory[target+size]==0x6d);
        for(i=0;i<size;++i){uint8_t v=(uint8_t)(i*17),want=(f&2)?adds[a]:((f&1)&&!v)?0:(uint8_t)(v+adds[a]);check(memory[target+i]==want);}
    }
    for(irq=0;irq<2;++irq)for(o=0;o<2;++o)for(n=0;n<5;++n){
        uint16_t size=counts[n],offset=offsets[o];uint32_t target=68u*16384+offset;
        prepare(transfer_code,sizeof(transfer_code),irq);
        memset(memory+0x9000-1,0x6d,size+2);
        for(i=0;i<size;++i)memory[target+i]=(uint8_t)(i*13);
        put(0xe004,8);put(0xe006,0);put(0xe008,size);put(0xe00a,0x9000);put(0xe00c,offset);put(0xe00e,68);
        execute(irq);
        check(memory[0x8fff]==0x6d&&memory[0x9000+size]==0x6d);
        for(i=0;i<size;++i){check(memory[0x9000+i]==(uint8_t)(i*13));check(memory[target+i]==(uint8_t)(i*13));}
    }
    for(irq=0;irq<2;++irq)for(a=0;a<2;++a)for(o=0;o<2;++o){
        uint16_t offset=o?16383:0x2804;uint32_t target=64u*16384+offset;
        prepare(transfer_code,sizeof(transfer_code),irq);
        memory[target-1]=memory[target+2]=0x6d;
        memory[target]=0x9a;memory[target+1]=0xbc;
        memory[0x9000]=0x11;memory[0x9001]=0x22;
        watched=target;watched_size=2;write_count=0;
        put(0xe004,4);put(0xe006,a);put(0xe008,2);put(0xe00a,0x9000);put(0xe00c,offset);put(0xe00e,64);
        execute(irq);
        check(write_count==2&&writes[0]==target&&writes[1]==target+1);
        check(memory[target-1]==0x6d&&memory[target+2]==0x6d);
        check(memory[target]==((offset&1)==a?0x11:0x9a));
        check(memory[target+1]==(((offset+1)&1)==a?0x22:0xbc));
        watched_size=0;
    }
    for(irq=0;irq<2;++irq){
        prepare(write_code,sizeof(write_code),irq);put(0xe004,77);put(0xe006,0xa5);put(0xe008,3);execute(irq);check(selected==3&&registers[3][5]==77);
        registers[4][2]=0x42;memory[0xc100]=0;
        prepare(read_code,sizeof(read_code),irq);put(0xe004,0xc100);put(0xe006,0xa2);put(0xe008,4);execute(irq);check(selected==4&&memory[0xc100]==0x42);
        crc_value=0xffffffffu;crc_bytes=0;memcpy(memory+0x9000,"123456789",9);
        prepare(crc_code,sizeof(crc_code),irq);put(0xe004,0xc100);put(0xe006,4);put(0xe008,0x9000);execute(irq);
        prepare(crc_code,sizeof(crc_code),irq);put(0xe004,0xc100);put(0xe006,5);put(0xe008,0x9004);execute(irq);
        check(selected==2&&crc_bytes==9&&get32(0xc100)==0xcbf43926u);
        prepare(crc_code,sizeof(crc_code),irq);put(0xe004,0xc100);put(0xe006,0);put(0xe008,0x9000);execute(irq);
        check(crc_bytes==9&&get32(0xc100)==0xcbf43926u);
    }
    printf("%u ZVB Inline leaf assertions passed\n",assertions);return 0;
}
