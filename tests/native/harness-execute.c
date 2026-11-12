#include "harness.h"
#include "program/decode.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "source/catalog.h"
#endif

/* Publication and saved execution must select the same ordinary corpora. */
static uint8_t saved_corpus(void) {
    uint16_t stage=test_stage;
    /* Same odd stages9..53, with47 deliberately excluded. Compact enough
     * to keep the retained native cores linked in the corpus test image. */
    return stage>=9&&stage<=53&&(stage&1u)&&stage!=47;
}
/* Assert the explicit warm-cache contract for each compiler variant. */
uint8_t cached_code_pages(void) SOURCE_BLOCK_CALL {
    uint8_t count=code_required;
#ifdef ZP_OPTIONAL_REAL
    --count;
#endif
#ifdef ZP_UNITS_EXPERIMENT
    count-=3; /* Artifact I/O, dependency/type metadata and linker are transient. */
#endif
#ifdef ZP_OVERLAYS
    --count; /* Ordinary corpus programs do not demand the overlay compiler. */
#endif
    return count;
}
void execute_loaded(uint8_t kind,uint8_t code,uint16_t expected_position) SOURCE_BLOCK_CALL {
    uint8_t owned,i;
#ifdef ZP_UNITS_EXPERIMENT
    uint8_t unit_error=kind&0x80u;kind&=0x7fu;
#endif
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
    for(i=0;i<3;++i)if(b.pages[i]==255&&
        !(i&&(far_get(b.pages[0],18)&NATIVE_SINGLE_PAGE))&&
        !(i==2&&(far_get(b.pages[0],18)&NATIVE_SHARED_STACK)))++owned;
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
#ifdef ZP_UNITS_EXPERIMENT
        if(unit_error) {
            uint8_t file=0;Source unit;
            check(build_location(&s,&b,pc,&file,&position)&&file!=0);
            check(file&&sources_get(&b.sources,file,&unit)&&
                  strcmp(unit.path,s.path)&&position==expected_position&&position<unit.length);
        } else
#endif
        {
        check(build_locate(&s,&b,pc,&position));
        check(position==expected_position);
        }
    }
    check(build_check(&s,&b)); /* native program mutations remain part of this build */
    if(test_stage==2) {
        uint8_t status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/SLICE.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);
        status=255; check(run_saved(&s,&b,&status)&&status==0);
        check(!b.valid&&pages_used()==cached_code_pages()+1);
    }
    if(test_stage==7) {
        uint8_t status=255;
        check(exec(EXEC_PRESERVE_PROGRAM,"H:/NAME.COM",0,&status)==0&&status==0);
        check(pages_used()==owned);
    }
    if(saved_corpus()) {
        uint8_t status=255;
        check(run_saved(&s,&b,&status)&&status==(kind==3?code:kind==5?1:0));
        check(!b.valid&&pages_used()==cached_code_pages()+1);
    }
    build_close(&b);
    check(pages_used()==cached_code_pages()+1);
}
