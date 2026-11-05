#include "compiler_units.h"
#include "output_transport.h"
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zos_errors.h>

/* Container integers are explicit little-endian fields. The version binds
 * the primitive signature/call ABI and exact runtime profile. A checksum
 * detects accidental corruption; compiled native code is not sandboxed. */
static uint8_t lower(uint8_t c) {return c>='A'&&c<='Z'?c+32:c;}
static uint32_t object_hash(uint8_t page,uint16_t length) {
    uint32_t h=2166136261UL;uint16_t i;
    for(i=0;i<length;++i) {
        if(!(i&63u)&&!cc_poll())return 0;
        if(i>=28&&i<32)continue;
        h=(h^far_get(page,i))*16777619UL;
    }
    return h;
}
static uint8_t tag(TypeId type) {
    return type==K_INTEGER?0:type==K_BYTE?1:type==K_CHAR?2:type==K_BOOLEAN?3:type==K_REAL?4:255;
}
static uint16_t code_word(uint16_t at) {
    uint16_t v=cc_code_get(at);return v|((uint16_t)cc_code_get(at+1)<<8);
}
static uint16_t add_reference(uint16_t site,uint16_t value,uint16_t hint) {
    uint16_t previous=0,next,record;
    if(hint&&cc_unit_word(hint+2)<site)previous=hint;
    next=previous?cc_unit_word(previous+6):cc_unit_word(UH_REFERENCES);
    while(next&&cc_unit_word(next+2)<site) {
        if(!cc_poll())return 0;previous=next;next=cc_unit_word(next+6);
    }
    if(next&&cc_unit_word(next+2)==site){cc_fail(UNIT_FORMAT);return 0;}
    record=cc_unit_record(UR_CODE,site,value,next,0,0);if(!record)return 0;
    if(previous)cc_unit_put(previous+6,record);else cc_unit_put(UH_REFERENCES,record);
    cc_unit_put(UH_REFERENCE_COUNT,cc_unit_word(UH_REFERENCE_COUNT)+1);return record;
}
static uint8_t supported(void) {
    uint8_t i;
    if(cc.sources->file||cc.image->sources.count!=1||cc.console_dynamic||cc.console_used||cc.standard_table_operand||cc.file_stream_operand||
       cc.keyboard_poll||cc.keyboard_status||cc.keyboard_state||cc.keyboard_probe||cc.keyboard_enqueue||
       cc.keyboard_console_probe||cc.crt_state||cc.crt_row||cc.random_state||cc.hook_character||cc.hook_echo||cc.hook_open||cc.random_entry||cc.parameter_entry||
       cc.file_entry||cc.keyboard_entry||cc.interrupt_entry||cc.hook_output_calls||cc.hook_mask_call||
       cc.error_entry||cc.transfer_entry||cc.unpack_entry||cc.console_control!=1)goto bad;
    for(i=0;i<15;++i)if(cc.crt_entries[i])goto bad;
    for(i=0;i<HOOK_COUNT;++i)if(cc.hook_variables[i])goto bad;
    return 1;
bad:
    cc_fail(UNIT_UNSUPPORTED);return 0;
}
static void out_byte(uint8_t page,uint16_t *at,uint8_t value) {
    if(*at>=PAGE_SIZE){cc_fail(99);return;}far_set(page,(*at)++,value);
}
static void out_word(uint8_t page,uint16_t *at,uint16_t value) {
    out_byte(page,at,(uint8_t)value);out_byte(page,at,(uint8_t)(value>>8));
}
static void at_word(uint8_t page,uint16_t at,uint16_t value) {
    out_word(page,&at,value);
}
static void at_long(uint8_t page,uint16_t at,uint32_t value) {
    uint8_t i;for(i=0;i<4;++i)out_byte(page,&at,(uint8_t)(value>>(8*i)));
}
static void symbol_name(uint8_t page,uint16_t *at,uint16_t symbol) {
    uint8_t i,n=CCS(symbol,name_length);SourcePosition start=cc_reference_position(CCS(symbol,name_start));
    for(i=0;i<n;++i)out_byte(page,at,lower(cc_source_byte(start+i)));
}
uint8_t cc_units_write_object(void) ROUTINES {
    uint16_t at,end,fragments=0,exports=0,pcs=0,owner_id,off,target,delta,r,i,j,first,count;
    uint16_t artifact,page_position,blob,reference,begin,finish,site,all_refs;
    uint8_t page,kind,buffer[64];uint32_t total,export_bytes=0,hash,source_hash;
    if(!supported())return 0;
    if(cc.emitted>cc.sources->unit_start)
        cc_unit_record(UR_FRAGMENT,cc.sources->unit_start,cc.emitted,cc.sources->unit_owner,cc.sources->unit_real,0);
    end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES) {
        kind=(uint8_t)cc_unit_word(at);
        if(kind==UR_FRAGMENT)++fragments;
        if(kind==UR_PC)++pcs;
        if(kind==UR_EXPORT) {
            ++exports;i=cc_unit_word(at+2);j=cc_unit_word(at+4);
            export_bytes+=10UL+CCS(i,name_length)+2UL*cc_rget(j,R_COUNT);
        }
    }
    reference=0;
    for(i=0;i<cc.relocations;++i) {
        if(!cc_poll())return 0;r=i*RELOCATION_BYTES;
        site=cc_reloc_get(r)|((uint16_t)cc_reloc_get(r+1)<<8);
        delta=cc_reloc_get(r+2)|((uint16_t)cc_reloc_get(r+3)<<8);
        if((site&0x8000u)||delta!=1){cc_fail(UNIT_UNSUPPORTED);return 0;}
        site+=CC_CODE_BASE;reference=add_reference(site,code_word(site),reference);
        if(cc.error->code)return 0;
    }
    all_refs=cc_unit_word(UH_REFERENCE_COUNT);
    count=fragments+(cc.image->globals_bytes?1:0);
    total=ZPU_HEADER_BYTES;total+=cc_unit_word(UH_NAME_LENGTH);total+=strlen(cc.sources->root->path);
    total+=count; /* One bounded runtime flag for every fragment, including data. */
    total+=export_bytes;total+=6UL*pcs;total+=(uint32_t)MAP_RECORD_BYTES*cc.entries;
    blob=(uint16_t)total;
    total+=CC_FRAGMENT_HEADER;total+=(uint32_t)CC_FRAGMENT_RECORD*count;
    total+=(uint32_t)CC_FRAGMENT_RELOCATION*all_refs;total+=cc.emitted-CC_CODE_BASE;total+=cc.image->globals_bytes;
    if(total>PAGE_SIZE||blob>ZPU_HEADER_BYTES+PAGE_SIZE){cc_fail(99);return 0;}
    artifact=cc_unit_owned_page();if(!artifact)return 0;page=(uint8_t)cc_unit_word(artifact+2);
    for(at=0;at<total;++at)far_set(page,at,0);
    for(i=0;i<4;++i)far_set(page,i,(uint8_t)"ZPU1"[i]);
    at_word(page,4,ZPU_ABI);at_word(page,6,(uint16_t)total);
    for(i=0;i<4;++i)far_set(page,8+i,cc_runtime_profile_get(i));
    source_hash=unit_source_fingerprint(cc.sources->root);if(cc.error->code)return 0;
    at_long(page,12,source_hash);at_word(page,16,cc.sources->root->length);
    far_set(page,18,(uint8_t)cc_unit_word(UH_NAME_LENGTH));far_set(page,19,(uint8_t)strlen(cc.sources->root->path));
    at_word(page,32,cc.emitted-CC_CODE_BASE);at_word(page,34,cc.image->globals_bytes);
    at_word(page,20,exports);at_word(page,22,pcs);at_word(page,24,cc.entries);at_word(page,26,blob);
    page_position=ZPU_HEADER_BYTES;
    for(i=0;i<cc_unit_word(UH_NAME_LENGTH);++i)out_byte(page,&page_position,cc_unit_byte(UH_NAME+i));
    for(i=0;cc.sources->root->path[i];++i)out_byte(page,&page_position,(uint8_t)cc.sources->root->path[i]);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_EXPORT) {
        i=cc_unit_word(at+2);j=cc_unit_word(at+4);first=cc_rget(j,R_PARAMETERS);r=cc_rget(j,R_COUNT);
        if(!cc_unit_fragment(cc_rget(j,R_ENTRY),&owner_id,&off))return 0;
        out_byte(page,&page_position,CCS(i,name_length));out_byte(page,&page_position,CCS(i,role)==S_FUNCTION?1:0);
        out_byte(page,&page_position,CCS(i,role)==S_FUNCTION?tag(CCS(i,type)):255);out_byte(page,&page_position,0);
        out_word(page,&page_position,r);out_word(page,&page_position,owner_id);out_word(page,&page_position,off);
        symbol_name(page,&page_position,i);
        for(i=0;i<r;++i) {
            out_byte(page,&page_position,CCS(first+i,role)==S_REFERENCE?1:0);
            out_byte(page,&page_position,tag(CCS(first+i,type)));
        }
    }
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_PC) {
        if(!cc_unit_fragment(cc_unit_word(at+2),&owner_id,&off)||
           !cc_unit_fragment(cc_unit_word(at+4),&target,&delta)||target!=owner_id){cc_fail(UNIT_FORMAT);return 0;}
        out_word(page,&page_position,owner_id);out_word(page,&page_position,off);out_word(page,&page_position,delta);
    }
    for(i=0;i<cc.entries*MAP_RECORD_BYTES;++i)out_byte(page,&page_position,buffer_get(cc.image->map_page,i));
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_FRAGMENT)
        out_byte(page,&page_position,(uint8_t)cc_unit_word(at+8));
    if(cc.image->globals_bytes)out_byte(page,&page_position,0);
    if(page_position!=blob){cc_fail(UNIT_FORMAT);return 0;}
    for(i=0;i<4;++i)out_byte(page,&page_position,(uint8_t)"ZPF1"[i]);
    out_word(page,&page_position,1);out_word(page,&page_position,count);out_word(page,&page_position,all_refs);
    out_word(page,&page_position,cc.emitted-CC_CODE_BASE+cc.image->globals_bytes);
    out_word(page,&page_position,0);out_word(page,&page_position,0);
    reference=cc_unit_word(UH_REFERENCES);r=0;
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_FRAGMENT) {
        begin=cc_unit_word(at+2);finish=cc_unit_word(at+4);i=r;
        while(reference&&cc_unit_word(reference+2)<finish){++r;reference=cc_unit_word(reference+6);}
        out_word(page,&page_position,begin-CC_CODE_BASE);out_word(page,&page_position,finish-begin);
        out_word(page,&page_position,i);out_word(page,&page_position,r-i);
    }
    if(cc.image->globals_bytes) {
        out_word(page,&page_position,cc.emitted-CC_CODE_BASE);out_word(page,&page_position,cc.image->globals_bytes);
        out_word(page,&page_position,r);out_word(page,&page_position,0);
    }
    if(r!=all_refs){cc_fail(UNIT_FORMAT);return 0;}
    reference=cc_unit_word(UH_REFERENCES);
    while(reference) {
        if(!cc_poll())return 0;site=cc_unit_word(reference+2);
        if(!cc_unit_fragment(site,&owner_id,&off))return 0;
        if(cc_unit_word(reference)==UR_GLOBAL) {
            owner_id=fragments;delta=cc_unit_word(reference+4)-GLOBAL_BASE;
            if(delta>=cc.image->globals_bytes){cc_fail(UNIT_FORMAT);return 0;}
        } else if(!cc_unit_fragment(cc_unit_word(reference+4),&owner_id,&delta))return 0;
        out_word(page,&page_position,off);out_word(page,&page_position,1);
        out_word(page,&page_position,owner_id);out_word(page,&page_position,delta);
        reference=cc_unit_word(reference+6);
    }
    for(i=CC_CODE_BASE;i<cc.emitted;++i)out_byte(page,&page_position,cc_code_get(i));
    for(i=0;i<cc.image->globals_bytes;++i)out_byte(page,&page_position,0);
    if(cc.error->code||page_position!=total){cc_fail(UNIT_FORMAT);return 0;}
    hash=object_hash(page,(uint16_t)total);at_long(page,28,hash);if(cc.error->code)return 0;
    for(at=0;at<total;at+=count) {
        if(!cc_poll())return 0;count=(uint16_t)total-at;if(count>sizeof(buffer))count=sizeof(buffer);
        if(!far_read(page,at,buffer,count)||!output_write_buffer(cc_units_output(),buffer,count)) {
            cc_fail(COMPILE_PLATFORM_FAILURE);return 0;
        }
    }
    return 1;
}

