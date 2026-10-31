#include "harness.h"

/* Shared resident checks keep test-bank capacity independent of corpus size. */
void check(uint8_t ok) {
    ++test_index;
    if(ok) ++test_passed;
    else { if(test_failed<64) test_failures[test_failed]=test_index; ++test_failed; }
}
uint8_t load(const char *text) { return source_import(&s,(const uint8_t*)text,(uint16_t)strlen(text),&d); }
uint8_t seed(const char *path,const char *text) {
    int8_t fd;
    uint16_t n=(uint16_t)strlen(text),expected=n;
    strcpy((char*)console_scratch,path); fd=open((char*)console_scratch,O_WRONLY|O_CREAT|O_TRUNC);
    if(fd<0) return 0;
    strcpy((char*)console_scratch,text);
    if(write(fd,console_scratch,&n)||n!=expected) { close(fd); return 0; }
    return close(fd)==0;
}
/* Publication and saved execution must select the same ordinary corpora. */
static uint8_t saved_corpus(void) {
    uint16_t stage=test_stage;
    /* Same odd stages9..53, with47 deliberately excluded. Compact enough
     * to keep the retained native cores linked in the corpus test image. */
    return stage>=9&&stage<=53&&(stage&1u)&&stage!=47;
}
void execute_loaded(uint8_t kind,uint8_t code,uint16_t expected_position) {
    uint8_t owned;
    uint8_t before[7],after[7];
    uint16_t pc,position;
    uint32_t revision;
    s.dirty=1; revision=s.revision;
    if(test_stage==3) { __asm di __endasm; }
    test_machine(before);
    check(compile(&s,&b,&d));
    test_machine(after);
    check(!memcmp(before,after,7)); /* compiler bank, stack and IFF restored */
    if(!b.valid) { if(test_stage==3) { __asm ei __endasm; } return; }
    if(test_stage==2) {
        check(seed("H:/SLICE.COM","old executable")); check(seed("H:/SLICE.ZMP","old map"));
        check(save_build(&s,&b,0));
    }
    if(test_stage==7||saved_corpus()) check(save_build(&s,&b,0));
    owned=pages_used();
    test_machine(before);
    check(run_build(&s,&b));
    test_machine(after);
    if(test_stage==3) { __asm ei __endasm; }
    check(!memcmp(before,after,7));
    check(!memcmp(before+3,ram_result+8,4));
    check(before[0]==ram_result[15]);
    check(ram_result[26]==kind&&ram_result[28]==code);
    if(kind==5) check(ram_result[32]==1);
    check(s.dirty&&s.revision==revision&&pages_used()==owned);
    if(kind==3||kind==5) {
        pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
        check(build_locate(&s,&b,pc,&position));
        check(position==expected_position);
    }
    check(build_check(&s,&b)); /* writable run pages cannot mutate cached image */
    if(test_stage==2) {
        uint8_t status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/SLICE.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);
        status=255; check(run_saved(&s,&b,&status)&&status==0);
        check(pages_used()==owned);
    }
    if(test_stage==7) {
        uint8_t status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/NAME.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);
    }
    if(saved_corpus()) {
        uint8_t status=255;
        check(run_saved(&s,&b,&status)&&status==(kind==3?code:kind==5?1:0));
        check(pages_used()==owned);
    }
    build_close(&b);
    check(pages_used()==code_required+1);
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
    uint8_t ok=load(text);
    check(ok); if(ok) execute_loaded(3,code,position);
}
