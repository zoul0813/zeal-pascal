#include "compiler_units.h"

/* Uses validation already resolved every wire type. Symbol cache misses must
 * use that retained catalog, without loading the transient decoder while
 * expression or file generation owns the remaining physical pages. */
#define EXPORT_HEADER 14u
static TypeId resolved_type(uint16_t module,TypeId wire) {
    uint16_t at,end=cc_unit_word(UH_USED);
    if(wire<6)return wire;
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
        if(cc_unit_word(at)==UR_TYPE&&cc_unit_word(at+2)==module&&cc_unit_word(at+4)==wire&&cc_unit_word(at+8)==2)
            return cc_unit_word(at+6);
    cc_fail(UNIT_FORMAT);return TYPE_NONE;
}
int16_t cc_unit_import_symbol(uint16_t module,uint16_t export) ROUTINES {
    uint16_t scope=0,symbol,count,i,id,parameters,data;uint8_t role;
    TypeId type;
    /* The retained namespace calls this decoder only on a symbol-cache miss. */
    role=cc_unit_module_byte(module,export+1);count=cc_unit_module_word(module,export+6);
    type=cc_unit_module_word(module,export+4);if(type!=TYPE_NONE)type=resolved_type(module,type);
    if(cc.error->code)return -1;
    if((uint32_t)cc.symbol_count+1+count>SYMBOL_LIMIT){cc_fail(99);return -1;}
    symbol=cc.symbol_count++;cc_symbol_clear(symbol);CCS_SET(symbol,type,type);CCS_SET(symbol,owner,0);
    parameters=export+EXPORT_HEADER+cc_unit_module_byte(module,export);data=parameters+4*count;
    if(role>=2) {
        CCS_SET(symbol,role,role==2?S_CONSTANT:S_TYPE);CCS_SET(symbol,address,cc_unit_module_word(module,export+10));
        if(role==2&&cc_unit_module_word(module,export+12))CCS_SET(symbol,address,cc_source_reference((((SourcePosition)(0x8000u|module)<<16)|data)));
    } else {
        if(cc.routine_count==65535u){cc_fail(99);return -1;}
        scope=++cc.routine_count;cc_routine_clear(scope);CCS_SET(symbol,role,role?S_FUNCTION:S_PROCEDURE);CCS_SET(symbol,initial_type,scope);
        cc_rput(scope,R_SYMBOL,symbol);cc_rput(scope,R_FLAGS,RF_DEFINED|RF_UNIT_IMPORT);
        /* Import nested types before allocating the contiguous parameter block. */
        for(i=0;i<count&&!cc.error->code;++i) {
            type=cc_unit_module_word(module,parameters+4*i+2);if(type!=TYPE_NONE)resolved_type(module,type);
        }
        if((uint32_t)cc.symbol_count+count>SYMBOL_LIMIT){cc_fail(99);return -1;}
        cc_rput(scope,R_PARAMETERS,cc.symbol_count);cc_rput(scope,R_COUNT,count);
        for(i=0;i<count&&!cc.error->code;++i) {
            id=cc.symbol_count++;cc_symbol_clear(id);CCS_SET(id,owner,scope);
            CCS_SET(id,role,cc_unit_module_byte(module,parameters+4*i)?S_REFERENCE:S_VALUE);
            type=cc_unit_module_word(module,parameters+4*i+2);CCS_SET(id,type,type==TYPE_NONE?type:resolved_type(module,type));
        }
    }
    if(!cc_unit_record(UR_IMPORT,scope,module,export,0,symbol))return -1;
    return (int16_t)symbol;
}