static uint32_t module_long(uint16_t module,uint16_t at) {
    return cc_unit_module_word(module,at)|((uint32_t)cc_unit_module_word(module,at+2)<<16);
}
static uint8_t identifier(uint16_t module,uint16_t at,uint16_t n) {
    uint16_t i;uint8_t c;
    if(!n||n>127)return 0;
    for(i=0;i<n;++i) {
        c=cc_unit_module_byte(module,at+i);
        if(!((c>='a'&&c<='z')||c=='_'||(i&&c>='0'&&c<='9')))return 0;
    }
    return 1;
}
static uint8_t equal_name(uint16_t module,uint16_t at,uint16_t n,const char *name) {
    uint16_t i;if(strlen(name)!=n)return 0;
    for(i=0;i<n;++i)if(cc_unit_module_byte(module,at+i)!=(uint8_t)name[i])return 0;
    return 1;
}
static uint8_t metadata_valid(uint16_t module,const char *name) {
    uint16_t n,path,at,end,blob,exports,pcs,maps,i,j,k,p,params,fragment,offset,size,old,next;
    uint16_t last_fragment=0,last_site=0,refbase,first,count,site,code_bytes;
    uint32_t expected,actual,wide;
    uint8_t c,mode,type,abi;CompilerFragmentObject object;
    end=cc_unit_word(module+4);
    if(end<ZPU_HEADER_BYTES)return 0;
    for(i=0;i<4;++i)if(cc_unit_module_byte(module,i)!=(uint8_t)"ZPU1"[i])return 0;
    abi=(uint8_t)cc_unit_module_word(module,4);
    if((cc_unit_module_word(module,4)!=ZPU_ABI&&cc_unit_module_word(module,4)!=ZPU_LEGACY_ABI)||
       cc_unit_module_word(module,6)!=end)return 0;
    for(i=0;i<4;++i)if(cc_unit_module_byte(module,8+i)!=cc_runtime_profile_get(i)){cc_fail(UNIT_STALE);return 0;}
    expected=module_long(module,28);actual=object_hash((uint8_t)cc_unit_word(module+2),end);
    if(expected!=actual)return 0;
    n=cc_unit_module_byte(module,18);path=cc_unit_module_byte(module,19);
    if(!n||n>11||!path||path>127||!identifier(module,ZPU_HEADER_BYTES,n)||!equal_name(module,ZPU_HEADER_BYTES,n,name))return 0;
    for(i=0;i<path;++i)if(!cc_unit_module_byte(module,ZPU_HEADER_BYTES+n+i))return 0;
    blob=cc_unit_module_word(module,26);cc_unit_put(module+UNIT_RECORD_BYTES+10,blob);
    if((uint32_t)blob+CC_FRAGMENT_HEADER>end)return 0;
    cc_unit_descriptor(module,&object);
    if(cc_fragment_validate(&object,1)!=CF_OK)return 0;
    code_bytes=cc_unit_module_word(module,32);
    count=cc_unit_module_word(module,blob+6);refbase=blob+CC_FRAGMENT_HEADER+count*CC_FRAGMENT_RECORD;
    /* Local identities are rebound by the caller, never by mutating the
     * checksum-bound source page. Initial units cannot import other units. */
    if(cc_unit_module_word(module,blob+4)!=1)return 0;
    for(i=0;i<cc_unit_module_word(module,blob+8);++i)
        if(cc_unit_module_word(module,refbase+i*8+2)!=1)return 0;
    if(!code_bytes||cc_unit_module_word(module,blob+10)!=(uint32_t)code_bytes+cc_unit_module_word(module,34))return 0;
    k=count;
    if(cc_unit_module_word(module,34)) {
        if(count<2)return 0;--k;at=blob+16+k*8;
        if(cc_unit_module_word(module,at)!=code_bytes||
           cc_unit_module_word(module,at+2)!=cc_unit_module_word(module,34)||cc_unit_module_word(module,at+6))return 0;
    }
    for(i=0;i<k;++i) {
        at=blob+16+i*8;wide=(uint32_t)cc_unit_module_word(module,at)+cc_unit_module_word(module,at+2);
        if(wide>code_bytes)return 0;
    }
    at=ZPU_HEADER_BYTES+n+path;cc_unit_put(module+10,at);exports=cc_unit_module_word(module,20);
    for(i=0;i<exports;++i) {
        if(!cc_poll())return 0;
        if((uint32_t)at+10>blob)return 0;
        n=cc_unit_module_byte(module,at);c=cc_unit_module_byte(module,at+1);type=cc_unit_module_byte(module,at+2);
        params=cc_unit_module_word(module,at+4);fragment=cc_unit_module_word(module,at+6);offset=cc_unit_module_word(module,at+8);
        wide=(uint32_t)at+10+n+2UL*params;
        if(wide>blob||!identifier(module,at+10,n)||c>1||cc_unit_module_byte(module,at+3)||
           (c?type>(abi==ZPU_LEGACY_ABI?3:4):type!=255)||fragment>=count)return 0;
        size=cc_unit_module_word(module,blob+16+fragment*8+2);if(offset>=size||cc_unit_module_word(module,blob+16+fragment*8)>=code_bytes)return 0;
        old=cc_unit_word(module+10);
        for(j=0;j<i;++j) {
            next=old+10+cc_unit_module_byte(module,old)+2*cc_unit_module_word(module,old+4);
            if(cc_unit_module_byte(module,old)==n) {
                for(k=0;k<n;++k)if(cc_unit_module_byte(module,old+10+k)!=cc_unit_module_byte(module,at+10+k))break;
                if(k==n)return 0;
            }
            old=next;
        }
        for(j=0;j<params;++j) {
            mode=cc_unit_module_byte(module,at+10+n+2*j);type=cc_unit_module_byte(module,at+11+n+2*j);
            if(mode>1||(type>(abi==ZPU_LEGACY_ABI?3:4)&&!(mode&&type==255)))return 0;
        }
        at=(uint16_t)wide;
    }
    pcs=cc_unit_module_word(module,22);maps=cc_unit_module_word(module,24);
    wide=at;wide+=6UL*pcs;wide+=8UL*maps;if(abi==ZPU_ABI)wide+=count;if(wide!=blob)return 0;
    for(i=0;i<pcs;++i,at+=6) {
        if(!cc_poll())return 0;
        fragment=cc_unit_module_word(module,at);site=cc_unit_module_word(module,at+2);offset=cc_unit_module_word(module,at+4);
        if(fragment>=count||cc_unit_module_word(module,blob+16+fragment*8)>=code_bytes)return 0;size=cc_unit_module_word(module,blob+16+fragment*8+2);
        if((uint32_t)site+2>size||offset>=size||(i&&(fragment<last_fragment||(fragment==last_fragment&&site<last_site))))return 0;
        last_fragment=fragment;last_site=site+2;
        first=cc_unit_module_word(module,blob+16+fragment*8+4);n=cc_unit_module_word(module,blob+16+fragment*8+6);
        for(j=0;j<n;++j) {
            p=cc_unit_module_word(module,refbase+8*(first+j));
            if(site<p+2&&p<site+2)return 0;
        }
    }
    for(i=0;i<maps;++i,at+=8) {
        first=cc_unit_module_word(module,at);n=cc_unit_module_word(module,at+2);
        if(first>=n||n>code_bytes||cc_unit_module_word(module,at+4)||
           cc_unit_module_word(module,at+6)>cc_unit_module_word(module,16))return 0;
    }
    if(abi==ZPU_ABI)for(i=0;i<count;++i) {
        c=cc_unit_module_byte(module,at+i);
        if(c>ZPU_REAL||(i+1==count&&cc_unit_module_word(module,34)&&c))return 0;
    }
    return !cc.error->code;
}
uint16_t cc_units_load_object(const char *name,const char *path) ROUTINES {
    uint16_t module=0,at=0,n,i,seal;uint8_t buffer[64],page,ok=1;
    int8_t fd;uint32_t version;
    fd=output_open_path(path,O_RDONLY);if(fd<0){cc_fail(UNIT_MISSING);return 0;}
    module=cc_unit_owned_page();if(!module){output_finish(fd,0);return 0;}page=(uint8_t)cc_unit_word(module+2);
    for(;;) {
        if(!cc_poll()){ok=0;break;}n=sizeof(buffer);platform_error=read(fd,buffer,&n);
        if(platform_error||n>sizeof(buffer)||(uint32_t)at+n>PAGE_SIZE){ok=0;break;}
        if(!n)break;for(i=0;i<n;++i)far_set(page,at++,buffer[i]);
    }
    if(!output_finish(fd,ok)){cc_fail(UNIT_FORMAT);return 0;}
    cc_unit_put(module+4,at);seal=module+UNIT_RECORD_BYTES;version=page_versions[page];
    cc_unit_put(seal+6,(uint16_t)version);cc_unit_put(seal+8,(uint16_t)(version>>16));
    cc_unit_put(module+8,cc_unit_word(UH_MODULES)+1);
    if(!metadata_valid(module,name)){cc_fail(UNIT_FORMAT);return 0;}
    return module;
}
