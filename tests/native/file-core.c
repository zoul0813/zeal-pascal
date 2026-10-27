#include "file_runtime.h"
#include "path.h"
#include <string.h>
#include <zos_vfs.h>
#ifndef __SDCC
#include <stdio.h>
uint8_t zf_user_memory[65536];
#define VM(at) (zf_user_memory+(at))
extern uint8_t model_pages,model_handles,model_close_error,model_read_error,model_write_error;
extern uint8_t model_zero_write,model_short,model_readonly;
extern uint8_t model_curdir_error;
extern uint8_t model_rm_error,model_corrupt_write;
extern uint8_t model_oversize_read,model_oversize_write;
extern uint16_t model_open_calls,model_close_calls,model_read_calls,model_write_calls;
extern uint16_t model_fail_open_at,model_fail_close_at,model_fail_read_at,model_fail_write_at;
extern uint16_t model_zero_read_at;
extern uint8_t model_serial_present,model_serial_attr;
extern uint8_t model_serial_input[256],model_serial_output[512];
extern uint16_t model_serial_size,model_serial_position,model_serial_written;
extern uint16_t model_serial_ioctl_calls,model_serial_fail_ioctl_at,model_serial_seek_calls;
#else
#define VM(at) ((uint8_t *)(at))
extern void test_complete(void);
extern void file_probe(uint8_t cleanup,void *job) ZFILE_CALL;
uint8_t file_probe_before[7],file_probe_after[7],file_probe_ok;
#endif

