#include "compiler_units.h"

/* Compile-time metadata lives in the sealed artifact, never in emitted code.
 * Type IDs are local to a file; imported aliases retain their canonical owner. */
#define TYPE_HEADER 18u
#define MEMBER_HEADER 10u
#define EXPORT_HEADER 14u
static void put_byte(uint8_t page,uint16_t *at,uint8_t value) {
    if(*at>=ZPU_ARTIFACT_LIMIT){cc_fail(99);return;}far_set(page,(*at)++,value);
}
static void put_word(uint8_t page,uint16_t *at,uint16_t value) {
    put_byte(page,at,(uint8_t)value);put_byte(page,at,(uint8_t)(value>>8));
}
static void put_name(uint8_t page,uint16_t *at,uint16_t symbol) {
    uint8_t i,c;SourcePosition start=cc_reference_position(CCS(symbol,name_start));
    for(i=0;i<CCS(symbol,name_length);++i) {
        c=cc_source_byte(start+i);put_byte(page,at,c>='A'&&c<='Z'?c+32:c);
    }
}
static uint16_t literal_bytes(uint16_t symbol) {
    if(CCS(symbol,role)!=S_CONSTANT)return 0;
    if(cc_family(CCS(symbol,type))==K_REAL)return 6;
    if(cc_family(CCS(symbol,type))==K_STRING){cc_constant_literal(CCS(symbol,address));return 1+cc.literal_length;}
    return 0;
}
uint32_t cc_unit_exports_bytes(void) ROUTINES {
    uint16_t at,end=cc_unit_word(UH_USED),symbol,scope;uint32_t bytes=0;
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_EXPORT) {
        symbol=cc_unit_word(at+2);scope=cc_unit_word(at+4);
        bytes+=EXPORT_HEADER+CCS(symbol,name_length)+literal_bytes(symbol)+(scope?4UL*cc_rget(scope,R_COUNT):0);
    }
    return bytes;
}
void cc_unit_exports_write(uint8_t page,uint16_t *at) ROUTINES {
    uint16_t record,end=cc_unit_word(UH_USED),symbol,scope,first,count,fragment,offset,n,i;
    uint8_t role;
    for(record=UNIT_RECORD_BASE;record<end&&!cc.error->code;record+=UNIT_RECORD_BYTES)if(cc_unit_word(record)==UR_EXPORT) {
        symbol=cc_unit_word(record+2);scope=cc_unit_word(record+4);role=CCS(symbol,role);
        count=scope?cc_rget(scope,R_COUNT):0;first=scope?cc_rget(scope,R_PARAMETERS):0;
        fragment=offset=0;
        if(scope&&!cc_unit_fragment(cc_rget(scope,R_ENTRY),&fragment,&offset))return;
        if(role==S_CONSTANT)offset=CCS(symbol,address);
        n=literal_bytes(symbol);
        put_byte(page,at,CCS(symbol,name_length));put_byte(page,at,role==S_FUNCTION?1:role==S_CONSTANT?2:role==S_TYPE?3:0);
        put_byte(page,at,0);put_byte(page,at,0);put_word(page,at,role==S_PROCEDURE?TYPE_NONE:CCS(symbol,type));
        put_word(page,at,count);put_word(page,at,fragment);put_word(page,at,n?0:offset);put_word(page,at,n);
        put_name(page,at,symbol);
        for(i=0;i<count;++i) {
            put_byte(page,at,CCS(first+i,role)==S_REFERENCE);put_byte(page,at,0);put_word(page,at,CCS(first+i,type));
        }
        if(n&&cc_family(CCS(symbol,type))==K_REAL) {
            cc_real_literal(CCS(symbol,address));for(i=0;i<6;++i)put_byte(page,at,zr_result[i]);
        } else if(n) {
            put_byte(page,at,(uint8_t)cc.literal_length);for(i=0;i<cc.literal_length;++i)put_byte(page,at,cc.literal[i]);
        }
    }
}
int16_t cc_unit_import_symbol(uint16_t module,uint16_t export) ROUTINES {
    uint16_t at,end=cc_unit_word(UH_USED),scope=0,symbol,count,i,id,parameters,data;uint8_t role;
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
        if(cc_unit_word(at)==UR_IMPORT&&cc_unit_word(at+4)==module&&cc_unit_word(at+6)==export)
            return (int16_t)cc_unit_word(at+10);
    count=cc_unit_module_word(module,export+6);role=cc_unit_module_byte(module,export+1);
    if((uint32_t)cc.symbol_count+1+count>SYMBOL_LIMIT){cc_fail(99);return -1;}
    symbol=cc.symbol_count++;cc_symbol_clear(symbol);CCS_SET(symbol,owner,0);
    CCS_SET(symbol,type,cc_unit_module_word(module,export+4));
    parameters=export+EXPORT_HEADER+cc_unit_module_byte(module,export);data=parameters+4*count;
    if(role==2) {
        CCS_SET(symbol,role,S_CONSTANT);CCS_SET(symbol,address,cc_unit_module_word(module,export+10));
        if(cc_unit_module_word(module,export+12))CCS_SET(symbol,address,
            cc_source_reference(((SourcePosition)(0x8000u|module)<<16)|data));
    } else {
        if(cc.routine_count==65535u){cc_fail(99);return -1;}
        scope=++cc.routine_count;cc_routine_clear(scope);
        CCS_SET(symbol,role,role?S_FUNCTION:S_PROCEDURE);CCS_SET(symbol,initial_type,scope);
        cc_rput(scope,R_SYMBOL,symbol);cc_rput(scope,R_FLAGS,RF_DEFINED|RF_UNIT_IMPORT);
        cc_rput(scope,R_PARAMETERS,cc.symbol_count);cc_rput(scope,R_COUNT,count);
        for(i=0;i<count;++i) {
            id=cc.symbol_count++;cc_symbol_clear(id);CCS_SET(id,owner,scope);
            CCS_SET(id,role,cc_unit_module_byte(module,parameters+4*i)?S_REFERENCE:S_VALUE);
            CCS_SET(id,type,cc_unit_module_word(module,parameters+4*i+2));
        }
    }
    if(!cc_unit_record(UR_IMPORT,scope,module,export,0,symbol))return -1;
    return (int16_t)symbol;
}
