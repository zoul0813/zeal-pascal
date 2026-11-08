#include "compiler_units.h"
#ifdef ZP_STATIC_LAYOUT
#include "file-unit.h"
#endif
#if defined(__SDCC)
#pragma codeseg BANK9
#pragma constseg BANK9
#endif
/* Names belong to this permanent coordinator. Only retained stack strings
 * cross graph-load calls that replace compiler banks. */
uint8_t cc_file_native_prepare(void) ROUTINES {
#ifdef ZP_STATIC_LAYOUT
    static const char *const handlers[34]={
        0,"reset","rewrite","close","read","write","seek","position","position","position","flush","erase",
        "blocks","blocks","blocks","blocks",0,0,0,0,"text_read","text_write","text_readln","text_write",
        "predicate","predicate","predicate","rename","begin",0,"path",0,0,"path"};
    char name[12];uint16_t module,export;uint8_t op;
    if(!cc.file_entry||cc.error->code)return !cc.error->code;
    if(!cc_units_begin())return 0;
    strcpy(name,"sysfile");module=cc_units_graph(name,0);if(!module)return 0;
    if((cc_unit_module_word(module,28)|((uint32_t)cc_unit_module_word(module,30)<<16))!=SYSFILE_CHECKSUM){cc_fail(UNIT_STALE);return 0;}
    export=cc_unit_export(module,"dispatch");if(!export){cc_fail(UNIT_FORMAT);return 0;}
    cc_unit_record(UR_NATIVE,module,export,cc.file_native_operand,0,0);
    export=cc_unit_export(module,"table");if(!export){cc_fail(UNIT_FORMAT);return 0;}
    cc_unit_record(UR_NATIVE,module,export,0,65535u,0);
    for(op=0;op<34&&!cc.error->code;++op)if(handlers[op]&&(cc.file_operations[op>>3]&(1u<<(op&7u)))) {
        export=cc_unit_export(module,handlers[op]);if(!export){cc_fail(UNIT_FORMAT);return 0;}
        cc_unit_record(UR_NATIVE,module,export,0,op+1u,0);
    }
    if(cc.error->code||!cc_units_link()||!cc_units_close())return 0;
    if(!sources_finish(&cc.image->sources)){cc_fail(99);return 0;}
#endif
    return !cc.error->code;
}

#ifdef ZP_UNITS_EXPERIMENT
static uint8_t native_location(const CompilerFragmentLayout *layout,uint16_t module,uint16_t fragment,uint16_t *address) {
    if(cc_fragment_layout_get(layout,cc_unit_word(module+8),fragment,address)==CF_OK)return 1;
    cc_fail(UNIT_FORMAT);return 0;
}
uint8_t cc_file_native_bind(const CompilerFragmentLayout *layout,uint16_t end) ROUTINES {
    uint16_t at,module,export,address,offset,i;
    /* The table anchor's relocated operand names private native state. It is
     * not executed by the program; selected handler pointers bind here. */
    offset=0;
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_NATIVE&&cc_unit_word(at+8)==65535u) {
        module=cc_unit_word(at+2);export=cc_unit_word(at+4);
        if(!native_location(layout,module,cc_unit_module_word(module,export+8),&address)){cc_fail(UNIT_FORMAT);return 0;}
        address+=cc_unit_module_word(module,export+10);
        if(cc_code_get(address)!=0x21||cc_code_get(address+3)!=0xc9){cc_fail(UNIT_FORMAT);return 0;}
        offset=cc_code_get(address+1)|((uint16_t)cc_code_get(address+2)<<8);
    }
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_NATIVE&&cc_unit_word(at+8)!=65535u) {
        module=cc_unit_word(at+2);export=cc_unit_word(at+4);
        if(!native_location(layout,module,cc_unit_module_word(module,export+8),&address)){cc_fail(UNIT_FORMAT);return 0;}
        address+=cc_unit_module_word(module,export+10);i=cc_unit_word(at+8);
        if(!i)cc_patch_chain(cc_unit_word(at+6),address);
        else {
            if(!offset||i>34||offset<CC_CODE_BASE||(uint32_t)offset+68u>cc.emitted){cc_fail(UNIT_FORMAT);return 0;}
            cc_word(offset+2u*(i-1u),address);cc_relocate(offset+2u*(i-1u));
        }
    }
    return !cc.error->code;
}

#endif
