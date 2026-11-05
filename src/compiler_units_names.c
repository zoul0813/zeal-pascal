#include "compiler_units.h"
#include "library.h"

/* Unit namespace and primitive signature materialization share the compiler
 * metadata bank. Cached object bytes and state stay in owned far pages. */
static uint8_t module_name(uint16_t module,const char *name) {
    uint8_t i,n=cc_unit_module_byte(module,18);
    if(strlen(name)!=n)return 0;
    for(i=0;i<n;++i)if(cc_unit_module_byte(module,ZPU_HEADER_BYTES+i)!=(uint8_t)name[i])return 0;
    return 1;
}
static uint16_t find_module(const char *name) {
    uint16_t at,end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
        if(cc_unit_word(at)==UR_MODULE&&cc_unit_word(at+8)&&module_name(at,name))return at;
    return 0;
}
void cc_units_imports(void) ROUTINES {
    char name[128],filename[16],directory[128],path[128];uint16_t i,n;
    if(cc.sources->unit_mode||(cc.sources->options&&cc.sources->options->kind==COMPILE_CHAIN)) {
        cc_fail(UNIT_UNSUPPORTED);return;
    }
    if(!cc_units_begin())return;
    cc_next();
    for(;;) {
        if(cc.token!=T_ID){cc_fail(53);return;}
        n=(uint16_t)strlen(cc.name);if(!n||n>11){cc_fail(UNIT_UNSUPPORTED);return;}
        strcpy(name,cc.name);
        if(!find_module(name)) {
            if(cc_unit_word(UH_MODULES)==CC_FRAGMENT_OBJECTS){cc_fail(99);return;}
            for(i=0;i<n;++i)filename[i]=name[i]>='a'&&name[i]<='z'?name[i]-32:name[i];
            strcpy(filename+n,".ZPU");strcpy(directory,cc.input->path);
            i=(uint16_t)strlen(directory);while(i&&directory[i-1]!='/')--i;directory[i]=0;
            if(!sources_path(filename,directory,path)){cc_fail(UNIT_MISSING);return;}
            if(!cc_units_load(name,path))return;
        }
        cc_next();if(cc.token!=',')break;cc_next();
    }
    cc_expect(';',1);
}
static uint16_t next_export(uint16_t module,uint16_t at) {
    return at+10+cc_unit_module_byte(module,at)+2*cc_unit_module_word(module,at+4);
}
static uint16_t find_export(uint16_t module,const char *name) {
    uint16_t at=cc_unit_word(module+10),count=cc_unit_module_word(module,20),i,j,n;
    for(i=0;i<count;++i,at=next_export(module,at)) {
        n=cc_unit_module_byte(module,at);if(strlen(name)!=n)continue;
        for(j=0;j<n;++j)if(cc_unit_module_byte(module,at+10+j)!=(uint8_t)name[j])break;
        if(j==n)return at;
    }
    return 0;
}
static TypeId primitive_type(uint8_t tag) {
    return tag==0?K_INTEGER:tag==1?K_BYTE:tag==2?K_CHAR:tag==3?K_BOOLEAN:tag==4?K_REAL:TYPE_NONE;
}
static int16_t materialize(uint16_t module,uint16_t export) {
    uint16_t at,end=cc_unit_word(UH_USED),scope,symbol,count,i,id,parameters;
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
        if(cc_unit_word(at)==UR_IMPORT&&cc_unit_word(at+4)==module&&cc_unit_word(at+6)==export)
            return (int16_t)cc_unit_word(at+10);
    count=cc_unit_module_word(module,export+4);
    if(cc.routine_count==65535u||(uint32_t)cc.symbol_count+1+count>SYMBOL_LIMIT){cc_fail(99);return -1;}
    scope=++cc.routine_count;cc_routine_clear(scope);symbol=cc.symbol_count++;cc_symbol_clear(symbol);
    CCS_SET(symbol,role,cc_unit_module_byte(module,export+1)?S_FUNCTION:S_PROCEDURE);
    CCS_SET(symbol,type,primitive_type(cc_unit_module_byte(module,export+2)));
    CCS_SET(symbol,initial_type,scope);CCS_SET(symbol,owner,0);
    cc_rput(scope,R_SYMBOL,symbol);cc_rput(scope,R_FLAGS,RF_DEFINED|RF_UNIT_IMPORT);
    cc_rput(scope,R_PARAMETERS,cc.symbol_count);cc_rput(scope,R_COUNT,count);
    parameters=export+10+cc_unit_module_byte(module,export);
    for(i=0;i<count;++i) {
        id=cc.symbol_count++;cc_symbol_clear(id);CCS_SET(id,owner,scope);
        CCS_SET(id,role,cc_unit_module_byte(module,parameters+2*i)?S_REFERENCE:S_VALUE);
        CCS_SET(id,type,primitive_type(cc_unit_module_byte(module,parameters+2*i+1)));
    }
    if(!cc_unit_record(UR_IMPORT,scope,module,export,0,symbol))return -1;
    return (int16_t)symbol;
}
int16_t cc_units_lookup(void) ROUTINES {
    uint16_t module=0,export=0,at,end,match=0,candidate=0;SourceCursor cursor;
    if(cc.sources->unit_page==255||cc.sources->unit_mode||cc.token!=T_ID)return -1;
    if(!cc.sources->unit_qualifier&&(module=find_module(cc.name))!=0) {
        cc_source_save(&cursor);cc_next();
        if(cc.token=='.') {
            cc_next();if(cc.token!=T_ID){cc_fail(53);return -1;}
            cc.sources->unit_qualifier=cc_unit_word(module+8);
        } else {cc_source_restore(&cursor);module=0;}
    }
    end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_MODULE&&cc_unit_word(at+8)) {
        if(cc.sources->unit_qualifier&&cc.sources->unit_qualifier!=cc_unit_word(at+8))continue;
        export=find_export(at,cc.name);
        if(export) {
            if(match){cc_fail(UNIT_AMBIGUOUS);return -1;}
            match=at;candidate=export;
        }
    }
    if(!match){if(cc.sources->unit_qualifier)cc_fail(UNIT_INTERFACE);return -1;}
    return materialize(match,candidate);
}
void cc_units_call(uint8_t opcode,uint16_t scope) ROUTINES {
    uint16_t at,end=cc_unit_word(UH_USED),site;
    if(cc.suppress||cc.error->code)return;
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_IMPORT&&cc_unit_word(at+2)==scope) {
        site=cc_link(opcode,cc_unit_word(at+8));cc_unit_put(at+8,site);return;
    }
    cc_fail(UNIT_INTERFACE);
}