volatile uint16_t test_passed,test_failed,test_failures[64],test_index;
volatile uint8_t test_done;
static FileJob invocation;
static FileCleanup ending;
static uint8_t saved_token[16];
static void check_at(uint8_t ok,uint16_t line) {
    if(ok) ++test_passed;
    else {
        if(test_index<64) test_failures[test_index++]=line;
        ++test_failed;
#ifndef __SDCC
        printf("FAIL file component line %u\n",line);
#endif
    }
}
#define check(ok) check_at((ok),__LINE__)
static void put(uint16_t at,uint16_t value) {
    VM(at)[0]=(uint8_t)value; VM(at)[1]=(uint8_t)(value>>8);
}
static uint16_t get(uint16_t at) { return (uint16_t)VM(at)[0]|((uint16_t)VM(at)[1]<<8); }
static uint16_t call(uint16_t operation) {
    invocation.operation=operation;
#ifdef __SDCC
    file_probe(0,&invocation);
    check(file_probe_ok&&!memcmp(file_probe_before,file_probe_after,7));
#else
    zf_dispatch(&invocation);
#endif
    return invocation.error;
}
static void defaults(void) {
    memset(&invocation,0,sizeof(invocation));
    invocation.object=0xc000; invocation.descriptor=0xc100; invocation.data=0xc200;
    invocation.code_origin=0x8000; invocation.static_end=0xe000;
    invocation.heap_floor=0xe400; invocation.heap_frontier=0xe400;
    invocation.stack_pointer=0xe700;
    memset(VM(0xc000),0,32); VM(0xc100)[0]=1; put(0xc101,2);
}
static uint16_t named(uint16_t operation,const char *name) {
    uint16_t error;
    invocation.data=0xc140;
    VM(0xc140)[0]=(uint8_t)strlen(name); memcpy(VM(0xc141),name,strlen(name));
    error=call(operation); invocation.data=0xc200; return error;
}
static void assign(const char *name) { check(named(ZF_ASSIGN,name)==0); }
static void clean(uint16_t operation,uint16_t owner,uint16_t end) {
    ending.operation=operation; ending.owner=owner; ending.end=end;
#ifdef __SDCC
    file_probe(1,&ending);
    check(file_probe_ok&&!memcmp(file_probe_before,file_probe_after,7));
#else
    zf_cleanup(&ending);
#endif
    check(ending.error==0);
}
static void seed(const char *name,const uint8_t *data,uint16_t count) {
    int8_t fd=open(name,O_WRONLY|O_CREAT|O_TRUNC);
    uint16_t size=count;
    check(fd>=0);
    if(fd>=0) { check(write(fd,data,&size)==0&&size==count); check(close(fd)==0); }
}
static void basic(void) {
    uint16_t i;
    defaults(); assign("H:/TYPED.DAT");
    check(get(0xc000)==0x465a&&get(0xc006)==0&&VM(0xc008)[0]==1&&get(0xc00a)==2);
    check(call(ZF_REWRITE)==0);
    for(i=1;i<=3;++i) { put(0xc200,i*11); check(call(ZF_WRITE)==0&&invocation.actual==1); }
    check(call(ZF_SIZE)==0&&invocation.result==3);
    check(call(ZF_POSITION)==0&&invocation.result==3);
    invocation.count=1; check(call(ZF_SEEK)==0); put(0xc200,99); check(call(ZF_WRITE)==0);
    check(call(ZF_FLUSH)==0); check(call(ZF_CLOSE)==0); check(call(ZF_RESET)==0);
    for(i=1;i<=3;++i) { check(call(ZF_READ)==0); check(get(0xc200)==(i==2?99:i*11)); }
    check(call(ZF_EOF)==0&&invocation.result==1);
    check(call(ZF_READ)==0x99&&invocation.actual==0);
    invocation.count=4; check(call(ZF_SEEK)==0x91);
    invocation.count=0; check(call(ZF_SEEK)==0);
    memcpy(saved_token,VM(0xc000),16); memcpy(VM(0xc020),saved_token,16);
    invocation.object=0xc020;
    check(call(ZF_CLOSE)!=0); /* copied token cannot close the real handle */
    assign("H:/COPY.DAT"); check(call(ZF_REWRITE)==0); put(0xc200,77); check(call(ZF_WRITE)==0);
    check(call(ZF_CLOSE)==0);
    invocation.object=0xc000; check(call(ZF_READ)==0&&get(0xc200)==11);
    invocation.data=0xc140; VM(0xc140)[0]=8; memcpy(VM(0xc141),"H:/OTHER",8);
    check(call(ZF_ASSIGN)==0x20); invocation.data=0xc200;
    check(call(ZF_CLOSE)==0); check(call(ZF_RESET)==0);
    check(call(ZF_READ)==0&&get(0xc200)==11); check(call(ZF_CLOSE)==0);
    clean(ZF_END_PROGRAM,0,0);
}
static void lifetime(void) {
    uint16_t old_generation;
    defaults();
    invocation.object=0xe402; invocation.heap_frontier=0xe422;
    put(0xe400,32); memset(VM(0xe402),0xa5,32);
    assign("H:/HEAP.DAT"); check(get(0xe408)==0xe402); check(call(ZF_REWRITE)==0);
    put(0xc200,42); check(call(ZF_WRITE)==0);
    memcpy(saved_token,VM(0xe402),16); old_generation=get(0xe406);
    clean(ZF_END_OBJECT,0xe402,0);
    check(call(ZF_READ)!=0); assign("H:/HEAP.DAT");
    check(get(0xe406)!=old_generation); check(call(ZF_RESET)==0);
    memcpy(VM(0xe402),saved_token,16); check(call(ZF_CLOSE)!=0);
    /* End-of-object cleanup uses trusted records even if its token is stale. */
    clean(ZF_END_OBJECT,0xe402,0);
    put(0xe400,0x8020); invocation.data=0xc140; check(call(ZF_ASSIGN)==0x1ff);
    put(0xe400,32); invocation.object=0xe418; check(call(ZF_ASSIGN)==0x1ff);
    invocation.object=0xe402; put(0xe400,31); check(call(ZF_ASSIGN)==0x1ff);
    clean(ZF_END_PROGRAM,0,0);

    defaults(); invocation.frame=0xe800; invocation.object=0xe780;
    put(0xe800,0xe900); put(0xe7fc,0xe800); put(0xe7f8,0xe780);
    put(0xe900,0); put(0xe8fc,0xe900); put(0xe8f8,0xe880);
    memset(VM(0xe780),0,16);
    assign("H:/FRAME.DAT"); check(get(0xe786)==0xe800); check(call(ZF_REWRITE)==0);
    put(0xc200,55); check(call(ZF_WRITE)==0);
    put(0xe900,0xe800); check(call(ZF_CLOSE)==0x1ff); put(0xe900,0);
    clean(ZF_END_FRAME,0xe800,0); check(call(ZF_READ)!=0);
    invocation.object=0xe7f0; invocation.data=0xc140; check(call(ZF_ASSIGN)==0x1ff);
    clean(ZF_END_PROGRAM,0,0);
}
static void boundaries(void) {
    static const uint8_t keep[]={9,8,7,6};
    static const uint8_t maximum[]={255,255,1,0};
    uint8_t actual[4];
    uint16_t size=4;
    int8_t fd;
    defaults(); seed("H:/ZERO.DAT",keep,4); put(0xc101,0); assign("H:/ZERO.DAT");
    check(call(ZF_REWRITE)==0x90); check(call(ZF_RESET)==0x90);
    fd=open("H:/ZERO.DAT",O_RDONLY); check(fd>=0);
    if(fd>=0) { check(read(fd,actual,&size)==0&&size==4&&!memcmp(actual,keep,4)); check(close(fd)==0); }
    put(0xc101,4); assign("H:/TYPED.DAT"); check(call(ZF_RESET)==0x90);
    put(0xc101,1); seed("H:/MAX.DAT",maximum,4); assign("H:/MAX.DAT");
    check(call(ZF_RESET)==0); check(call(ZF_SIZE)==0&&invocation.result==65535u);
    invocation.count=invocation.result; check(call(ZF_SEEK)==0);
    check(call(ZF_WRITE)==0xf2&&invocation.actual==0); check(call(ZF_CLOSE)==0);
    invocation.descriptor=0xdffe; check(call(ZF_RESET)==0x1ff);
    invocation.descriptor=0xc100; invocation.object=0xdff1; check(call(ZF_RESET)==0x1ff);
    clean(ZF_END_PROGRAM,0,0);
}
static void block_files(void) {
    uint16_t i;
    defaults(); VM(0xc100)[0]=2; put(0xc101,128); assign("H:/BLOCKS.DAT");
    check(call(ZF_REWRITE)==0);
    for(i=0;i<256;++i) VM(0xc200)[i]=(uint8_t)i;
    invocation.count=2; check(call(ZF_BLOCKWRITE)==0&&invocation.actual==2);
    check(call(ZF_SIZE)==0&&invocation.result==2); check(call(ZF_CLOSE)==0);
    check(call(ZF_RESET)==0); memset(VM(0xc200),0,384); invocation.count=3;
    check(call(ZF_BLOCKREAD_PARTIAL)==0&&invocation.actual==2);
    for(i=0;i<256;++i) check(VM(0xc200)[i]==(uint8_t)i);
    check(call(ZF_POSITION)==0&&invocation.result==2);
    check(call(ZF_BLOCKREAD_PARTIAL)==0&&invocation.actual==0);
    check(call(ZF_BLOCKREAD)==0x99&&invocation.actual==0);
    invocation.count=0; check(call(ZF_BLOCKREAD)==0&&invocation.actual==0);
    invocation.count=512; check(call(ZF_BLOCKREAD)==0x1ff);
    check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);
}
static void many(void) {
    uint16_t i;
    defaults();
    for(i=0;i<205;++i) {
        invocation.object=0xc800+i*16; memset(VM(invocation.object),0xa5,16);
        assign("H:/MANY.DAT");
    }
    /* More than one private page, but no open descriptor per association. */
    invocation.object=0xc800+204u*16; check(call(ZF_REWRITE)==0); check(call(ZF_CLOSE)==0);
    clean(ZF_END_PROGRAM,0,0);
}
static void wide_components(void) {
    defaults(); put(0xc101,300); assign("H:/WIDE.DAT");
    check(call(ZF_REWRITE)==0); memset(VM(0xc200),0x6b,300);
    check(call(ZF_WRITE)==0); check(call(ZF_CLOSE)==0);
    /* Both in-place finalization and Rewrite on an existing file exercise
     * the pinned metadata status path through the real native SDK too. */
    check(call(ZF_REWRITE)==0); check(call(ZF_WRITE)==0); check(call(ZF_CLOSE)==0);
    check(call(ZF_RESET)==0); memset(VM(0xc200),0,300);
    check(call(ZF_READ)==0&&VM(0xc200)[0]==0x6b&&VM(0xc200)[299]==0x6b);
    check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);
}
static void text_defaults(const char *name) {
    defaults(); VM(0xc100)[0]=0; put(0xc101,0); assign(name);
}
static void text_value(const char *value) {
    VM(0xc200)[0]=(uint8_t)strlen(value); memcpy(VM(0xc201),value,strlen(value));
    check(call(ZF_TEXT_WRITE)==0);
}
static void text_files(void) {
    static const uint8_t white[]="  \r\n\t 12 \t\r\n \r\n\032X";
    text_defaults("H:/TEXT.DAT"); check(call(ZF_REWRITE)==0);
    text_value("ABC"); text_value("D"); check(call(ZF_TEXT_WRITELN)==0);
    text_value("  123"); check(call(ZF_TEXT_WRITELN)==0); check(call(ZF_CLOSE)==0);
    check(call(ZF_RESET)==0); invocation.count=ZF_TEXT_CHAR;
    check(call(ZF_TEXT_READ)==0&&invocation.actual==1&&VM(0xc200)[0]=='A');
    check(call(ZF_EOF)==0&&!invocation.result); check(call(ZF_EOLN)==0&&!invocation.result);
    invocation.count=ZF_TEXT_STRING|(20u<<8);
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]==3&&!memcmp(VM(0xc201),"BCD",3));
    check(call(ZF_EOLN)==0&&invocation.result); check(call(ZF_TEXT_READLN)==0);
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]==5&&!memcmp(VM(0xc201),"  123",5));
    check(call(ZF_TEXT_READLN)==0); check(call(ZF_EOF)==0&&invocation.result);
    check(call(ZF_EOLN)==0&&invocation.result); check(call(ZF_TEXT_READLN)==0);
    check(call(ZF_CLOSE)==0); check(call(ZF_TEXT_READ)==2); check(call(ZF_TEXT_WRITE)==3);
    clean(ZF_END_PROGRAM,0,0);

    seed("H:/WHITE.TXT",white,sizeof(white)-1);
    text_defaults("H:/WHITE.TXT"); check(call(ZF_RESET)==0);
    check(call(ZF_EOLN)==0&&!invocation.result);
    check(call(ZF_SEEKEOLN)==0&&invocation.result); check(call(ZF_TEXT_READLN)==0);
    check(call(ZF_SEEKEOF)==0&&!invocation.result);
    put(0xc200,0xc250); put(0xc250,777); invocation.count=ZF_TEXT_INTEGER;
    check(call(ZF_TEXT_READ)==0&&invocation.actual==1&&get(0xc250)==777);
    check(VM(0xc200+ZF_NUMBER_TOKEN)[0]==2&&!memcmp(VM(0xc201+ZF_NUMBER_TOKEN),"12",2));
    check(call(ZF_SEEKEOLN)==0&&invocation.result);
    check(call(ZF_SEEKEOF)==0&&invocation.result);
    check(call(ZF_TEXT_READ)==0&&!invocation.actual&&get(0xc250)==777);
    invocation.count=ZF_TEXT_CHAR; check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]==26);
    check(call(ZF_EOF)==0&&invocation.result); check(call(ZF_CLOSE)==0);
    clean(ZF_END_PROGRAM,0,0);

    seed("H:/EMPTY.TXT",white,0); text_defaults("H:/EMPTY.TXT"); check(call(ZF_RESET)==0);
    check(call(ZF_EOF)==0&&invocation.result); check(call(ZF_EOLN)==0&&invocation.result);
    invocation.count=ZF_TEXT_STRING; VM(0xc201)[0]=77;
    check(call(ZF_TEXT_READ)==0&&!VM(0xc200)[0]&&VM(0xc201)[0]==77);
    check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);

    text_defaults("H:/TEXT.DAT"); check(call(ZF_RESET)==0);
    invocation.count=ZF_TEXT_INTEGER; put(0xc200,0x7000);
    check(call(ZF_TEXT_READ)==0x1ff);
    invocation.count=ZF_TEXT_CHAR; check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]=='A');
    check(call(ZF_TEXT_WRITE)==3); check(call(ZF_POSITION)==0x20); check(call(ZF_FLUSH)==0x20);
    check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);

    text_defaults("H:/AUTOTEXT.DAT"); check(call(ZF_REWRITE)==0); text_value("AUTO");
    clean(ZF_END_PROGRAM,0,0);
}
static void file_paths(void) {
    char out[128],name[129];
    uint16_t n;
    check(path_resolve("../Mixed.dat","t:/src/pascal",out)&&!strcmp(out,"T:/src/Mixed.dat"));
    check(path_resolve("/Root","h:/src",out)&&!strcmp(out,"H:/Root"));
    check(path_resolve("h:/one//./two/../File","",out)&&!strcmp(out,"H:/one/File"));
    check(path_resolve("../../../Root","T:/a",out)&&!strcmp(out,"T:/Root"));
    strcpy(out,"../Exact");
    check(path_resolve(out,"H:/dir/",out)&&!strcmp(out,"H:/Exact"));
    check(!path_resolve("H:relative","T:/",out));
    check(!path_resolve("H:/dir/","",out));
    check(!path_resolve("H:/dir/..","",out));
    check(!path_resolve("H:/dir/.","",out));
    check(!path_resolve("H:/","",out));
    check(!path_resolve("","T:/",out));
    check(!path_resolve("name","",out));
    check(!path_resolve("H:/invalid:name","",out));
    check(!path_resolve("H:/invalid\001","",out));
    check(path_resolve("H:/123456789012345","",out));
    check(!path_resolve("H:/1234567890123456","",out));
    memcpy(name,"H:/",3);
    for(n=3;n<127;++n) name[n]=(n%16==2)?'/':'a';
    name[127]=0; check(path_resolve(name,"",out)&&strlen(out)==127);
    name[127]='a'; name[128]=0; check(!path_resolve(name,"",out));

    defaults();
    check(curdir((char*)VM(0xc400))==0); check(chdir("H:/")==0);
    assign("./PATHCASE");
    check(chdir("A:/")==0); /* A later logged-drive change cannot retarget F. */
    check(call(ZF_REWRITE)==0); put(0xc200,314);
    check(call(ZF_WRITE)==0); check(call(ZF_CLOSE)==0);
    check(call(ZF_RESET)==0); check(call(ZF_READ)==0&&get(0xc200)==314);
    check(call(ZF_CLOSE)==0);
    invocation.data=0xc140; VM(0xc140)[0]=12;
    memcpy(VM(0xc141),"H:/bad\000name!",12);
    check(call(ZF_ASSIGN)==0x20); /* Embedded NUL never selects a prefix. */
    VM(0xc140)[0]=10; memcpy(VM(0xc141),"H:relative",10);
    check(call(ZF_ASSIGN)==0x20);
    VM(0xc140)[0]=20; memset(VM(0xc141),'a',20);
    memcpy(VM(0xc141),"H:/",3); check(call(ZF_ASSIGN)==0x20);
    invocation.data=0xc200;
    check(call(ZF_RESET)==0); check(call(ZF_READ)==0&&get(0xc200)==314);
    check(call(ZF_CLOSE)==0); /* Invalid names retain the old association. */
    assign("h:/./PATHCASE"); check(call(ZF_RESET)==0);
    check(call(ZF_READ)==0&&get(0xc200)==314); check(call(ZF_CLOSE)==0);
    check(chdir("H:/")==0); assign("/PATHCASE");
    check(chdir("A:/")==0); check(call(ZF_RESET)==0);
    check(call(ZF_READ)==0&&get(0xc200)==314); check(call(ZF_CLOSE)==0);
    check(chdir((char*)VM(0xc400))==0);
    clean(ZF_END_PROGRAM,0,0);
}
static void renames(void) {
    uint16_t i,n;
    int8_t fd;
    zos_stat_t statbuf;
    defaults();
    for(i=0;i<321;++i) VM(0xc500+i)[0]=(uint8_t)(i^0x5a);
    seed("H:/RENAME.OLD",VM(0xc500),321);
    assign("H:/RENAME.OLD"); memcpy(saved_token,VM(0xc000),16);
    check(named(ZF_RENAME,"h:/./RENAMED.DAT")==0);
    check(!memcmp(saved_token,VM(0xc000),16));
    check(stat("H:/RENAME.OLD",&statbuf)==ERR_NO_SUCH_ENTRY);
    check(stat("H:/RENAMED.DAT",&statbuf)==0&&statbuf.s_size==321);
    check(named(ZF_RENAME,"H:/RENAMED.DAT")==0x20);
    check(named(ZF_RENAME,"H:/TYPED.DAT")==0x20);
    check(named(ZF_RENAME,"H:relative")==0x20);
    check(named(ZF_RENAME,"H:/missing/New")==1);
    memcpy(VM(0xc020),VM(0xc000),16); invocation.object=0xc020;
    check(named(ZF_RENAME,"H:/FORGED.DAT")!=0); invocation.object=0xc000;
    clean(ZF_END_PROGRAM,0,0);

    defaults(); assign("H:/RNINT.OLD"); check(call(ZF_REWRITE)==0);
    put(0xc200,456); check(call(ZF_WRITE)==0);
    check(named(ZF_RENAME,"H:/RNINT.NEW")==0x20); /* Must be closed. */
    check(call(ZF_CLOSE)==0); check(named(ZF_RENAME,"H:/RNINT.NEW")==0);
    check(call(ZF_RESET)==0); check(call(ZF_READ)==0&&get(0xc200)==456);
    check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);

    seed("H:/RNEMPTY.OLD",VM(0xc500),0);
    text_defaults("H:/RNEMPTY.OLD"); check(named(ZF_RENAME,"H:/RNEMPTY.NEW")==0);
    check(stat("H:/RNEMPTY.NEW",&statbuf)==0&&statbuf.s_size==0);
    check(call(ZF_RESET)==0); check(call(ZF_EOF)==0&&invocation.result);
    check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);

    check(mkdir("H:/RNDIRECTORY")==0);
    defaults(); assign("H:/RNDIRECTORY"); check(named(ZF_RENAME,"H:/RNDIR")==0x20);
    clean(ZF_END_PROGRAM,0,0);
    defaults(); assign("H:/RNMISSING"); check(named(ZF_RENAME,"H:/RNNOTMADE")==1);
    clean(ZF_END_PROGRAM,0,0);

    defaults();
    for(i=0;i<4096;++i) VM(0xc800+i)[0]=(uint8_t)(i*7+3);
    fd=open("H:/RNLARGE.OLD",O_WRONLY|O_CREAT|O_TRUNC); check(fd>=0);
    if(fd>=0) {
        for(i=0;i<17;++i) { n=4096; check(write(fd,VM(0xc800),&n)==0&&n==4096); }
        check(close(fd)==0);
    }
    assign("H:/RNLARGE.OLD"); check(named(ZF_RENAME,"H:/RNLARGE.NEW")==0);
    check(stat("H:/RNLARGE.NEW",&statbuf)==0&&statbuf.s_size==69632ul);
    check(stat("H:/RNLARGE.OLD",&statbuf)==ERR_NO_SUCH_ENTRY);
    clean(ZF_END_PROGRAM,0,0);

    /* Exercise the real TF and EEPROM filesystems in both directions. */
    defaults(); assign("T:/RNCROSS.OLD"); check(call(ZF_REWRITE)==0);
    put(0xc200,73); check(call(ZF_WRITE)==0); check(call(ZF_CLOSE)==0);
    check(named(ZF_RENAME,"B:/RNCROSS.NEW")==0);
    check(call(ZF_RESET)==0); check(call(ZF_READ)==0&&get(0xc200)==73);
    check(call(ZF_CLOSE)==0); check(named(ZF_RENAME,"T:/RNCROSS.NEW")==0);
    check(call(ZF_RESET)==0); check(call(ZF_READ)==0&&get(0xc200)==73);
    check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);
}
static void keyboard_files(void) {
    defaults(); VM(0xc100)[0]=0; put(0xc101,0); invocation.count=ZF_TEXT_CHAR;
    assign("kBd:"); check(call(ZF_TEXT_READ)==0&&invocation.stream==ZF_STANDARD_KBD);
    check(call(ZF_RESET)==0); check(call(ZF_TEXT_READ)==0&&invocation.stream==ZF_STANDARD_KBD);
    invocation.lookahead=0x158; invocation.result=1; invocation.actual=1;
    check(call(ZF_STREAM_RETURN)==0&&invocation.result==1&&invocation.actual==1);
    check(call(ZF_EOLN)==0&&invocation.lookahead==0x158);
    invocation.lookahead=0x258; check(call(ZF_STREAM_RETURN)==0x1ff);
    check(call(ZF_EOF)==0&&invocation.lookahead==0x158);
    invocation.lookahead=0x059; invocation.error=0x20; invocation.detail=77;
    check(call(ZF_STREAM_RETURN)==0x20&&invocation.detail==77);
    check(call(ZF_TEXT_READ)==0&&invocation.lookahead==0x59);
    check(call(ZF_CLOSE)==0); check(call(ZF_TEXT_READ)==0&&invocation.stream==ZF_STANDARD_KBD);
    check(call(ZF_REWRITE)==0); check(call(ZF_EOF)==0&&invocation.lookahead==0);
    check(call(ZF_TEXT_WRITELN)==3&&!invocation.stream);
    check(call(ZF_ERASE)==0x20); check(named(ZF_RENAME,"H:/KBDINVALID")==0x20);
    memcpy(VM(0xc020),VM(0xc000),16); invocation.object=0xc020;
    invocation.stream=ZF_STANDARD_KBD; invocation.error=0;
    check(call(ZF_STREAM_RETURN)==0x1ff);
    invocation.object=0xc000; check(call(ZF_EOF)==0&&invocation.lookahead==0);
    clean(ZF_END_PROGRAM,0,0);

    defaults(); check(named(ZF_ASSIGN,"KBD:")==0x20); /* Text only. */
    VM(0xc100)[0]=0; put(0xc101,0); invocation.code_origin=0xc100;
    invocation.standard_table=0xc300;
    put(0xc300,3); put(0xc302,0xc000); put(0xc304,ZF_STANDARD_INPUT);
    put(0xc306,0xc020); put(0xc308,ZF_STANDARD_OUTPUT);
    put(0xc30a,0xc040); put(0xc30c,ZF_STANDARD_KBD);
    invocation.object=0xc040;
    check(call(ZF_EOF)==0&&invocation.stream==ZF_STANDARD_KBD);
    check(call(ZF_RESET)==0x22); check(call(ZF_REWRITE)==0x22); check(call(ZF_CLOSE)==0x22);
    check(named(ZF_ASSIGN,"H:/KBDINVALID")==0x22);
    check(call(ZF_TEXT_WRITELN)==3&&!invocation.stream);
    put(0xc30c,ZF_STANDARD_OUTPUT); check(call(ZF_EOF)==0x1ff); /* duplicate identity */
    put(0xc30c,ZF_STANDARD_KBD); check(call(ZF_EOF)==0);
    clean(ZF_END_PROGRAM,0,0);
}
static void console_files(void) {
    uint16_t device;
    for(device=ZF_STANDARD_CON;device<=ZF_STANDARD_TRM;++device) {
        defaults(); VM(0xc100)[0]=0; put(0xc101,0); invocation.count=ZF_TEXT_CHAR;
        assign(device==ZF_STANDARD_CON?"cOn:":"tRm:");
        check(call(ZF_TEXT_READ)==0&&invocation.stream==device);
        invocation.lookahead=0x141; check(call(ZF_STREAM_RETURN)==0);
        check(call(ZF_CLOSE)==0); check(call(ZF_EOLN)==0&&invocation.lookahead==0x141);
        check(call(ZF_RESET)==0); check(call(ZF_EOF)==0&&invocation.lookahead==0);
        check(call(ZF_TEXT_WRITELN)==0&&invocation.stream==device);
        invocation.data=0xc200; VM(0xc200)[0]=1; VM(0xc201)[0]='X';
        check(call(ZF_TEXT_WRITE)==0&&invocation.stream==device);
        check(call(ZF_REWRITE)==0); check(call(ZF_TEXT_READ)==0&&invocation.lookahead==0);
        invocation.lookahead=0x142; invocation.stream=device==ZF_STANDARD_CON?ZF_STANDARD_TRM:ZF_STANDARD_CON;
        check(call(ZF_STREAM_RETURN)==0x1ff);
        check(call(ZF_ERASE)==0x20); check(named(ZF_RENAME,"H:/CONINVALID")==0x20);
        clean(ZF_END_PROGRAM,0,0);
    }
    defaults(); VM(0xc100)[0]=0; put(0xc101,0); invocation.code_origin=0xc100;
    invocation.standard_table=0xc300;
    put(0xc300,2); put(0xc302,0xc000); put(0xc304,ZF_STANDARD_CON);
    put(0xc306,0xc020); put(0xc308,ZF_STANDARD_TRM);
    for(device=ZF_STANDARD_CON;device<=ZF_STANDARD_TRM;++device) {
        invocation.object=device==ZF_STANDARD_CON?0xc000:0xc020;
        check(call(ZF_EOF)==0&&invocation.stream==device);
        check(call(ZF_RESET)==0x22);check(call(ZF_REWRITE)==0x22);check(call(ZF_CLOSE)==0x22);
        check(named(ZF_ASSIGN,"CON:")==0x22);
    }
    clean(ZF_END_PROGRAM,0,0);
}
static void standard_files(void) {
    uint16_t code;
    defaults(); invocation.code_origin=0xc100;
    VM(0xc100)[0]=0; put(0xc101,0);
    invocation.standard_table=0xc300;
    put(0xc300,2); put(0xc302,0xc000); put(0xc304,ZF_STANDARD_INPUT);
    put(0xc306,0xc020); put(0xc308,ZF_STANDARD_OUTPUT);
    check(call(ZF_TEXT_BEGIN)==0&&invocation.stream==ZF_STANDARD_INPUT);
    check(call(ZF_EOF)==0&&invocation.stream==ZF_STANDARD_INPUT);
    check(call(ZF_EOLN)==0&&invocation.stream==ZF_STANDARD_INPUT);
    check(call(ZF_SEEKEOF)==0&&invocation.stream==ZF_STANDARD_INPUT);
    check(call(ZF_SEEKEOLN)==0&&invocation.stream==ZF_STANDARD_INPUT);
    check(VM(0xc009)[0]==1&&VM(0xc029)[0]==1);
    check(call(ZF_CLOSE)==0x22&&!invocation.stream);
    check(call(ZF_RESET)==0x22); check(call(ZF_REWRITE)==0x22);
    check(named(ZF_ASSIGN,"H:/STD-INVALID")==0x22);
    check(call(ZF_ERASE)==0x20); check(named(ZF_RENAME,"H:/STD-INVALID")==0x20);
    invocation.object=0xc020;
    VM(0xc200)[0]=1; VM(0xc201)[0]='X';
    check(call(ZF_TEXT_WRITE)==0&&invocation.stream==ZF_STANDARD_OUTPUT);
    invocation.data=0x7000; check(call(ZF_TEXT_WRITE)==0x1ff&&!invocation.stream);
    invocation.count=ZF_TEXT_CHAR;
    check(call(ZF_TEXT_READ)==0x1ff&&!invocation.stream);
    invocation.data=0xc200;
    check(call(ZF_TEXT_READ)==0&&invocation.stream==ZF_STANDARD_OUTPUT);
    memcpy(VM(0xc040),VM(0xc020),16); invocation.object=0xc040;
    check(call(ZF_TEXT_WRITE)!=0&&!invocation.stream);
    assign("H:/STDCLONE.TXT"); check(!VM(0xc049)[0]);
    check(call(ZF_REWRITE)==0); text_value("X"); check(call(ZF_CLOSE)==0);
    invocation.object=0xc020; check(call(ZF_TEXT_WRITE)==0&&invocation.stream==ZF_STANDARD_OUTPUT);
    clean(ZF_END_PROGRAM,0,0);
    defaults(); invocation.code_origin=0xc100; VM(0xc100)[0]=0; put(0xc101,0);
    invocation.standard_table=0xc300;
    put(0xc300,2); put(0xc302,0xc000); put(0xc304,1); put(0xc306,0xc000); put(0xc308,2);
    check(call(ZF_TEXT_BEGIN)==0x1ff); /* duplicate address, before any registration */
    put(0xc306,0xc020); put(0xc308,1); check(call(ZF_TEXT_BEGIN)==0x1ff);
    put(0xc308,2); put(0xc306,0xc100); check(call(ZF_TEXT_BEGIN)==0x1ff);
    put(0xc306,0xc020); check(call(ZF_TEXT_BEGIN)==0);
    VM(0xc009)[0]=0; code=call(ZF_TEXT_BEGIN);
    check(code==0x1ff&&!invocation.stream); /* corrupted identity cannot mint a second owner */
    VM(0xc009)[0]=1; check(call(ZF_TEXT_BEGIN)==0&&invocation.stream==1);
    clean(ZF_END_PROGRAM,0,0);
}
/* Identity numbers are an independently fixed table expectation, not derived
 * from the product's new enumeration. Lst=6, Aux=7. */
