#include "harness.h"

/* Production corpus keeps scalar checks in its test-control bank. */
#ifdef ZP_PRODUCTION_CORPUS
void corpus_check(uint8_t ok) SOURCE_BLOCK_CALL;
uint8_t corpus_seed_write(int8_t fd) SOURCE_BLOCK_CALL;
void check(uint8_t ok) { corpus_check(ok); }
#else
void check(uint8_t ok) {
    ++test_index;
    if(ok) ++test_passed;
    else { if(test_failed<64) test_failures[test_failed]=test_index; ++test_failed; }
}
#endif
#ifndef ZP_TEST_ASSERTIONS_ONLY
uint8_t seed(const char *path,const char *text) {
#ifdef ZP_PRODUCTION_CORPUS
    int8_t fd;
    strcpy((char*)console_scratch,path);
    fd=open((char*)console_scratch,O_WRONLY|O_CREAT|O_TRUNC);
    if(fd<0)return 0;
    strcpy((char*)console_scratch,text);
    return corpus_seed_write(fd);
#else
    int8_t fd;
    uint16_t n=(uint16_t)strlen(text),expected=n;
    strcpy((char*)console_scratch,path); fd=open((char*)console_scratch,O_WRONLY|O_CREAT|O_TRUNC);
    if(fd<0) return 0;
    strcpy((char*)console_scratch,text);
    if(write(fd,console_scratch,&n)||n!=expected) { close(fd); return 0; }
    return close(fd)==0;
#endif
}

#endif
