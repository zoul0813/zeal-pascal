#include "harness.h"

/* Shared resident checks keep test-bank capacity independent of corpus size. */
void check(uint8_t ok) {
    ++test_index;
    if(ok) ++test_passed;
    else { if(test_failed<64) test_failures[test_failed]=test_index; ++test_failed; }
}
uint8_t load(const char *text) {
#ifdef ZP_SCREEN_LIBRARY
    static const char prefix[]="uses Screen;\n";
    uint8_t ok=source_import(&s,(const uint8_t*)prefix,sizeof(prefix)-1,&d);
    return ok&&source_replace(&s,s.length,s.length,(const uint8_t*)text,(uint16_t)strlen(text));
#else
    return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d);
#endif
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
void execution(const char *text,uint8_t kind,uint8_t code) {
    uint8_t ok=load(text);
    check(ok); if(ok) execute_loaded(kind,code,6);
}
void execution_file(const char *path,uint8_t kind,uint8_t code,uint16_t expected_position) {
    uint8_t ok=load_source(&s,path,&d);
    check(ok); if(ok) execute_loaded(kind,code,expected_position);
}
void checked_error(const char *text,uint8_t code,const char *where) {
    uint16_t position=(uint16_t)(strstr(text,where)-text);
#ifdef ZP_SCREEN_LIBRARY
    position+=13;
#endif
    uint8_t ok=load(text);
    check(ok); if(ok) execute_loaded(3,code,position);
}
