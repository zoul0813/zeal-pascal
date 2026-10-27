#include "editor.h"
#include "library.h"

volatile uint16_t test_passed,test_failed,test_stage,test_index,test_failures[64];
volatile uint8_t test_done;
Source s;
void test_machine(uint8_t *state) NATIVE;
void test_complete(void);
void editor_checks(Source *source);
void check(uint8_t ok) {
    ++test_index;
    if(ok) ++test_passed;
    else { if(test_failed<64) test_failures[test_failed]=test_index; ++test_failed; }
}
int app_main(void) {
    uint8_t before[7],after[7];
    check(source_new(&s,"H:/EDITOR.PAS"));
    test_stage=40;
    test_machine(before); editor_checks(&s); test_machine(after);
    check(!memcmp(before,after,7));
    source_close(&s); check(pages_used()==code_required);
    test_stage=6; test_done=255; test_complete(); return 0;
}