void cc_unit_descriptor(uint16_t module,CompilerFragmentObject *object) ROUTINES {
    uint16_t seal=module+UNIT_RECORD_BYTES;
    object->page=(uint8_t)cc_unit_word(module+2);object->offset=cc_unit_word(seal+10);
    object->bytes=cc_unit_word(module+4)-object->offset;
    object->lifetime=cc_unit_word(seal+2)|((uint32_t)cc_unit_word(seal+4)<<16);
    object->version=cc_unit_word(seal+6)|((uint32_t)cc_unit_word(seal+8)<<16);
    object->binding=cc_unit_word(module+8);
}
/* Release the I/O code page only after returning to the retained unit bank.
 * Failed release keeps its owned handle for the normal phase/terminal retry. */
static uint8_t io_finish(uint8_t ok) {
    uint8_t primary=platform_error;
    if(!code_phase_unit_io_release()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    if(!ok)platform_error=primary;
    return ok;
}
uint8_t cc_units_write(void) ROUTINES {
    uint8_t ok;
    if(!code_phase_unit_io()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    ok=cc_units_write_object();return io_finish(ok);
}
uint16_t cc_units_load(const char *name,const char *path) ROUTINES {
    uint16_t module;
    if(!code_phase_unit_io()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    module=cc_units_load_object(name,path);
    return io_finish(module!=0)?module:0;
}
