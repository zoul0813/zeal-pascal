#include "harness.h"

/* Shared resident checks keep test-bank capacity independent of corpus size. */
void check(uint8_t ok) {
    ++test_index;
    if(ok) ++test_passed;
    else { if(test_failed<64) test_failures[test_failed]=test_index; ++test_failed; }
}
uint8_t seed(const char *path,const char *text) {
    int8_t fd;
    uint16_t n=(uint16_t)strlen(text),expected=n;
    strcpy((char*)console_scratch,path); fd=open((char*)console_scratch,O_WRONLY|O_CREAT|O_TRUNC);
    if(fd<0) return 0;
    strcpy((char*)console_scratch,text);
    if(write(fd,console_scratch,&n)||n!=expected) { close(fd); return 0; }
    return close(fd)==0;
}
