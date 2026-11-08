#include "compiler_units.h"
#include "library.h"

/* The retained namespace owns visibility and cached symbol IDs. A transient
 * metadata decoder handles cache misses; artifacts/state own far pages. */
static uint8_t module_name(uint16_t module,const char *name) {
    uint8_t i,n=cc_unit_module_byte(module,18);
    if(strlen(name)!=n)return 0;
    for(i=0;i<n;++i)if(cc_unit_module_byte(module,ZPU_HEADER_BYTES+i)!=(uint8_t)name[i])return 0;
    return 1;
}
uint16_t cc_units_find(const char *name) ROUTINES {
    uint16_t at,end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
        if(cc_unit_word(at)==UR_MODULE&&cc_unit_word(at+8)&&module_name(at,name))return at;
    return 0;
}
void cc_units_imports(void) ROUTINES {
    char name[128];uint16_t n;
    if(!cc_units_begin())return;
    cc_next();
    for(;;) {
        if(cc.token!=T_ID){cc_fail(53);return;}
        n=(uint16_t)strlen(cc.name);if(!n||n>11){cc_fail(UNIT_UNSUPPORTED);return;}
        strcpy(name,cc.name);
        if(!cc_units_graph(name,1))return;
        cc_next();if(cc.token!=',')break;cc_next();
    }
    cc_expect(';',1);
}
static uint16_t next_export(uint16_t module,uint16_t at) {
    return at+14+cc_unit_module_byte(module,at)+4*cc_unit_module_word(module,at+6)+cc_unit_module_word(module,at+12);
}
uint16_t cc_unit_export(uint16_t module,const char *name) ROUTINES {
    uint16_t at=cc_unit_word(module+10),count=cc_unit_module_word(module,20),i,j,n;
    for(i=0;i<count;++i,at=next_export(module,at)) {
        n=cc_unit_module_byte(module,at);if(strlen(name)!=n)continue;
        for(j=0;j<n;++j)if(cc_unit_module_byte(module,at+14+j)!=(uint8_t)name[j])break;
        if(j==n)return at;
    }
    return 0;
}
static int16_t materialize(uint16_t module,uint16_t export) {
    uint16_t at,end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES) {
        if(!(at&127u)&&!cc_poll())return -1;
        if(cc_unit_word(at)==UR_IMPORT&&cc_unit_word(at+4)==module&&cc_unit_word(at+6)==export)
            return (int16_t)cc_unit_word(at+10);
    }
    return cc_unit_import_symbol(module,export);
}
int16_t cc_units_lookup(void) ROUTINES {
    uint16_t module=0,export=0,at,end,match=0,candidate=0;SourceCursor cursor;
    if(cc.sources->unit_page==255||cc.token!=T_ID)return -1;
    if(!cc.sources->unit_qualifier&&(module=cc_units_find(cc.name))!=0&&(cc_unit_word(cc_units_state(module)+4)&UM_VISIBLE)) {
        cc_source_save(&cursor);cc_next();
        if(cc.token=='.') {
            cc_next();if(cc.token!=T_ID){cc_fail(53);return -1;}
            cc.sources->unit_qualifier=cc_unit_word(module+8);
        } else {cc_source_restore(&cursor);module=0;}
    }
    end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_MODULE&&cc_unit_word(at+8)) {
        if(!(cc_unit_word(cc_units_state(at)+4)&UM_VISIBLE))continue;
        if(cc.sources->unit_qualifier&&cc.sources->unit_qualifier!=cc_unit_word(at+8))continue;
        export=cc_unit_export(at,cc.name);
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
    /* Only known leaf-unit calls qualify for the shared-page stack model. */
    if(cc.sources->unit_mode)cc_require_full_memory();
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_IMPORT&&cc_unit_word(at+2)==scope) {
        if(cc_unit_module_byte(cc_unit_word(at+4),cc_unit_word(at+6)+2)&ZPU_CALLER_IO) {
            if(opcode!=0xcd){cc_fail(UNIT_UNSUPPORTED);return;}
            cc_emit(0x3e);cc_emit(cc.io_check);
        }
        site=cc_link(opcode,cc_unit_word(at+8));cc_unit_put(at+8,site);return;
    }
    cc_fail(UNIT_INTERFACE);
}


