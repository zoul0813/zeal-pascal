#include "harness.h"

/* Publication and saved execution must select the same ordinary corpora. */
static uint8_t saved_corpus(void) {
    uint16_t stage=test_stage;
    /* Same odd stages9..53, with47 deliberately excluded. Compact enough
     * to keep the retained native cores linked in the corpus test image. */
    return stage>=9&&stage<=53&&(stage&1u)&&stage!=47;
}
void execute_loaded(uint8_t kind,uint8_t code,uint16_t expected_position) SOURCE_BLOCK_CALL {
    uint8_t owned,i;
    uint8_t before[7],after[7];
    uint16_t pc,position;zos_stat_t info;
    uint32_t revision;
    s.dirty=1; revision=s.revision;
    if(test_stage==3) { __asm di __endasm; }
    test_machine(before);
    check(compile(&s,&b,&d));
    test_machine(after);
    check(!memcmp(before,after,7)); /* compiler bank, stack and IFF restored */
    if(!b.valid) { if(test_stage==3) { __asm ei __endasm; } return; }
    if(test_stage==2) {
        check(seed("H:/SLICE.COM","old executable")); check(stat("H:/SLICE.ZMP",&info)==ERR_NO_SUCH_ENTRY);
        check(save_build(&s,&b,0));
    }
    if(test_stage==7||saved_corpus()) check(save_build(&s,&b,0));
    owned=pages_used();
    for(i=0;i<3;++i)if(b.pages[i]==255)++owned;
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
    check(build_check(&s,&b)); /* native program mutations remain part of this build */
    if(test_stage==2) {
        uint8_t status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/SLICE.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);
        status=255; check(run_saved(&s,&b,&status)&&status==0);
        check(!b.valid&&pages_used()==code_required+1);
    }
    if(test_stage==7) {
        uint8_t status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/NAME.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);
    }
    if(saved_corpus()) {
        uint8_t status=255;
        check(run_saved(&s,&b,&status)&&status==(kind==3?code:kind==5?1:0));
        check(!b.valid&&pages_used()==code_required+1);
    }
    build_close(&b);
    check(pages_used()==code_required+1);
}
