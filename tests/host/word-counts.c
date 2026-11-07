/* Execute the shipped memory primitives with unsigned16 count boundaries. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hw/z80.h"
#include "word-counts-runtime.h"
static uint8_t memory[MEM_SPACE_SIZE];static z80 cpu;static device_t ram;static unsigned checks;
void vtimer_tick(uint64_t ignored){(void)ignored;}
uint64_t vtimer_stall_bus(uint64_t ignored,const bool *requested){(void)ignored;(void)requested;return 0;}
int mmu_init(mmu_t *m){memset(m,0,sizeof(*m));return 0;}
static uint8_t read_byte(device_t *d,uint32_t at){(void)d;assert(at<sizeof(memory));return memory[at];}
static void write_byte(device_t *d,uint32_t at,uint8_t value){(void)d;assert(at<sizeof(memory));memory[at]=value;}
static void check(int ok){++checks;assert(ok);}
static void prepare(uint16_t entry,uint16_t count){
    unsigned i;memset(memory,0x6d,sizeof(memory));memcpy(memory+0x4000,runtime_image,sizeof(runtime_image));z80_reset(&cpu);
    for(i=0;i<4;++i){cpu.mmu.pages[i]=i;cpu.mmu.vpages[i].dev=&ram;cpu.mmu.vpages[i].base_phys=0;}
    cpu.pc=entry;cpu.sp=0x3000;memory[0x3000]=0;memory[0x3001]=0x20;cpu.b=count>>8;cpu.c=(uint8_t)count;
}
static void run(void){unsigned i;for(i=0;i<2000000&&cpu.pc!=0x2000;++i)z80_step(&cpu);check(cpu.pc==0x2000);check(cpu.sp==0x3002);check(!cpu.b&&!cpu.c);}
int main(void){
    static const uint16_t counts[]={0,1,32767,32768};unsigned n,i;
    device_init_mem(&ram,"flat RAM",read_byte,write_byte,sizeof(memory));memset(&cpu,0,sizeof(cpu));
    for(n=0;n<4;++n){
        prepare(0x4130,counts[n]);cpu.h=0x80;cpu.l=0;cpu.a=0x5a;run();
        for(i=0;i<32768;++i)check(memory[0x8000+i]==(i<counts[n]?0x5a:0x6d));
        check(memory[0x7fff]==0x6d&&memory[0]==0x6d);
    }
    prepare(0x4102,32768);cpu.h=0x7f;cpu.l=0xff;cpu.d=0x80;cpu.e=0;
    for(i=0;i<32768;++i)memory[0x7fff+i]=(uint8_t)(i*17);
    run();for(i=0;i<32768;++i)check(memory[0x8000+i]==(uint8_t)(i*17));check(memory[0x7ffe]==0x6d);
    prepare(0x4102,0);cpu.h=cpu.d=0x80;cpu.l=0;cpu.e=1;run();
    for(i=0;i<32768;++i)check(memory[0x8000+i]==0x6d);
    prepare(0x4102,65535);cpu.h=cpu.l=cpu.d=cpu.e=0;run();
    for(i=0;i<32768;++i)check(memory[0x8000+i]==0x6d);
    printf("%u unsigned memory-count assertions passed\n",checks);return 0;
}
