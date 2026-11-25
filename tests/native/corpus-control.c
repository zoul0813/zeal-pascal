#include "harness.h"
extern volatile uint8_t test_group;

/* No Pascal source pointers cross banks: execution_file captures its path. */
void control_tests(void) SOURCE_BLOCK_CALL {
    int8_t capture=open("H:/CONTROL.TXT",O_WRONLY|O_CREAT|O_TRUNC);
    check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    test_stage=9; execution_file("H:/CONTROL.PAS",1,0,0);
    test_stage=10;
    execution_file("H:/EAGERAND.PAS",3,2,6);
    execution_file("H:/EAGEROR.PAS",3,2,6);
    execution_file("H:/NESTED.PAS",3,2,34);
    if(capture>=0) { check(swap(capture,DEV_STDOUT)==0); check(close(capture)==0); }
}

uint8_t select_group(void) SOURCE_BLOCK_CALL {
    int8_t fd=open("H:/CORE.GRP",O_RDONLY);
    uint8_t value[2]={0,0};uint16_t bytes=2,failures=test_failed;
    if(fd<0)return 1;
    check(read(fd,value,&bytes)==0&&bytes==1);
    check(close(fd)==0);
    check(value[0]<=18);
    test_group=value[0];
    return test_failed==failures;
}

void corpus_check(uint8_t ok) SOURCE_BLOCK_CALL {
    ++test_index;
    if(ok) ++test_passed;
    else { if(test_failed<64) test_failures[test_failed]=test_index; ++test_failed; }
}

/* seed captures caller-bank strings into resident scratch before switching. */
uint8_t corpus_seed_write(int8_t fd) SOURCE_BLOCK_CALL {
    uint16_t n=(uint16_t)strlen((char*)console_scratch),expected=n;
    uint8_t ok=write(fd,console_scratch,&n)==0&&n==expected;
    return close(fd)==0&&ok;
}
