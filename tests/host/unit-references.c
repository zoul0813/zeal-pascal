#include "compiler_units.h"
#include <assert.h>
#include <stdio.h>
Compiler cc;
static Diagnostic diagnostic;
static uint8_t relocations[64],catalog[256],code[256];
static unsigned checks,polls,cancel;
static void check(uint8_t ok){++checks;assert(ok);}
void cc_fail(uint8_t error){if(!diagnostic.code)diagnostic.code=error;}
uint8_t cc_poll(void){if(++polls==cancel){cc_fail(COMPILE_INTERRUPTED);return 0;}return 1;}
uint8_t cc_reloc_get(uint16_t at){assert(at<sizeof(relocations));return relocations[at];}
void cc_reloc_put(uint16_t at,uint8_t value){assert(at<sizeof(relocations));relocations[at]=value;}
uint16_t cc_unit_word(uint16_t at){assert(at+1<sizeof(catalog));return catalog[at]|((uint16_t)catalog[at+1]<<8);}
uint8_t cc_code_get(uint16_t at){assert(at>=CC_CODE_BASE&&at<CC_CODE_BASE+sizeof(code));return code[at-CC_CODE_BASE];}
static void put(uint8_t *bytes,uint16_t at,uint16_t value){bytes[at]=(uint8_t)value;bytes[at+1]=(uint8_t)(value>>8);}
static void reset(void){memset(&cc,0,sizeof(cc));memset(&diagnostic,0,sizeof(diagnostic));memset(relocations,0,sizeof(relocations));memset(catalog,0,sizeof(catalog));memset(code,0,sizeof(code));cc.error=&diagnostic;cc.emitted=CC_CODE_BASE+sizeof(code);polls=cancel=0;}
static void node(uint16_t at,uint16_t kind,uint16_t site,uint16_t value,uint16_t next){put(catalog,at,kind);put(catalog,at+2,site);put(catalog,at+4,value);put(catalog,at+6,next);}
static void relocation(uint8_t index,uint16_t site,uint16_t target){put(relocations,index*4,site);put(relocations,index*4+2,1);put(code,site,target);}
int main(void){
    UnitReference cursor;uint8_t i,j,permutation[16],swap;unsigned trial;uint32_t random=12345;uint16_t expected[]={0x8001,0x8004,0x8008,0x8010,0x8018};
    reset();cc.relocations=3;relocation(0,24,0x8090);relocation(1,1,0x8080);relocation(2,8,0x80a0);
    put(catalog,UH_REFERENCES,32);node(32,UR_GLOBAL,0x8004,0x8123,48);node(48,UR_EXTERNAL,0x8010,99,0);
    check(cc_unit_relocations_prepare());
    cc_unit_reference_begin(&cursor);
    for(i=0;i<5;++i){check(cursor.kind&&cursor.site==expected[i]);cc_unit_reference_next(&cursor);}check(!cursor.kind&&!diagnostic.code);
    cc_unit_reference_begin(&cursor);check(cursor.kind==UR_CODE&&cursor.value==0x8080);
    cc_unit_reference_next(&cursor);check(cursor.kind==UR_GLOBAL&&cursor.value==0x8123&&cursor.record==32);
    cc_unit_reference_next(&cursor);check(cursor.kind==UR_CODE&&cursor.value==0x80a0);
    cc_unit_reference_next(&cursor);check(cursor.kind==UR_EXTERNAL&&cursor.value==99&&cursor.record==48);
    /* The external call record replaces an ordinary fixup at the same site. */
    reset();cc.relocations=2;relocation(0,16,0);relocation(1,20,0x8088);put(catalog,UH_REFERENCES,32);node(32,UR_EXTERNAL,0x8010,99,0);
    check(cc_unit_relocations_prepare());cc_unit_reference_begin(&cursor);check(cursor.kind==UR_EXTERNAL&&cursor.relocation==1);
    cc_unit_reference_next(&cursor);check(cursor.kind==UR_CODE&&cursor.site==0x8014);cc_unit_reference_next(&cursor);check(!cursor.kind&&!diagnostic.code);
    reset();cc.relocations=1;relocation(0,4,0);put(catalog,UH_REFERENCES,32);node(32,UR_GLOBAL,0x8004,0,0);
    check(cc_unit_relocations_prepare());cc_unit_reference_begin(&cursor);check(diagnostic.code==UNIT_FORMAT);
    reset();cc.relocations=2;relocation(0,4,0);relocation(1,4,0);check(!cc_unit_relocations_prepare()&&diagnostic.code==UNIT_FORMAT);
    reset();cc.relocations=1;put(relocations,0,0x8000);put(relocations,2,1);check(!cc_unit_relocations_prepare()&&diagnostic.code==UNIT_UNSUPPORTED);
    reset();cc.relocations=1;relocation(0,2,0);put(relocations,2,2);check(!cc_unit_relocations_prepare()&&diagnostic.code==UNIT_UNSUPPORTED);
    reset();cc.relocations=1;put(relocations,0,255);put(relocations,2,1);check(!cc_unit_relocations_prepare()&&diagnostic.code==UNIT_FORMAT);
    reset();cc.relocations=1;relocation(0,2,0);cancel=1;check(!cc_unit_relocations_prepare()&&diagnostic.code==COMPILE_INTERRUPTED);
    reset();check(cc_unit_relocations_prepare());cc_unit_reference_begin(&cursor);check(!cursor.kind&&!diagnostic.code);
    for(trial=0;trial<100;++trial) {
        reset();cc.relocations=16;
        for(i=0;i<16;++i)permutation[i]=i;
        for(i=15;i;--i){random=random*1664525u+1013904223u;j=(uint8_t)(random%(i+1u));swap=permutation[i];permutation[i]=permutation[j];permutation[j]=swap;}
        for(i=0;i<16;++i)relocation(i,1+4*permutation[i],0x8080+permutation[i]);
        check(cc_unit_relocations_prepare());cc_unit_reference_begin(&cursor);
        for(i=0;i<16;++i){check(cursor.kind==UR_CODE&&cursor.site==0x8001+4*i&&cursor.value==0x8080+i);cc_unit_reference_next(&cursor);}
        check(!cursor.kind&&!diagnostic.code);
    }
    printf("%u unit reference assertions passed\n",checks);return 0;
}
