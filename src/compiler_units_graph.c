#include "compiler_units.h"

/* One invocation owns one canonical module per Pascal name. Dependency IDs
 * belong to each file; a sealed table after its immutable bytes binds those
 * IDs to this invocation's catalog. No executable body is copied here. */
static uint8_t own_name(const char *name) {
    uint16_t i,n=cc_unit_word(UH_NAME_LENGTH);
    if(!cc.sources->unit_mode||strlen(name)!=n)return 0;
    for(i=0;i<n;++i)if(cc_unit_byte(UH_NAME+i)!=(uint8_t)name[i])return 0;
    return 1;
}
uint16_t cc_units_graph_load(const char *name,uint8_t visible) ROUTINES {
    uint16_t module,state,at,i,j,n,dependency,count,wire,flags,seal;
    uint32_t expected,actual,version;char path[128],directory[128],filename[16],child[12];
    uint8_t page;
    if(!cc_stack_check())return 0;
    if(own_name(name)){cc_fail(UNIT_CYCLE);return 0;}
    module=cc_units_find(name);
    if(module) {
        state=cc_units_state(module);flags=cc_unit_word(state+4);
        if(flags&UM_LOADING){cc_fail(UNIT_CYCLE);return 0;}
        if(!(flags&UM_READY)){cc_fail(UNIT_FORMAT);return 0;}
        if(visible)cc_unit_put(state+4,flags|UM_VISIBLE);
        return module;
    }
    if(cc_unit_word(UH_MODULES)==CC_FRAGMENT_OBJECTS){cc_fail(99);return 0;}
    n=(uint16_t)strlen(name);if(!n||n>11){cc_fail(UNIT_FORMAT);return 0;}
    for(i=0;i<n;++i)filename[i]=name[i]>='a'&&name[i]<='z'?name[i]-32:name[i];
    strcpy(filename+n,".ZPU");strcpy(directory,cc.sources->root->path);
    i=(uint16_t)strlen(directory);while(i&&directory[i-1]!='/')--i;directory[i]=0;
    /* An unrepresentable source candidate is absent; the normal loader still
     * tries the directory of the loaded compiler container. */
    if(!sources_path(filename,directory,path))path[0]=0;
    module=cc_units_load(name,path);if(!module)return 0;
    state=cc_units_state(module);cc_unit_put(state+4,UM_LOADING|(visible?UM_VISIBLE:0));
    count=cc_unit_module_word(module,36);at=cc_unit_module_word(module,40);
    page=(uint8_t)cc_unit_word(module+2);n=cc_unit_word(module+4);
    far_set(page,n,(uint8_t)cc_unit_word(module+8));far_set(page,n+1,0);
    seal=module+UNIT_RECORD_BYTES;version=page_versions[page];
    cc_unit_put(seal+6,(uint16_t)version);cc_unit_put(seal+8,(uint16_t)(version>>16));
    for(i=0;i<count&&!cc.error->code;++i) {
        wire=cc_unit_module_word(module,at+2);
        for(j=0;j<cc_unit_module_byte(module,at);++j)child[j]=(char)cc_unit_module_byte(module,at+8+j);
        child[j]=0;
        expected=cc_unit_module_word(module,at+4)|((uint32_t)cc_unit_module_word(module,at+6)<<16);
        /* Recursive loads must not observe this page while it is being bound.
         * Byte reads retain the updated seal, never a borrowed mapped pointer. */
        seal=module+UNIT_RECORD_BYTES;version=page_versions[page];
        cc_unit_put(seal+6,(uint16_t)version);cc_unit_put(seal+8,(uint16_t)(version>>16));
        dependency=cc_units_graph_load(child,0);if(!dependency)return 0;
        actual=cc_unit_module_word(dependency,28)|((uint32_t)cc_unit_module_word(dependency,30)<<16);
        if(actual!=expected){cc_fail(UNIT_STALE);return 0;}
        far_set(page,n+2u*(wire-1u),(uint8_t)cc_unit_word(dependency+8));
        far_set(page,n+2u*(wire-1u)+1,0);
        version=page_versions[page];cc_unit_put(seal+6,(uint16_t)version);cc_unit_put(seal+8,(uint16_t)(version>>16));
        at+=8+cc_unit_module_byte(module,at);
    }
    seal=module+UNIT_RECORD_BYTES;version=page_versions[page];
    cc_unit_put(seal+6,(uint16_t)version);cc_unit_put(seal+8,(uint16_t)(version>>16));
    if(cc.error->code)return 0;
    cc_unit_put(state+4,UM_READY|(visible?UM_VISIBLE:0));return module;
}

uint8_t cc_units_graph_validate(void) ROUTINES {
    CompilerFragmentObject objects[CC_FRAGMENT_OBJECTS];uint16_t at,end=cc_unit_word(UH_USED);uint8_t count=0,status;
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
        if(cc_unit_word(at)==UR_MODULE&&cc_unit_word(at+8)) {
            if(count==CC_FRAGMENT_OBJECTS){cc_fail(99);return 0;}
            cc_unit_descriptor(at,objects+count++);
        }
    if(!count)return 1;
    status=cc_fragment_validate(objects,count);
    if(status!=CF_OK){cc_fail(UNIT_FORMAT);return 0;}
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
        if(cc_unit_word(at)==UR_MODULE&&cc_unit_word(at+8)&&!cc_units_types_ready(at))return 0;
    return 1;
}

uint16_t cc_unit_target(uint16_t module,uint16_t identity) ROUTINES {
    uint16_t at,end=cc_unit_word(UH_USED),n=cc_unit_word(module+4),binding;
    if(!identity||identity>cc_unit_module_word(module,36)+1){cc_fail(UNIT_FORMAT);return 0;}
    if(!cc_unit_module_ready(module))return 0;
    binding=far_get((uint8_t)cc_unit_word(module+2),n+2u*(identity-1u));
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
        if(cc_unit_word(at)==UR_MODULE&&cc_unit_word(at+8)==binding)return at;
    cc_fail(UNIT_FORMAT);return 0;
}
