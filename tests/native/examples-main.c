/* Compile the actual example files and run saved native programs with replay
 * keyboard input. Each run gets a fresh capture descriptor; the database survives the two runs. */
#include "harness.h"
#include "image.h"
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
volatile uint8_t test_hash[32];
Source s; Build b; Diagnostic d;
static char binary[128];
void test_complete(void);
/* Use exactly the published native image, with the requested .bin name. */
static uint8_t publish_bin(void) {
    int8_t file;uint16_t offset=0,size,expected,i;uint8_t ok=1;
    strcpy(binary,s.path);strcpy(binary+strlen(binary)-4,".bin");
    file=open(binary,O_WRONLY|O_CREAT|O_TRUNC);if(file<0)return 0;
    while(offset<b.length&&ok) {
        size=b.length-offset;if(size>128)size=128;expected=size;
        for(i=0;i<size;++i)console_scratch[i]=image_get(&b,offset+i);
        if(write(file,console_scratch,&size)||size!=expected)ok=0;
        offset+=size;
    }
    if(close(file))ok=0;
    return ok;
}
static void replay(const char *output_path) {
    int8_t capture;
    uint8_t status=255,owned=pages_used();
    capture=open(output_path,O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) {
        check(swap(capture,DEV_STDOUT)==0);
        check(exec(EXEC_PRESERVE_PROGRAM,binary,0,&status)==0&&status==0);
        check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);
    }
    check(pages_used()==owned);
}
static void example(const char *path,const char *output_path) {
    check(load_source(&s,path,&d));
    if(!compile(&s,&b,&d)) {
        check(0);output("EXAMPLE COMPILE ");output(path);output(" error ");
        output_number(d.code);output(" at ");output_number(d.start);output("\n");return;
    }
    check(b.valid);check(save_build(&s,&b,0));
    if(publish_bin()) {check(1);replay(output_path);}else check(0);
}
int app_main(void) {
    if(!console_open())return 1;
    check(source_new(&s,"H:/lighthouse/lighthouse.pas"));
    test_stage=1;
    check(chdir("H:/lighthouse") == 0);
    example("H:/lighthouse/lighthouse.pas","H:/adventure.out");build_close(&b);
    test_stage=2;
    check(chdir("H:/partsbin") == 0);
    example("H:/partsbin/partsbin.pas","H:/parts.out");
    if(b.valid)replay("H:/reopen.out");build_close(&b);
    test_stage=3;
    check(chdir("H:/loan") == 0);
    example("H:/loan/loan.pas","H:/loan.out");
    if(b.valid)replay("H:/zero.out");build_close(&b);
    test_stage=4;
    check(chdir("H:/concord") == 0);
    example("H:/concord/concord.pas","H:/concord.out");build_close(&b);
    test_stage=5;
    check(chdir("H:/life") == 0);
    example("H:/life/life.pas","H:/life.out");build_close(&b);
    test_stage=6;
    check(chdir("H:/tasks") == 0);
    example("H:/tasks/tasks.pas","H:/tasks.out");build_close(&b);
    source_close(&s);check(pages_used()==code_required);
    test_done=255;test_complete();return 0;
}