static void printer_files(void) {
    defaults(); VM(0xc100)[0]=0; put(0xc101,0); invocation.count=ZF_TEXT_CHAR;
    assign("lSt:"); check(call(ZF_RESET)==0); check(call(ZF_REWRITE)==0);
    check(call(ZF_TEXT_READ)==2&&!invocation.stream);
    check(call(ZF_EOF)==2); check(call(ZF_EOLN)==2); check(call(ZF_SEEKEOF)==2);
    check(call(ZF_TEXT_WRITELN)==0x20&&!invocation.stream);
    VM(0xc200)[0]=1; VM(0xc201)[0]='X'; check(call(ZF_TEXT_WRITE)==0x20);
    check(call(ZF_CLOSE)==0); check(call(ZF_TEXT_WRITELN)==0x20);
    check(call(ZF_ERASE)==0x20); check(named(ZF_RENAME,"H:/DEVINVALID")==0x20);
    clean(ZF_END_PROGRAM,0,0);
    defaults(); check(named(ZF_ASSIGN,"LST:")==0x20); check(named(ZF_ASSIGN,"AUX:")==0x20);
    VM(0xc100)[0]=0; put(0xc101,0); invocation.code_origin=0xc100;
    invocation.standard_table=0xc300;
    put(0xc300,7);
    { uint16_t i; for(i=1;i<=7;++i) { put(0xc2fe + i*4,0xc000+(i-1)*16); put(0xc300+i*4,i); } }
    invocation.object=0xc050; check(call(ZF_TEXT_WRITELN)==0x20);
    check(call(ZF_CLOSE)==0x22); check(call(ZF_RESET)==0x22); check(call(ZF_REWRITE)==0x22);
    check(named(ZF_ASSIGN,"LST:")==0x22);
    invocation.object=0xc060;
    check(call(ZF_CLOSE)==0x22); check(call(ZF_RESET)==0x22); check(call(ZF_REWRITE)==0x22);
    check(named(ZF_ASSIGN,"AUX:")==0x22);
    put(0xc31c,6); check(call(ZF_EOF)==0x1ff); /* duplicate Lst/Aux identity */
    put(0xc31c,7); clean(ZF_END_PROGRAM,0,0);
}
static void callback_output_files(void) {
    /* New requests are explicitly tagged. A mask only lends output after the
     * existing binding, direction and complete value have been authenticated. */
    defaults(); VM(0xc100)[0]=0; put(0xc101,0);
    assign("LST:"); invocation.count=0x4001; VM(0xc200)[0]=1; VM(0xc201)[0]='X';
    check(call(ZF_TEXT_WRITE)==0&&invocation.stream==6);
    check(call(ZF_TEXT_WRITELN)==0&&invocation.stream==6);
    invocation.count=0x8000; check(call(ZF_TEXT_WRITE)==0x20&&!invocation.stream);
    invocation.count=0x4001; invocation.data=0xffff; check(call(ZF_TEXT_WRITE)==0x1ff&&!invocation.stream);
    invocation.data=0xc200; assign("KBD:");check(call(ZF_TEXT_WRITE)==3&&!invocation.stream);
    assign("AUX:");invocation.count=0x4002;check(call(ZF_TEXT_WRITE)==0&&invocation.stream==7);
    assign("uSr:");invocation.count=ZF_TEXT_CHAR;
    check(call(ZF_TEXT_READ)==0&&invocation.stream==8);
    invocation.lookahead=0x15a;check(call(ZF_STREAM_RETURN)==0);
    check(call(ZF_CLOSE)==0);check(call(ZF_EOF)==0&&invocation.lookahead==0x15a);
    check(call(ZF_RESET)==0);check(call(ZF_EOLN)==0&&invocation.lookahead==0);
    invocation.count=0;check(call(ZF_TEXT_WRITELN)==0&&invocation.stream==8);
    check(named(ZF_RENAME,"H:/USRINVALID")==0x20);check(call(ZF_ERASE)==0x20);
    clean(ZF_END_PROGRAM,0,0);
    defaults();check(named(ZF_ASSIGN,"USR:")==0x20);
}
#ifndef __SDCC
static void serial_input(const char *data,uint16_t length) {
    memcpy(model_serial_input,data,length); model_serial_size=length; model_serial_position=0;
}
static void serial_files(void) {
    uint16_t before,position;
    uint8_t scenario;
    defaults(); VM(0xc100)[0]=0; put(0xc101,0); invocation.count=ZF_TEXT_CHAR;
    assign("aUx:"); serial_input("A\032B\r\nC",6);
    check(call(ZF_EOF)==0&&!invocation.result&&!invocation.stream&&model_handles==0);
    position=model_serial_position; check(position==1); before=model_open_calls;
    model_serial_present=0;
    check(call(ZF_EOLN)==0&&!invocation.result&&model_serial_position==position&&model_open_calls==before);
    check(call(ZF_CLOSE)==0);
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]=='A'&&model_open_calls==before);
    model_serial_present=1;
    check(call(ZF_EOF)==0&&invocation.result);
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]==26);
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]=='B');
    check(call(ZF_TEXT_READLN)==0&&model_serial_position==4);
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]==10);
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]=='C');
    check(call(ZF_TEXT_READ)==0x99&&VM(0xc200)[0]=='C'&&model_handles==0);
    serial_input("  \t\r\n123 Z",10); invocation.count=ZF_TEXT_INTEGER;
    put(0xc200,0xc280); check(call(ZF_SEEKEOF)==0&&!invocation.result);
    check(call(ZF_TEXT_READ)==0&&invocation.actual==1&&VM(0xc20a)[0]==3&&
        !memcmp(VM(0xc20b),"123",3));
    invocation.count=ZF_TEXT_CHAR; check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]==' ');
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]=='Z');
    serial_input("XY",2); check(call(ZF_EOF)==0&&!invocation.result);
    check(call(ZF_RESET)==0); check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]=='Y');
    serial_input("PQ",2); check(call(ZF_EOF)==0&&!invocation.result);
    invocation.object=0xc020; assign("AUX:"); check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]=='Q');
    invocation.object=0xc000; check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]=='P');
    serial_input("xyz\r",4); invocation.count=ZF_TEXT_STRING|(2u<<8);
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]==2&&!memcmp(VM(0xc201),"xy",2));
    check(call(ZF_TEXT_READLN)==0&&model_serial_position==4);
    serial_input(" \t\rQ",4); check(call(ZF_SEEKEOLN)==0&&invocation.result);
    check(call(ZF_TEXT_READLN)==0&&model_serial_position==3);
    invocation.count=ZF_TEXT_CHAR; check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]=='Q');
    check(call(ZF_ERASE)==0x20); check(named(ZF_RENAME,"CON:")==0x20);
    model_serial_written=0; model_serial_attr=0; text_value("RAW");
    check(call(ZF_TEXT_WRITELN)==0&&model_serial_attr==0&&model_handles==0);
    check(model_serial_written==5&&!memcmp(model_serial_output,"RAW\r\n",5));
    check(model_serial_seek_calls==0);
    /* Missing serial, setup, transfer, restoration and close faults must not
     * become success, leak a handle, or change a destination on failed input. */
    model_serial_present=0; check(call(ZF_TEXT_WRITELN)==0x20);
    check(call(ZF_TEXT_READ)==0x20); model_serial_present=1;
    for(scenario=1;scenario<=3;++scenario) {
        before=model_serial_ioctl_calls; model_serial_fail_ioctl_at=before+scenario;
        check(call(ZF_TEXT_WRITELN)==(scenario==3?0xf0:0x20)&&model_handles==0);
        model_serial_fail_ioctl_at=0; model_serial_attr=0;
    }
    model_zero_write=1; check(call(ZF_TEXT_WRITELN)==0xf0&&model_serial_attr==0&&model_handles==0);
    model_zero_write=0; model_oversize_write=1;
    check(call(ZF_TEXT_WRITELN)==0xf0&&model_serial_attr==0&&model_handles==0);
    model_oversize_write=0; model_write_error=ERR_FAILURE; model_close_error=ERR_NO_SUCH_ENTRY;
    check(call(ZF_TEXT_WRITELN)==0xf0&&invocation.detail==ERR_FAILURE&&model_handles==0);
    model_write_error=0; model_close_error=0;
    invocation.count=ZF_TEXT_CHAR; model_read_error=ERR_FAILURE; VM(0xc200)[0]='!';
    check(call(ZF_TEXT_READ)==0x99&&VM(0xc200)[0]=='!'&&model_handles==0);
    model_read_error=0; model_oversize_read=1;
    check(call(ZF_TEXT_READ)==0x99&&VM(0xc200)[0]=='!'&&model_handles==0);
    model_oversize_read=0; serial_input("Z",1); model_close_error=ERR_FAILURE;
    check(call(ZF_EOF)==0xf0&&model_handles==0); model_close_error=0;
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]=='Z');
    before=model_serial_position; invocation.count=0;
    check(call(ZF_TEXT_BEGIN)==0&&model_serial_position==before);
    clean(ZF_END_PROGRAM,0,0); model_serial_attr=1;
}
static void rename_faults(void) {
    uint16_t scenario,i,n;
    int8_t fd;
    for(scenario=0;scenario<21;++scenario) {
        /* Each transaction starts with exactly one complete old copy. */
        rm("H:/RFAIL.NEW");
        for(i=0;i<321;++i) VM(0xc500+i)[0]=(uint8_t)(i^0x5a);
        seed("H:/RFAIL.OLD",VM(0xc500),321);
        text_defaults("H:/RFAIL.OLD");
        model_open_calls=model_close_calls=model_read_calls=model_write_calls=0;
        if(scenario<4) model_fail_open_at=3+scenario;
        else if(scenario<8) model_fail_close_at=scenario-2;
        else if(scenario<12) model_fail_read_at=scenario==8?1:scenario==9?5:scenario==10?6:12;
        else if(scenario==12) model_fail_write_at=2;
        else if(scenario==13) model_corrupt_write=1;
        else if(scenario==14) model_rm_error=ERR_FAILURE;
        else if(scenario==15) model_zero_write=1;
        else if(scenario==16) model_readonly=1;
        else if(scenario==17) model_zero_read_at=6;
        else if(scenario==18) model_corrupt_write=2;
        else if(scenario==19) model_oversize_read=1;
        else model_oversize_write=1;
        check(named(ZF_RENAME,"H:/RFAIL.NEW")==
            ((scenario>=8&&scenario<12)||scenario==17||scenario==19?0x99:scenario==16?0x20:0xf0));
        check(model_handles==0);
        model_fail_open_at=model_fail_close_at=model_fail_read_at=model_fail_write_at=0;
        model_rm_error=model_corrupt_write=0;
        model_zero_write=model_readonly=model_zero_read_at=0;
        model_oversize_read=model_oversize_write=0;
        fd=open("H:/RFAIL.OLD",O_RDONLY); check(fd>=0); n=321;
        check(read(fd,VM(0xc700),&n)==0&&n==321&&!memcmp(VM(0xc500),VM(0xc700),321));
        check(close(fd)==0);
        check(call(ZF_RESET)==0); invocation.count=ZF_TEXT_CHAR;
        check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]==0x5a);
        check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);
    }
    rm("H:/RFAIL.NEW");
    /* Legal short reads/writes are completed rather than rejected. */
    text_defaults("H:/RFAIL.OLD"); model_short=1;
    check(named(ZF_RENAME,"H:/RSHORT.NEW")==0); model_short=0;
    check(call(ZF_RESET)==0); invocation.count=ZF_TEXT_CHAR;
    check(call(ZF_TEXT_READ)==0&&VM(0xc200)[0]==0x5a);
    check(call(ZF_CLOSE)==0); clean(ZF_END_PROGRAM,0,0);
}
static void faults(void) {
    uint8_t handles;
    defaults(); assign("H:/ERROR.DAT"); check(call(ZF_REWRITE)==0);
    put(0xc200,1234); model_short=1; check(call(ZF_WRITE)==0&&invocation.actual==1);
    model_short=0; model_zero_write=1; check(call(ZF_WRITE)==0xf0&&invocation.actual==0);
    model_zero_write=0; model_write_error=ERR_FAILURE;
    check(call(ZF_WRITE)==0xf0&&invocation.actual==0); model_write_error=0;
    check(call(ZF_CLOSE)==0); check(call(ZF_RESET)==0);
    model_read_error=ERR_FAILURE; put(0xc200,0xaaaa);
    check(call(ZF_READ)==0x99&&invocation.actual==0&&get(0xc200)==0xaaaa);
    model_read_error=0; model_close_error=ERR_FAILURE;
    check(call(ZF_CLOSE)==0xf0); check(model_handles==0); check(call(ZF_CLOSE)==0);
    model_close_error=0; check(call(ZF_RESET)==0); check(call(ZF_CLOSE)==0);
    check(call(ZF_RESET)==0); model_close_error=ERR_NO_SUCH_ENTRY;
    check(call(ZF_CLOSE)==0xff&&invocation.detail==ERR_NO_SUCH_ENTRY&&model_handles==0);
    model_close_error=0;
    model_readonly=1; check(call(ZF_RESET)==0); check(call(ZF_READ)==0&&get(0xc200)==1234);
    check(call(ZF_WRITE)==3); check(call(ZF_CLOSE)==0); model_readonly=0;
    invocation.object=0xc020; assign("H:/OTHER.DAT"); check(call(ZF_REWRITE)==0);
    put(0xc200,5678); check(call(ZF_WRITE)==0);
    invocation.object=0xc000; check(call(ZF_REWRITE)==0);
    put(0xc200,1234); check(call(ZF_WRITE)==0);
    handles=model_handles; check(handles==2); model_close_error=ERR_FAILURE;
    ending.operation=ZF_END_PROGRAM; ending.owner=ending.end=0; zf_cleanup(&ending);
    check(ending.error==0xf0&&ending.detail==ERR_FAILURE&&model_handles==0&&model_pages==0);
    model_close_error=0;
    /* An earlier close failure must not suppress a later file's header. */
    defaults(); assign("H:/OTHER.DAT"); model_short=1; check(call(ZF_RESET)==0);
    check(call(ZF_SIZE)==0&&invocation.result==1);
    check(call(ZF_READ)==0&&get(0xc200)==5678); check(call(ZF_CLOSE)==0);
    assign("H:/ERROR.DAT"); check(call(ZF_RESET)==0);
    check(call(ZF_SIZE)==0&&invocation.result==1);
    check(call(ZF_READ)==0&&get(0xc200)==1234); check(call(ZF_CLOSE)==0);
    model_short=0; clean(ZF_END_PROGRAM,0,0);

    defaults(); assign("H:/ERROR.DAT");
    model_curdir_error=ERR_FAILURE;
    invocation.data=0xc140; VM(0xc140)[0]=8; memcpy(VM(0xc141),"relative",8);
    check(call(ZF_ASSIGN)==0x20&&invocation.detail==ERR_FAILURE);
    invocation.data=0xc200;
    check(call(ZF_RESET)==0); check(call(ZF_READ)==0&&get(0xc200)==1234);
    check(call(ZF_CLOSE)==0); model_curdir_error=0;
    clean(ZF_END_PROGRAM,0,0);
}
#endif
int main(void) {
    check(sizeof(FileJob)==ZF_JOB_BYTES&&sizeof(FileCleanup)==10);
    basic(); lifetime(); boundaries(); block_files(); many(); wide_components(); text_files(); file_paths(); renames(); standard_files(); keyboard_files(); console_files(); printer_files(); callback_output_files();
#ifndef __SDCC
    serial_files(); faults(); rename_faults(); check(model_pages==0&&model_handles==0);
    printf("%u file runtime assertions passed, %u failed\n",test_passed,test_failed);
#else
    test_done=255; test_complete();
#endif
    return test_failed?1:0;
}
