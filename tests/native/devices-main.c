#include "harness.h"
#include <zos_serial.h>
volatile uint16_t test_passed,test_failed,test_stage;
volatile uint8_t test_done;
volatile uint16_t test_failures[64],test_index;
Source s; Build b; Diagnostic d;
void test_complete(void);
int app_main(void) {
    int8_t capture,serial;
    uint8_t attribute,original;
    if(!console_open()) return 1;
    check(source_new(&s,"H:/DEVOUTPUT.PAS"));
    capture=open("H:/DEVS.TXT",O_WRONLY|O_CREAT|O_TRUNC);check(capture>=0);
    if(capture>=0) check(swap(capture,DEV_STDOUT)==0);
    serial=open("#SER0",O_RDWR);check(serial>=0);
    if(serial>=0) {
        check(ioctl(serial,SERIAL_CMD_GET_ATTR,&original)==0);
        check(ioctl(serial,SERIAL_CMD_SET_ATTR,0)==0);
        check(close(serial)==0);
    }
    test_stage=41;
    execution_file("H:/DEVOUTPUT.PAS",1,0,0);
    execution_file("H:/DEVFAIL.PAS",3,0x20,31);
    serial=open("#SER0",O_RDWR);check(serial>=0);
    if(serial>=0) {
        check(ioctl(serial,SERIAL_CMD_GET_ATTR,&attribute)==0&&attribute==0);
        check(ioctl(serial,SERIAL_CMD_SET_ATTR,(void *)(uint16_t)original)==0);
        check(close(serial)==0);
    }
    if(capture>=0) {check(swap(capture,DEV_STDOUT)==0);check(close(capture)==0);}
    source_close(&s);check(pages_used()==code_required);
    test_stage=6;test_done=255;test_complete();return 0;
}
