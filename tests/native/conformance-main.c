#include "harness.h"

volatile uint16_t test_passed,test_failed,test_stage,test_failures[64],test_index;
volatile uint8_t test_done,test_hash[32],corpus_result[8];
volatile char corpus_error_path[128];
extern volatile uint8_t corpus_data_page;
Source s;Build b;Diagnostic d;
void test_complete(void);
void corpus_checkpoint(void);

/* Private test input: LE16 count, then (diagnostic byte, NUL-padded path[128]).
 * Sources live on disposable HostFS. No corpus parser enters the product. */
static uint8_t read_record(int8_t fd,uint16_t bytes) {
    uint16_t count=bytes;
    return !read(fd,console_scratch,&count)&&count==bytes;
}
int app_main(void) {
    int8_t manifest,capture;
    uint16_t count,index,i;
    uint8_t expected,loaded,compiled,cpu[7];
    char path[128],folder[128];
    test_machine(cpu);corpus_data_page=cpu[6];
    if(!console_open())return 1;
    check(source_new(&s,"H:/CORPUS.PAS"));
    manifest=open("H:/CORPUS.DAT",O_RDONLY);check(manifest>=0);
    if(manifest<0)goto done;
    if(!read_record(manifest,2)){check(0);goto close_manifest;}
    count=console_scratch[0]|((uint16_t)console_scratch[1]<<8);
    check(count>0&&count<=200);if(!count||count>200)goto close_manifest;
    for(index=0;index<count;++index) {
        test_stage=9; /* Shared harness executes RAM and saved COM. */
        if(!read_record(manifest,129)){check(0);break;}
        expected=console_scratch[0];memcpy(path,console_scratch+1,128);
        check(path[0]=='H'&&path[1]==':'&&path[2]=='/'&&memchr(path,0,128)!=0);
        if(path[0]!='H'||path[1]!=':'||path[2]!='/'||!memchr(path,0,128))break;
        strcpy(folder,path);i=(uint16_t)strlen(folder);
        while(i&&folder[i-1]!='/')--i;
        if(i<=3||i>sizeof(folder)-sizeof("/OUTPUT.TXT")+1){check(0);break;}folder[i-1]=0;
        strcpy((char*)console_scratch,folder);check(!chdir((char*)console_scratch));
        loaded=load_source(&s,path,&d);compiled=0;
        if(expected) {
            if(loaded)compiled=compile(&s,&b,&d);
            check(!compiled&&d.code==expected);
            if(b.valid)build_close(&b);
        } else {
            check(loaded);
            if(loaded) {
                strcat(folder,"/OUTPUT.TXT");strcpy((char*)console_scratch,folder);
                capture=open((char*)console_scratch,O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
                if(capture>=0) {
                    check(!swap(capture,DEV_STDOUT));
                    execute_loaded(1,0,0);compiled=d.code==0;
                    check(!swap(capture,DEV_STDOUT));check(!close(capture));
                }
            }
        }
        check(pages_used()==code_required+1);
        corpus_error_path[0]=0;
        if(expected&&!compiled) {
            const char *failed=loaded?compile_error_path():path;
            if(failed)strcpy((char*)corpus_error_path,failed);
        }
        corpus_result[0]=(uint8_t)index;corpus_result[1]=(uint8_t)(index>>8);
        corpus_result[2]=loaded;corpus_result[3]=compiled;corpus_result[4]=d.code;
        corpus_result[5]=(uint8_t)d.start;corpus_result[6]=(uint8_t)(d.start>>8);
        corpus_result[7]=pages_used();corpus_checkpoint();
    }
close_manifest:
    check(!close(manifest));
done:
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
