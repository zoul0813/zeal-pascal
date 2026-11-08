/* Independent mapping/I/O oracle, running bytes extracted from Pascal sources.
 * This is not a replacement for native whole-unit hardware acceptance. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "hw/z80.h"
#include "zvb-leaves.h"
static uint8_t memory[MEM_SPACE_SIZE],registers[8][16],selected;
static unsigned port_writes[256];
static unsigned sampling,sample_count,sample_depth,sample_polls,sample_throttles,sample_drains;static uint8_t sample_data[1024];
static uint32_t crc_value,crc_bytes;static unsigned assertions;
static z80 cpu;static device_t ram,io;
static unsigned long leaf_cycles;
static unsigned watch_vram_reads,vram_reads;
static uint32_t watched,writes[4];static unsigned watched_size,write_count;
void vtimer_tick(uint64_t ignored){(void)ignored;}
uint64_t vtimer_stall_bus(uint64_t ignored,const bool *requested){(void)ignored;(void)requested;return 0;}
int mmu_init(mmu_t *m){memset(m,0,sizeof(*m));return 0;}
static void check(int value){++assertions;assert(value);}
static uint8_t rd(device_t *d,uint32_t at){(void)d;assert(at<sizeof(memory));if(watch_vram_reads&&at>=64u*16384&&at<72u*16384)++vram_reads;return memory[at];}
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
        if(selected==3&&at==0xa2&&sampling){
            /* SDK include/zvb_hardware.h: FULL_BIT=6, READY_BIT=7.
             * Four-byte FIFO; one sample drains every third status poll. */
            if(++sample_polls%3==0&&sample_depth){--sample_depth;++sample_drains;}
            if(sample_depth==4)++sample_throttles;
            return registers[3][2]|(sample_depth==4?0x40:0)|(sample_depth?0:0x80);
        }
        return registers[selected][at&15];
    }return 0;
}
static void out(device_t *d,uint32_t at,uint8_t value){
    unsigned i;(void)d;++port_writes[at&255];
    if(at>=0xf0&&at<=0xf3){cpu.mmu.pages[at&3]=value;return;}
    if(at==0x8e){assert(value<8);selected=value;return;}
    if(at>=0xa0&&at<=0xaf){
        if(selected==2&&at==0xa1){crc_value^=value;for(i=0;i<8;++i)crc_value=(crc_value>>1)^((crc_value&1)?0xedb88320u:0);++crc_bytes;}
        else {registers[selected][at&15]=value;
            if(selected==3&&at==0xa0&&sampling){assert(sample_count<sizeof(sample_data)&&sample_depth<4);++sample_depth;sample_data[sample_count++]=value;}
        }
    }
}
static void put(uint16_t at,uint16_t v){memory[at]=(uint8_t)v;memory[at+1]=(uint8_t)(v>>8);}
static uint32_t get32(uint16_t at){uint32_t v=0;unsigned i;for(i=0;i<4;++i)v|=(uint32_t)memory[at+i]<<(8*i);return v;}
static void prepare(const uint8_t *code,unsigned size,uint8_t enabled){
    unsigned i;z80_reset(&cpu);memset(port_writes,0,sizeof(port_writes));
    cpu.pc=0x8000;cpu.sp=0xdfe0;cpu.ix=0xe000;cpu.iy=0x1234;cpu.i=0x73;
    cpu.iff1=cpu.iff2=enabled;
    for(i=0;i<4;++i){cpu.mmu.pages[i]=i?i:17;cpu.mmu.vpages[i].dev=&ram;cpu.mmu.vpages[i].base_phys=0;}
    memcpy(memory+0x8000,code,size);memory[0x8000+size]=0xc9;put(0xdfe0,0x7000);
    selected=5;
}
static void execute(uint8_t enabled){
    unsigned i;unsigned long start=cpu.cyc;for(i=0;i<1000000&&cpu.pc!=0x7000;++i)z80_step(&cpu);leaf_cycles=cpu.cyc-start;
    check(cpu.pc==0x7000);check(cpu.sp==0xdfe2);check(cpu.ix==0xe000&&cpu.iy==0x1234);
    check(cpu.iff1==enabled&&cpu.iff2==enabled);
    check(cpu.mmu.pages[0]==17&&cpu.mmu.pages[1]==1&&cpu.mmu.pages[2]==2&&cpu.mmu.pages[3]==3);
}
/* Independent whole-output model: no circular/chunk implementation in oracle. */
static unsigned last_decode_bytes;
static unsigned model(const uint8_t *source,unsigned size,unsigned kind,unsigned budget,uint8_t *output,unsigned *length){
    unsigned at=0,used=0;
    while(at<size){
        unsigned command=source[at++],count,offset=0,run=0,value=0;
        if(kind==16){count=(command&127)+1;run=command<128?1:0;
            if(!run){if(at==size)goto invalid;value=source[at++];}}
        else if(command<64){count=1;value=command;}
        else if(command<128){count=(command&63)+1;run=1;}
        else {run=2;
            if(command<192){count=((command>>4)&3)+3;offset=(command&15)+1;}
            else{count=(command&63)+4;if(at==size)goto invalid;offset=source[at++]+1;}
            if(offset>used||offset>256)goto invalid;
        }
        if((run==1&&count>size-at)||count>budget-used)goto invalid;
        while(count--){output[used]=run==1?source[at++]:run==2?output[used-offset]:value;++used;}
    }
    *length=used;return 1;
invalid:*length=used;return 0;
}
static void decoder_case(const uint8_t *source,unsigned size,unsigned kind,unsigned from,unsigned irq,unsigned high){
    static uint8_t expected[65536],actual[65536];
    unsigned length,valid=model(source,size,kind,65536-from,expected,&length),used=0,done=0,steps=0,i;
    uint16_t address=high?(uint16_t)(65536-size):0xa000;
    memset(memory+0xc200,0,19);put(0xc200,address);put(0xc202,size);put(0xc204,(uint16_t)(0-from));
    memory[0xc20f]=kind;memory[0xc210]=!from;
    if(size)memcpy(memory+address,source,size);
    memset(memory+0x9102,0x6d,258);
    while(!done){
        unsigned count,error;
        prepare(decode_code,sizeof(decode_code),irq);put(0xe004,0x9103);put(0xe006,0xc200);put(0xc206,0);
        execute(irq);count=memory[0xc206]+256*memory[0xc207];error=memory[0xc212];done=memory[0xc211];
        check(count<=256&&used+count<=65536);check(error==0||error==2);
        check(memory[0x9102]==0x6d&&memory[0x9203]==0x6d);
        if(error){check(!valid&&done);break;}
        memcpy(actual+used,memory+0x9103,count);used+=count;
        check(++steps<=257);check(count||done);check(selected==5);
    }
    check(used==(valid?length:length/256*256));
    for(i=0;i<used;++i)check(actual[i]==expected[i]);
    if(size)check(!memcmp(memory+address,source,size));last_decode_bytes=used;
}
static void decoder_tests(void){
    unsigned random_long[2]={0,0};
    unsigned i,n,irq,seed=0x7134;uint8_t source[2048];
    for(irq=0;irq<2;++irq){
        for(n=0;n<512;++n){source[2*n]=255;source[2*n+1]=(uint8_t)n;}
        decoder_case(source,1024,16,0,irq,1);
        source[1024]=255;source[1025]=17;decoder_case(source,1026,16,0,irq,0); /* output exceeds 64K */
        for(n=0;n<4;++n){source[n*65]=127;for(i=0;i<64;++i)source[n*65+1+i]=(uint8_t)(n*64+i);}
        source[260]=255;source[261]=255;source[262]=255;source[263]=0;
        decoder_case(source,264,17,0,irq,1); /* offset256 and overlapping offset1 */
        /* Deterministic mid-token chunk resumes: all three run kinds. */
        for(n=0;n<4;++n){
            unsigned at=0,total=n==3?476:220,remaining=total,offset=n==0?1:n==1?17:n==2?5:256;
            while(remaining){unsigned count=remaining>64?64:remaining;
                source[at++]=(uint8_t)(0x40+count-1);
                for(i=0;i<count;++i)source[at++]=(uint8_t)(total-remaining+i);
                remaining-=count;
            }
            source[at++]=255;source[at++]=(uint8_t)(offset-1);
            decoder_case(source,at,17,0,irq,1);
        }
        source[0]=0xe3;source[1]=7;source[2]=0xff;source[3]=9;source[4]=0xff;source[5]=12;
        decoder_case(source,6,16,0,irq,0); /* repeat straddles at228 */
        source[0]=0xff;source[1]=7;source[2]=0xc7;source[3]=9;source[4]=0x7f;
        for(i=0;i<128;++i)source[5+i]=(uint8_t)i;
        decoder_case(source,133,16,0,irq,0); /* RLE literal starts at200 */
        {unsigned at=0;for(n=0;n<4;++n){unsigned count=n==3?28:64;
            source[at++]=(uint8_t)(0x40+count-1);for(i=0;i<count;++i)source[at++]=(uint8_t)i;}
         source[at++]=0x7f;for(i=0;i<64;++i)source[at++]=(uint8_t)i;
         decoder_case(source,at,17,0,irq,1); /* exactly enough LZ literal input */}
        for(n=0;n<128;++n){source[0]=0x40+n%64;for(i=1;i<66;++i)source[i]=(uint8_t)i;
            decoder_case(source,n%66,17,65536-(n+1),irq,0);}
        for(n=0;n<2048;++n){unsigned size=1+(n%128),kind=n&1?16:17;
            for(i=0;i<size;++i){seed=seed*1664525u+1013904223u;source[i]=(uint8_t)(seed>>24);}
            decoder_case(source,size,kind,n%3?0:65536-(n%257+1),irq,n&1);
            if(last_decode_bytes>256)++random_long[kind==17];
        }
    }
    printf("Random cases producing >256 bytes: RLE %u, LZ %u; deterministic fixtures cover all pending run kinds\n",random_long[0],random_long[1]);
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
    /* SDK memset_vram writes each byte. Filling device memory must never
     * copy back a previously written byte through a VRAM read. */
    for(irq=0;irq<2;++irq)for(o=0;o<2;++o)for(n=0;n<5;++n)for(f=2;f<4;++f)for(a=0;a<2;++a){
        uint16_t size=counts[n],offset=offsets[o];uint32_t target=68u*16384+offset;
        prepare(transfer_code,sizeof(transfer_code),irq);
        memset(memory+target-1,0x6d,size+2);
        put(0xe004,f);put(0xe006,a?0x12:0);put(0xe008,size);put(0xe00a,0x9000);put(0xe00c,offset);put(0xe00e,68);
        vram_reads=0;watch_vram_reads=1;execute(irq);watch_vram_reads=0;
        check(vram_reads==0);
        for(i=0;i<size;++i)check(memory[target+i]==(a?0x12:0));
        if(size&&(unsigned)offset+size<=16384)check(leaf_cycles<45u*size+1000);
        check(memory[target-1]==0x6d&&memory[target+size]==0x6d);
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
    /* Public Inline bodies: actual Pascal argument slots, direct VRAM writes. */
    for(irq=0;irq<2;++irq)for(o=0;o<2;++o)for(a=0;a<3;++a){
        unsigned x=o?79:0,y=o?39:0,layer=a&1;
        uint32_t target=64u*16384+layer*4096+y*80+x;
        prepare(tile_code,sizeof(tile_code),irq);
        memory[target-1]=memory[target+1]=0x6d;memory[target]=0;
        put(0xe004,y);put(0xe006,x);put(0xe008,layer);put(0xe00a,adds[a]);
        execute(irq);check(memory[target]==adds[a]);check(leaf_cycles<600);
        if(!irq&&!o&&!a)printf("Direct tile leaf: %lu T-states including RET\n",leaf_cycles);
        check(memory[target-1]==0x6d&&memory[target+1]==0x6d);
    }
    for(irq=0;irq<2;++irq)for(o=0;o<128;++o){
        uint32_t base=64u*16384+0x2800+o*8;
        const uint8_t *codes[]={setx_code,sety_code,setxy_code,settile_code,setflags_code};
        unsigned sizes[]={sizeof(setx_code),sizeof(sety_code),sizeof(setxy_code),sizeof(settile_code),sizeof(setflags_code)};
        for(a=0;a<5;++a){
            prepare(codes[a],sizes[a],irq);memset(memory+base,0x6d,8);
            watched=base;watched_size=8;write_count=0;
            put(0xe004,0x1234);put(0xe006,a==2?0x5678:o);put(0xe008,o);
            execute(irq);
            if(a<2){unsigned off=a?0:2;check(write_count==2&&writes[0]==base+off&&writes[1]==base+off+1);
                check(memory[base+off]==0x34&&memory[base+off+1]==0x12);}
            else if(a==2){check(write_count==4&&writes[0]==base+2&&writes[1]==base+3&&writes[2]==base&&writes[3]==base+1);
                check(memory[base]==0x34&&memory[base+1]==0x12&&memory[base+2]==0x78&&memory[base+3]==0x56);}
            else {check(write_count==2&&writes[0]==base+4&&writes[1]==base+5);
                check(memory[base+4]==(a==3?0x34:0x6d)&&memory[base+5]==(a==4?0x34:0x6d));}
            check(memory[base+6]==0x6d&&memory[base+7]==0x6d);watched_size=0;
        }
    }
    for(irq=0;irq<2;++irq)for(a=0;a<5;++a){
        const uint8_t *codes[]={tileload_code,palette_code,palread_code,sprite_code,sprites_code};
        unsigned sizes[]={sizeof(tileload_code),sizeof(palette_code),sizeof(palread_code),sizeof(sprite_code),sizeof(sprites_code)};
        unsigned size=a==0?80:a<3?512:a==3?8:1024;
        uint32_t target=64u*16384+(a==0?4096+39*80:a<3?0xe00:0x2800);
        prepare(codes[a],sizes[a],irq);
        memset(memory+target-1,0x6d,size+2);memset(memory+0x9000-1,0x6d,size+2);
        for(i=0;i<size;++i)memory[(a==2?target:0x9000)+i]=(uint8_t)(i*19);
        if(a==0){put(0xe004,39);put(0xe006,0);put(0xe008,1);put(0xe00a,size);put(0xe00c,0x9000);}
        else if(a<3){put(0xe004,0);put(0xe006,size);put(0xe008,0x9000);}
        else if(a==3){put(0xe004,0x9000);put(0xe006,0);}
        else {put(0xe004,128);put(0xe006,0x9000);put(0xe008,0);}
        execute(irq);
        check(memory[target-1]==0x6d&&memory[target+size]==0x6d);
        check(memory[0x8fff]==0x6d&&memory[0x9000+size]==0x6d);
        for(i=0;i<size;++i)check(memory[(a==2?0x9000:target)+i]==(uint8_t)(i*19));
    }
    for(irq=0;irq<2;++irq){
        static const unsigned counts[]={0,1,255,256,257,512,1024};
        for(n=0;n<sizeof(counts)/sizeof(counts[0]);++n){
            uint32_t expected=0xffffffffu;
            for(i=0;i<counts[n];++i){unsigned bit;uint8_t value=(uint8_t)(i*23);
                memory[0x9000+i]=value;expected^=value;
                for(bit=0;bit<8;++bit)expected=(expected>>1)^((expected&1)?0xedb88320u:0);}
            crc_value=0xffffffffu;crc_bytes=0;
            prepare(crc_code,sizeof(crc_code),irq);put(0xe004,0xc100);put(0xe006,counts[n]);put(0xe008,0x9000);execute(irq);
            check(crc_bytes==counts[n]&&get32(0xc100)==~expected);
            check(port_writes[0x8e]==1&&port_writes[0xa1]==counts[n]);
        }
        registers[3][13]=0x81;
        prepare(hold_code,sizeof(hold_code),irq);put(0xe004,1);put(0xe006,6);execute(irq);check(registers[3][13]==0x87);
        prepare(hold_code,sizeof(hold_code),irq);put(0xe004,0);put(0xe006,2);execute(irq);check(registers[3][13]==0x85);
        prepare(voices_code,sizeof(voices_code),irq);put(0xe004,3);put(0xe006,0x1234);put(0xe008,6);execute(irq);
        check(selected==3&&registers[3][13]==0x85&&registers[3][15]==6&&registers[3][0]==0x34&&registers[3][1]==0x12&&registers[3][2]==3);
        prepare(volume_code,sizeof(volume_code),irq);put(0xe004,2);put(0xe006,5);execute(irq);check(registers[3][15]==5&&registers[3][3]==2);
        prepare(channels_code,sizeof(channels_code),irq);put(0xe004,7);put(0xe006,3);execute(irq);check(registers[3][11]==3&&registers[3][12]==7);
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
    for(irq=0;irq<2;++irq){
        for(a=0;a<3;++a){
            unsigned size=a?300:301;sampling=1;sample_count=sample_depth=sample_polls=sample_throttles=sample_drains=0;
            prepare(samples_code,sizeof(samples_code),irq);
            registers[3][13]=0x85;memory[0xc200]=a;memory[0xc201]=7;
            for(i=0;i<size;++i)memory[0x9000+i]=(uint8_t)(i*23);
            put(0xe004,size);put(0xe006,0x9000);put(0xe008,0xc200);execute(irq);
            check(port_writes[0x8e]==1&&port_writes[0xa0]==size);
            check(sample_throttles>0&&sample_drains==size&&sample_depth==0);
            check(sample_count==size&&registers[3][13]==0x85&&registers[3][15]==128);
            check(registers[3][2]==(a==0?1:a==2?4:0)&&registers[3][1]==7);
            for(i=0;i<size;++i)check(sample_data[i]==(uint8_t)(i*23));sampling=0;
        }
        for(o=0;o<4;++o){
            uint16_t address=o*16384+0x123;uint32_t physical=(o?o:17)*16384+0x123;
            prepare(dma_code,sizeof(dma_code),irq);put(0xe004,address);execute(irq);
            check(port_writes[0x8e]==1&&port_writes[0xa1]==1&&port_writes[0xa2]==1&&port_writes[0xa3]==1&&port_writes[0xa0]==1);
            check(selected==4&&registers[4][0]==0x80&&registers[4][1]==(physical&255)&&registers[4][2]==((physical>>8)&255)&&registers[4][3]==physical>>16);
        }
    }
    decoder_tests();
    printf("%u ZVB Inline leaf assertions passed\n",assertions);return 0;
}
