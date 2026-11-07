#include "compiler_units.h"
#include "compiler_link.h"
#include "library.h"

/* This is an experimental resource limit, not an artifact-field limit. The
 * near arrays live only during final linking on the compiler's retained C
 * stack. Exported names acquire lexical symbols lazily; checked type graphs
 * can own additional compile-time metadata. */
#define UNIT_LINK_ROOTS 128u
static uint8_t location(const CompilerFragmentLayout *layout,uint16_t module,uint16_t fragment,uint16_t *address) {
    uint8_t status=cc_fragment_layout_get(layout,cc_unit_word(module+8),fragment,address);
    if(status==CF_OK)return 1;
    if(status!=CF_TARGET)cc_fail(UNIT_FORMAT);
    return 0;
}
static uint16_t module_exports_end(uint16_t module) {
    uint16_t at=cc_unit_word(module+10),i,n=cc_unit_module_word(module,20);
    for(i=0;i<n;++i)at+=14+cc_unit_module_byte(module,at)+4*cc_unit_module_word(module,at+6)+cc_unit_module_word(module,at+12);
    return at;
}
static uint8_t append_metadata(uint16_t module,const CompilerFragmentLayout *layout) {
    uint16_t blob=cc_unit_word(module+UNIT_RECORD_BYTES+10),fragments=cc_unit_module_word(module,blob+6);
    uint16_t refbase=blob+16+fragments*8,at,i,j,fragment,address,site,first,count,pcs,maps,map_start;
    uint16_t data_start,size,low,high,a,z,source,target,encoded,value;uint8_t kind;
    uint8_t file=(uint8_t)cc_unit_word(module+6);
    for(fragment=0;fragment<fragments;++fragment)if(location(layout,module,fragment,&address)) {
        first=cc_unit_module_word(module,blob+16+fragment*8+4);
        count=cc_unit_module_word(module,blob+16+fragment*8+6);
        for(i=0;i<count;++i) {
            if(!cc_poll())return 0;site=cc_unit_module_word(module,refbase+8*(first+i));
            encoded=cc_unit_module_word(module,refbase+8*(first+i)+4);
            kind=(uint8_t)(encoded>>CF_REFERENCE_SHIFT);
            if(!location(layout,cc_unit_target(module,cc_unit_module_word(module,refbase+8*(first+i)+2)),encoded&CF_FRAGMENT_MASK,&target)) {
                cc_fail(UNIT_FORMAT);return 0;
            }
            value=target+cc_unit_module_word(module,refbase+8*(first+i)+6);
            if(kind==CF_WORD16) {cc_word(address+site,value);cc_relocate(address+site);}
            else cc_link_reference(address+site,CL_CODE,0,value-CC_CODE_BASE,kind);
        }
    }
    at=module_exports_end(module);pcs=cc_unit_module_word(module,22);maps=cc_unit_module_word(module,24);
    for(i=0;i<pcs;++i,at+=6) {
        if(!cc_poll())return 0;fragment=cc_unit_module_word(module,at);
        if(!location(layout,module,fragment,&address))continue;
        site=cc_unit_module_word(module,at+2);source=cc_unit_module_word(module,at+4);
        /* Runtime PCs are code-origin offsets, not native addresses. The
         * ordinary final image delta must therefore not be added here. */
        cc_word(address+site,address-CC_CODE_BASE+source);
    }
    map_start=at;
    for(fragment=0;fragment<fragments;++fragment)if(location(layout,module,fragment,&address)) {
        data_start=cc_unit_module_word(module,blob+16+fragment*8);
        if(data_start>=cc_unit_module_word(module,32))continue;
        size=cc_unit_module_word(module,blob+16+fragment*8+2);
        low=data_start;high=data_start+size;
        for(j=0,at=map_start;j<maps;++j,at+=8) {
            if(!cc_poll())return 0;
            a=cc_unit_module_word(module,at);z=cc_unit_module_word(module,at+2);
            if(a<low)a=low;if(z>high)z=high;if(a>=z)continue;
            source=cc_unit_module_word(module,at+6);
            cc_mapping(address-CC_CODE_BASE+a-low,address-CC_CODE_BASE+z-low,
                ((SourcePosition)file<<16)|source,((SourcePosition)file<<16)|source);
        }
    }
    return !cc.error->code;
}
uint8_t cc_units_link_object(void) ROUTINES {
    CompilerFragmentObject objects[CC_FRAGMENT_OBJECTS];CompilerFragmentRoot roots[UNIT_LINK_ROOTS];
    CompilerFragmentLayout layout;CompilerFragmentImage linked;
    uint16_t modules[CC_FRAGMENT_OBJECTS],at,end,count=0,root_count=0,module,export,fragment,offset,address,i,origin;
    uint8_t status,ok=0,clean=1,primary,cleanup_error=0;
    if(cc.sources->unit_page==255)return 1;
    end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_MODULE&&cc_unit_word(at+8)) {
        if(count==CC_FRAGMENT_OBJECTS){cc_fail(99);return 0;}
        modules[count]=at;cc_unit_descriptor(at,objects+count);++count;
    }
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_IMPORT&&cc_unit_word(at+8)) {
        if(root_count==UNIT_LINK_ROOTS){cc_fail(99);return 0;}
        module=cc_unit_word(at+4);export=cc_unit_word(at+6);
        roots[root_count].identity=cc_unit_word(module+8);
        roots[root_count].fragment=cc_unit_module_word(module,export+8);++root_count;
    }
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_INIT_CALL) {
        if(root_count==UNIT_LINK_ROOTS){cc_fail(99);return 0;}
        module=cc_unit_word(at+2);
        roots[root_count].identity=cc_unit_word(module+8);
        roots[root_count].fragment=cc_unit_module_word(module,46)-1;++root_count;
    }
    if(cc.error->code)return 0;
    if(!root_count)return 1;
    origin=cc.emitted;cc_fragment_image_init(&linked);cc_fragment_layout_init(&layout);
    status=cc_fragment_plan(objects,(uint8_t)count,roots,root_count,origin,CC_STATIC_LIMIT,&linked,&layout);
    if(status!=CF_OK) {
        if(!cc.error->code)cc_fail(status==CF_CAPACITY||status==CF_MEMORY?99:UNIT_FORMAT);goto done;
    }
    /* The placement catalog is enough: append into existing compiler staging
     * rather than allocating a duplicate output image in scarce target RAM. */
    for(i=0;i<count;++i) {
        uint16_t blob=objects[i].offset,fragments=cc_unit_module_word(modules[i],blob+6),f,n,start,next=cc.emitted;
        uint8_t mask=0,index,last;
        for(f=0;f<fragments;++f)if(location(&layout,modules[i],f,&address)) {
            if(address!=next){cc_fail(UNIT_FORMAT);goto done;}
            if(cc_unit_module_byte(modules[i],blob-fragments+f)&ZPU_REAL)cc_require_real();
            if(cc_unit_module_byte(modules[i],blob-fragments+f)&ZPU_FULL_MEMORY)cc_require_full_memory();
            if(cc_unit_module_byte(modules[i],blob-fragments+f)&ZPU_HARDWARE)cc_require_hardware();
            start=cc_unit_prefix(modules[i])+cc_unit_module_word(modules[i],blob+16+8*f);
            n=cc_unit_module_word(modules[i],blob+18+8*f);next+=n;
            if(n) {
                last=(uint8_t)((start+n-1)/PAGE_SIZE);
                for(index=(uint8_t)(start/PAGE_SIZE);index<=last;++index)mask|=1u<<index;
            }
        }
        for(index=0;index<4;++index)if(mask&(1u<<index))
            if(!cc_unit_emit_page(modules[i],index,&layout))goto done;
        cc.emitted=next;
    }
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_IMPORT&&cc_unit_word(at+8)) {
        module=cc_unit_word(at+4);export=cc_unit_word(at+6);
        fragment=cc_unit_module_word(module,export+8);offset=cc_unit_module_word(module,export+10);
        if(!location(&layout,module,fragment,&address)){cc_fail(UNIT_FORMAT);goto done;}
        cc_patch_chain(cc_unit_word(at+8),address+offset);cc_rput(cc_unit_word(at+2),R_ENTRY,address+offset);
    }
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_INIT_CALL) {
        module=cc_unit_word(at+2);fragment=cc_unit_module_word(module,46)-1;
        if(!location(&layout,module,fragment,&address)){cc_fail(UNIT_FORMAT);goto done;}
        cc_word(cc_unit_word(at+4),address);
    }
    for(i=0;i<count;++i)if(!append_metadata(modules[i],&layout))goto done;
    ok=!cc.error->code;
done:
    primary=platform_error;
    if(!cc_fragment_layout_close(&layout)) {
        clean=0;if(!cleanup_error)cleanup_error=platform_error;if(layout.page!=255)cc_units_hold(layout.page,layout.lifetime,PAGE_COMPILER);
    }
    if(!cc_fragment_image_close(&linked)) {
        clean=0;if(!cleanup_error)cleanup_error=platform_error;
        for(i=0;i<2;++i)if(linked.pages[i]!=255)cc_units_hold(linked.pages[i],linked.lifetimes[i],PAGE_IMAGE);
        if(linked.workspace!=255)cc_units_hold(linked.workspace,linked.workspace_lifetime,PAGE_COMPILER);
    }
    if(!clean){cc_fail(COMPILE_PLATFORM_FAILURE);platform_error=!ok&&primary?primary:cleanup_error;return 0;}
    if(!ok)platform_error=primary;return ok;
}