void cc_unit_descriptor(uint16_t module,CompilerFragmentObject *object) ROUTINES {
    uint16_t seal=module+UNIT_RECORD_BYTES;
    memset(object,0,sizeof(*object));
    object->page=(uint8_t)cc_unit_word(module+2);object->offset=cc_unit_word(seal+10);
    object->bytes=cc_unit_word(module+4)-object->offset;
    object->lifetime=cc_unit_word(seal+2)|((uint32_t)cc_unit_word(seal+4)<<16);
    object->version=cc_unit_word(seal+6)|((uint32_t)cc_unit_word(seal+8)<<16);
    object->binding=cc_unit_word(module+8);
    object->bindings=cc_unit_prefix(module)?cc_unit_prefix(module):cc_unit_word(module+4);
    object->stored_bytes=cc_unit_prefix(module)?cc_unit_prefix(module)-object->offset:0;
    object->binding_count=(uint8_t)(cc_unit_module_word(module,36)+1);
    if(!cc_fragment_source_capture(object))cc_fail(UNIT_FORMAT);
}
/* Release the I/O code page only after returning to the retained unit bank.
 * Failed release keeps its owned handle for the normal phase/terminal retry. */
static uint8_t io_finish(uint8_t ok) {
    uint8_t primary=platform_error;
    if(!code_phase_unit_io_release()){
        cc_fail(COMPILE_PLATFORM_FAILURE);if(!ok)platform_error=primary;return 0;
    }
    if(!ok)platform_error=primary;
    return ok;
}
uint8_t cc_units_write(void) ROUTINES {
    uint8_t ok=0,primary,clean;
    if(!code_phase_unit_finish_begin()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    if(!code_phase_unit_io())cc_fail(COMPILE_PLATFORM_FAILURE);
    else {
        if(!code_phase_unit_meta())cc_fail(COMPILE_PLATFORM_FAILURE);
        else {
            ok=cc_units_write_object();primary=platform_error;
            if(!code_phase_unit_meta_release()){
                cc_fail(COMPILE_PLATFORM_FAILURE);if(ok)primary=platform_error;ok=0;
            }
            platform_error=primary;
        }
        ok=io_finish(ok);
    }
    primary=platform_error;
    if(!ok) {
        /* Do not compete with abandoned tables/artifacts while restoring the
         * parser. Each owner retains failed releases for the outer retry. */
        clean=cc_tables_end();if(!cc_units_close())clean=0;
        if(!clean)cc_fail(COMPILE_PLATFORM_FAILURE);
        platform_error=primary;
    }
    if(!code_phase_unit_finish_end()){
        cc_fail(COMPILE_PLATFORM_FAILURE);if(!ok)platform_error=primary;return 0;
    }
    if(!ok)platform_error=primary;return ok;
}
/* The artifact owns code, signatures and optional source identity. Register
 * each unit separately so identical authoring paths cannot merge source maps. */
static uint16_t bind_source(uint16_t module) {
    Source source;
    char canonical[128];
    uint16_t length,offset,i;
    int16_t source_id;
    uint32_t checksum;

    memset(&source,0,sizeof(source));
    source.page=255;
    source.length=cc_unit_module_word(module,16);
    length=cc_unit_module_byte(module,19);
    offset=ZPU_HEADER_BYTES+cc_unit_module_byte(module,18);
    for(i=0;i<length;++i)source.path[i]=(char)cc_unit_module_byte(module,offset+i);
    source.path[length]=0;
    if(!sources_canonical(source.path,canonical)||strcmp(source.path,canonical)) {
        cc_fail(UNIT_FORMAT);
        return 0;
    }
    checksum=cc_unit_module_word(module,12)|((uint32_t)cc_unit_module_word(module,14)<<16);
    source_id=sources_unit(&cc.image->sources,&source,checksum);
    if(source_id<0) {
        cc_fail(99);
        return 0;
    }
    cc_unit_put(module+6,(uint16_t)source_id);
    cc_unit_put(UH_MODULES,cc_unit_word(UH_MODULES)+1);
    return module;
}
uint16_t cc_units_load(const char *name,const char *path) ROUTINES {
    uint16_t module=0,i,n=(uint16_t)strlen(name);uint8_t primary;
    char fallback[128],filename[16];
    for(i=0;i<n;++i)filename[i]=name[i]>='a'&&name[i]<='z'?name[i]-32:name[i];
    strcpy(filename+n,".ZPU");fallback[0]=0;
    if(code_library_directory[0]&&!sources_path(filename,code_library_directory,fallback)) {
        fallback[0]=0; /* A valid local unit must not depend on fallback length. */
    }
    /* Uses clauses run outside the file generator. Its idle slot lets a
     * multi-page artifact coexist with the transient loader. Release the
     * loader before restoring that slot, including on a failed import. */
    if(!code_phase_unit_link_begin()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    if(!code_phase_unit_io())cc_fail(COMPILE_PLATFORM_FAILURE);
    else {
        module=cc_units_load_object(name,path,fallback[0]?fallback:0);
        if(!io_finish(module!=0))module=0;
    }
    primary=platform_error;
    if(!code_phase_unit_link_end()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    if(!module){platform_error=primary;return 0;}
    return bind_source(module);
}

uint16_t cc_units_state(uint16_t module) ROUTINES {
    uint16_t at,end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
        if(cc_unit_word(at)==UR_STATE&&cc_unit_word(at+2)==module)return at;
    return cc_unit_record(UR_STATE,module,0,0,0,0);
}
uint16_t cc_units_graph(const char *name,uint8_t visible) ROUTINES {
    uint16_t module=cc_units_graph_load(name,visible);
    return module&&cc_units_graph_validate()?module:0;
}
/* The retained coordinator owns the transient final linker's lifetime. */
uint8_t cc_units_link(void) ROUTINES {
    uint8_t ok=0,primary;
    if(cc.sources->unit_page==255)return 1;
    if(!code_phase_unit_finish_begin()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    if(!code_phase_unit_meta())cc_fail(COMPILE_PLATFORM_FAILURE);
    else {
        ok=cc_units_link_object();primary=platform_error;
        if(!code_phase_unit_meta_release()){cc_fail(COMPILE_PLATFORM_FAILURE);ok=0;}
        else if(!ok)platform_error=primary;
    }
    primary=platform_error;
    if(!code_phase_unit_finish_end()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    if(!ok)platform_error=primary;return ok;
}

/* Uses clauses run outside the file generator. Lend its idle slot while the
 * metadata bank validates/materializes type graphs, then restore its code. */
uint8_t cc_units_types_ready(uint16_t module) ROUTINES {
    uint16_t state=cc_units_state(module);uint8_t ok=0,primary;
    if(cc_unit_word(state+4)&UM_TYPES)return 1;
    if(!code_phase_unit_link_begin()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    if(!code_phase_unit_meta())cc_fail(COMPILE_PLATFORM_FAILURE);
    else {
        ok=cc_unit_types_ready(module);primary=platform_error;
        if(!code_phase_unit_meta_release()){cc_fail(COMPILE_PLATFORM_FAILURE);ok=0;}
        else if(!ok)platform_error=primary;
    }
    primary=platform_error;
    if(!code_phase_unit_link_end()){cc_fail(COMPILE_PLATFORM_FAILURE);return 0;}
    if(!ok)platform_error=primary;
    if(ok)cc_unit_put(state+4,cc_unit_word(state+4)|UM_TYPES);return ok;
}
