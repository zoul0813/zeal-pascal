#include "runtime/process/banks.h"
#include "runtime/process/chain.h"
#include <zos_vfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static OverlayBanks banks;
static uint8_t pages[64][16384],owned[64],directory[1024],kinds[16];
static FILE *files[16];
static const char *folder,*override;
static char cwd[128]="H:/app",opened[16][128];
static uint16_t directory_bytes;
uint8_t chain_buffer[128],chain_store;
static unsigned checks,opens,reads,seeks,allocations,closes,stats,read_bytes,page_ios;
static unsigned bad_read,zero_read,large_read,bad_alloc,bad_close,bad_seek,bad_stat,short_read,quota=32;
static unsigned returned[256];
static int bad_free=-1;
#define check(x) do{++checks;if(!(x)){fprintf(stderr,"bank check %u: %s (error %u)\n",__LINE__,#x,banks.error);abort();}}while(0)
static unsigned used(void) {unsigned i,n=0;for(i=32;i<64;++i)n+=owned[i]!=0;return n;}
static uint8_t slot(void) {uint8_t i;for(i=2;i<16;++i)if(!kinds[i])return i;return 255;}
zos_err_t palloc(uint8_t *page) {
    uint8_t i;if(++allocations==bad_alloc||used()>=quota){*page=31;return ERR_NO_MORE_MEMORY;}
    for(i=32;i<64;++i)if(!owned[i]){owned[i]=1;*page=i;memset(pages[i],0xa5,16384);return 0;}
    return ERR_NO_MORE_MEMORY;
}
zos_err_t pfree(uint8_t page) {
    check(page>=32&&page<64&&owned[page]);if(page==bad_free)return ERR_FAILURE;
    owned[page]=0;return 0;
}
zos_dev_t open(const char *path,uint8_t flags) {
    char full[1400];uint8_t fd=slot();++opens;
    check(flags==O_RDONLY&&path[1]==':'&&path[2]=='/');
    if(fd==255)return -ERR_CANNOT_REGISTER_MORE;
    snprintf(full,sizeof(full),"%s/%c%s",folder,path[0],path+2);
    files[fd]=fopen(override?override:full,"rb");if(!files[fd])return -ERR_NO_SUCH_ENTRY;
    kinds[fd]=1;strcpy(opened[fd],path);return fd;
}
zos_dev_t opendir(const char *path) {
    uint8_t fd=slot();++opens;
    if(strcmp(path,"H:/")&&strcmp(path,"T:/"))return -ERR_NO_SUCH_ENTRY;
    if(fd==255)return -ERR_CANNOT_REGISTER_MORE;
    kinds[fd]=2;strcpy(opened[fd],path);return fd;
}
zos_err_t curdir(char *path) {strcpy(path,cwd);return 0;}
zos_err_t dstat(zos_dev_t fd,zos_stat_t *info) {
    long size;check(fd>=2&&fd<16&&kinds[fd]==1);
    if(++stats==bad_stat)return ERR_FAILURE;
    check(!fseek(files[fd],0,SEEK_END));size=ftell(files[fd]);check(size>=0&&!fseek(files[fd],0,SEEK_SET));
    memset(info,0,sizeof(*info));info->s_size=(uint32_t)size;info->s_flags=1;return 0;
}
zos_err_t seek(zos_dev_t fd,int32_t *at,zos_whence_t whence) {
    check(fd>=2&&fd<16&&kinds[fd]==1&&whence==SEEK_SET&&*at>=0);
    if(++seeks==bad_seek)return ERR_FAILURE;
    return fseek(files[fd],(long)*at,SEEK_SET)?ERR_FAILURE:0;
}
zos_err_t read(zos_dev_t fd,void *out,uint16_t *bytes) {
    check(fd>=2&&fd<16&&kinds[fd]==1);if(++reads==bad_read)return ERR_FAILURE;
    if(reads==zero_read){*bytes=0;return 0;}
    if(reads==large_read){++*bytes;return 0;}
    if(short_read&&*bytes>short_read)*bytes=(uint16_t)short_read;
    *bytes=(uint16_t)fread(out,1,*bytes,files[fd]);read_bytes+=*bytes;
    if(reads<256)returned[reads]=*bytes;
    return ferror(files[fd])?ERR_FAILURE:0;
}
zos_err_t close(zos_dev_t fd) {
    check(fd>=2&&fd<16&&kinds[fd]);if(kinds[fd]==1)check(!fclose(files[fd]));
    files[fd]=0;kinds[fd]=0;return ++closes==bad_close?ERR_FAILURE:0;
}
void chain_page_io(uint8_t page,uint16_t at,uint16_t bytes) {
    check(page>=32&&page<64&&owned[page]&&bytes&&bytes<=128&&(uint32_t)at+bytes<=16384);
    ++page_ios;
    if(chain_store)memcpy(pages[page]+at,chain_buffer,bytes);else memcpy(chain_buffer,pages[page]+at,bytes);
}
static void clean(void) {
    uint8_t i;check(overlay_banks_close(&banks));check(!used());
    for(i=2;i<16;++i)check(!kinds[i]);
}
static void reset(void) {
    clean();opens=reads=seeks=allocations=closes=stats=read_bytes=page_ios=0;
    bad_read=zero_read=large_read=bad_alloc=bad_close=bad_seek=bad_stat=short_read=0;
    bad_free=-1;quota=32;override=0;strcpy(cwd,"H:/app");
}
static void metadata(const char *name) {
    char path[1400];FILE *input;
    snprintf(path,sizeof(path),"%s/%s",folder,name);input=fopen(path,"rb");check(input!=0);
    directory_bytes=(uint16_t)fread(directory,1,sizeof(directory),input);check(!ferror(input)&&!fclose(input));
}
static uint8_t bind(void) {return overlay_banks_bind(&banks,"BANK",directory,directory_bytes);}
static uint8_t enter(uint16_t member) {return overlay_banks_enter(&banks,member);}
static void inactive(uint16_t member) {check(overlay_banks_leave(&banks,member));}
int main(int argc,char **argv) {
    uint8_t a,c,p,d;unsigned i,total_reads,total_seeks,events[256],io;char path[1400];
    check(argc>=2);folder=argv[1];overlay_banks_init(&banks);metadata("directory.bin");
    check(overlay_banks_bind(&banks,"ZP0000/P",directory,directory_bytes));check(!opens&&!allocations);check(overlay_banks_close(&banks));
    {const char *bad[]={"/P","../P","A/../P","A/./P","P/","P//Q","H:/P","A\\P"};
     for(i=0;i<sizeof(bad)/sizeof(*bad);++i){check(!overlay_banks_bind(&banks,bad[i],directory,directory_bytes));check(banks.error==ERR_INVALID_PATH);check(overlay_banks_close(&banks));}}
    check(bind()&&!opens&&!allocations&&!used());check(banks.count==4&&banks.groups==2);
    a=enter(0);check(a!=255&&used()==1&&pages[a][0]==0x31&&read_bytes==80+16384);
    total_reads=reads;total_seeks=seeks;memcpy(events,returned,sizeof(events));
    check(total_reads<256);check(banks.files[1]<0&&banks.owned_count==1);
    check(banks.drives[0]=='H');inactive(0);io=reads+opens+allocations+seeks+closes+stats+page_ios;
    pages[a][0]=0x91;check(enter(0)==a&&pages[a][0]==0x91);inactive(0);
    check(io==reads+opens+allocations+seeks+closes+stats+page_ios);
    check(overlay_banks_set_drive(&banks,20));
    p=enter(1);d=p;check(p!=255&&p!=a&&pages[p][0]==0x32&&banks.drives[0]=='H');
    check(!strcmp(opened[banks.files[0]],"H:/app/BANK.000"));
    c=enter(2);check(c!=255&&c!=a&&c!=d&&pages[c][0]==0x33&&banks.drives[1]=='T');
    check(!strcmp(opened[banks.files[1]],"T:/app/BANK.001"));
    pages[c][0]=0x93;inactive(2);inactive(1);
    io=reads+opens+allocations+seeks+closes+stats+page_ios;
    check(enter(1)==d);check(enter(2)==c&&pages[c][0]==0x93);inactive(2);inactive(1);
    check(enter(0)==a&&pages[a][0]==0x91);inactive(0);
    check(enter(1)==d&&banks.selected[1]==65535u);
    check(enter(2)==c&&pages[c][0]==0x93);inactive(2);inactive(1);
    check(io==reads+opens+allocations+seeks+closes+stats+page_ios);reset();
    /* Drive0 follows the logged drive at FIRST USE, not at OvrDrive(0).
     * A later explicit selection cannot rebind the already opened group. */
    check(bind()&&overlay_banks_set_drive(&banks,0));strcpy(cwd,"T:/app");
    check(enter(0)!=255&&banks.drives[0]=='T');inactive(0);
    strcpy(cwd,"H:/app");check(overlay_banks_set_drive(&banks,8));
    check(enter(1)!=255&&banks.drives[0]=='T');inactive(1);reset();
    /* Merely binding a valid program must not touch even a missing group. */
    check(overlay_banks_bind(&banks,"MISSING",directory,directory_bytes));
    check(!opens&&!allocations&&enter(0)==255&&!allocations&&banks.selected[0]==65535u);reset();
    check(bind());a=enter(0);check(a!=255);check(enter(1)==255&&banks.selected[0]==0&&pages[a][0]==0x31);reset();
    check(bind());a=enter(0);check(a!=255);inactive(0);pages[a][0]=0x91;
    bad_read=reads+1;check(enter(1)==255&&banks.selected[0]==0&&pages[a][0]==0x91&&banks.loaded_member[1]==65535u);reset();
    for(i=1;i<=total_reads;++i) {
        bad_read=i;check(bind()&&enter(0)==255&&banks.error);reset();
        if(events[i]){zero_read=i;check(bind()&&enter(0)==255);reset();}
        large_read=i;check(bind()&&enter(0)==255);reset();
    }
    for(i=1;i<=total_seeks;++i){bad_seek=i;check(bind()&&enter(0)==255);reset();}
    for(i=1;i<=1;++i){bad_alloc=i;check(bind()&&enter(0)==255);reset();}
    bad_stat=1;check(bind()&&enter(0)==255&&!allocations);reset();
    short_read=3;check(bind()&&enter(0)!=255);inactive(0);reset();
    check(bind()&&enter(0)!=255);inactive(0);bad_close=1;
    check(!overlay_banks_close(&banks)&&!banks.ready&&!used());reset();
    check(bind()&&enter(0)!=255);inactive(0);bad_free=banks.owned[0];
    check(!overlay_banks_close(&banks)&&owned[bad_free]);bad_free=-1;reset();
    check(bind());check(!overlay_banks_set_drive(&banks,27)&&!banks.drive&&!allocations);reset();
    check(bind());check(!overlay_banks_set_drive(&banks,26)&&!banks.drive&&!allocations);reset();
    check(bind());bad_close=1;check(!overlay_banks_set_drive(&banks,20)&&!banks.drive);reset();
    /* Exhaustion fails instead of recycling any loaded executable. Warm
     * reselection before exhaustion changes neither bytes nor I/O/MMU traffic. */
    metadata("big-directory.bin");quota=3;snprintf(path,sizeof(path),"%s/big.000",folder);override=path;
    check(bind()&&banks.count==40&&!allocations&&!opens);
    for(i=0;i<3;++i){p=enter((uint16_t)i);check(p!=255&&pages[p][0]==(uint8_t)(0x31+i));
        pages[p][0]=(uint8_t)(0x91+i);inactive((uint16_t)i);}
    check(used()==3&&opens==1);strcpy(cwd,"T:/elsewhere");
    io=reads+opens+allocations+seeks+closes+stats+page_ios;
    for(i=0;i<3;++i){p=enter((uint16_t)i);check(p==banks.owned[i]&&pages[p][0]==(uint8_t)(0x91+i));inactive((uint16_t)i);}
    check(io==reads+opens+allocations+seeks+closes+stats+page_ios);
    check(enter(3)==255&&banks.error==ERR_NO_MORE_MEMORY&&used()==3&&banks.owned_count==3);
    for(i=0;i<3;++i)check(banks.loaded_member[i]==i&&pages[banks.owned[i]][0]==(uint8_t)(0x91+i));
    reset();metadata("directory.bin");
    for(i=2;i<(unsigned)argc;++i) {
        const char *suffix=strrchr(argv[i],'.');
        if(suffix&&!strcmp(suffix,".dir")) {
            FILE *input=fopen(argv[i],"rb");check(input!=0);
            directory_bytes=(uint16_t)fread(directory,1,sizeof(directory),input);check(!fclose(input));
            check(!bind()&&!opens&&!allocations);reset();metadata("directory.bin");
        } else {override=argv[i];check(bind()&&enter(0)==255&&banks.error);reset();}
    }
    printf("PASS: %u load-once overlay/persistence/drive checks; %u read and %u seek fault positions\n",checks,total_reads,total_seeks);
    return 0;
}
