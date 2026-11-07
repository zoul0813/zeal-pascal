#include "compiler_units.h"
#include "output_transport.h"
#include "library.h"
#include <zos_sys.h>
#include <zos_vfs.h>
#include <zos_errors.h>

/* Container integers are explicit little-endian fields. The version binds
 * the public metadata/call ABI and exact runtime profile. A checksum
 * detects accidental corruption; compiled native code is not sandboxed. */
static uint32_t object_hash(uint8_t page,uint16_t length) {
    uint32_t h=2166136261UL;uint16_t i;
    for(i=0;i<length;++i) {
        if(!(i&63u)&&!cc_poll())return 0;
        if(i>=28&&i<32)continue;
        h=(h^buffer_get(page,i))*16777619UL;
    }
    return h;
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
    if(next&&cc_unit_word(next+2)==site) {
        if(cc_unit_word(next)==UR_EXTERNAL)return next;
        cc_fail(UNIT_FORMAT);return 0;
    }
    record=cc_unit_record(UR_CODE,site,value,next,0,0);if(!record)return 0;
    if(previous)cc_unit_put(previous+6,record);else cc_unit_put(UH_REFERENCES,record);
    cc_unit_put(UH_REFERENCE_COUNT,cc_unit_word(UH_REFERENCE_COUNT)+1);return record;
}
static uint8_t supported(void) {
    uint8_t i;Source source;
    for(i=1;i<cc.image->sources.count;++i)
        if(!sources_get(&cc.image->sources,i,&source)||source.page!=255)goto bad;
    if(cc.sources->file||cc.console_dynamic||cc.console_used||cc.standard_table_operand||cc.file_stream_operand||
       cc.keyboard_poll||cc.keyboard_status||cc.keyboard_state||cc.keyboard_probe||cc.keyboard_enqueue||
       cc.keyboard_console_probe||cc.random_state||cc.hook_character||cc.hook_echo||cc.hook_open||cc.random_entry||cc.parameter_entry||
       cc.file_entry||cc.keyboard_entry||cc.interrupt_entry||cc.hook_output_calls||cc.hook_mask_call||
       cc.error_entry||cc.transfer_entry||cc.unpack_entry||cc.console_control!=1)goto bad;
    for(i=0;i<HOOK_COUNT;++i)if(cc.hook_variables[i])goto bad;
    return 1;
bad:
    cc_fail(UNIT_UNSUPPORTED);return 0;
}
static void out_byte(uint8_t page,uint16_t *at,uint8_t value) {
    if(*at>=ZPU_ARTIFACT_LIMIT||
       (!(*at%PAGE_SIZE)&&!buffer_reserve(page,*at+1))){cc_fail(99);return;}
    buffer_set(page,(*at)++,value);
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
uint8_t cc_units_write_object(void) ROUTINES {
    uint16_t at,end,fragments=0,exports=0,pcs=0,owner_id,off,target,delta,r,i,j,count;
    uint16_t artifact,page_position,prefix,blob,reference,begin,finish,site,all_refs,dependencies=0,dependency_bytes=0,head,next,module,export;
    UnitReference cursor;
    uint8_t page,kind,buffer[64];uint32_t total,export_bytes=0,type_bytes=0,hash,source_hash;
    if(!supported())return 0;
    if(cc.emitted>cc.sources->unit_start)
        cc_unit_record(UR_FRAGMENT,cc.sources->unit_start,cc.emitted,cc.sources->unit_owner,cc.sources->unit_real,0);
    end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES) {
        kind=(uint8_t)cc_unit_word(at);
        if(kind==UR_FRAGMENT)++fragments;
        if(kind==UR_PC)++pcs;
        if(kind==UR_EXPORT)++exports;
    }
    type_bytes=cc_unit_types_bytes();export_bytes=cc_unit_exports_bytes();
    /* Imported call chains become symbolic external fragment references.
     * Their native operands remain placeholders until the final program link. */
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES) {
        if(cc_unit_word(at)==UR_MODULE&&cc_unit_word(at+8)) {
            ++dependencies;dependency_bytes+=8+cc_unit_module_byte(at,18);
        }
        if(cc_unit_word(at)!=UR_IMPORT)continue;
        module=cc_unit_word(at+4);export=cc_unit_word(at+6);head=cc_unit_word(at+8);
        while(head&&!cc.error->code) {
            next=code_word(head);reference=add_reference(head,0,0);if(!reference)return 0;
            cc_unit_put(reference,UR_EXTERNAL);cc_unit_put(reference+4,module);
            cc_unit_put(reference+8,cc_unit_module_word(module,export+8));
            cc_unit_put(reference+10,cc_unit_module_word(module,export+10));head=next;
        }
    }
    if(dependencies>=CC_FRAGMENT_OBJECTS){cc_fail(99);return 0;}
    if(!cc_unit_relocations_prepare())return 0;
    all_refs=0;
    for(cc_unit_reference_begin(&cursor);cursor.kind;cc_unit_reference_next(&cursor))++all_refs;
    if(cc.error->code)return 0;
    count=fragments+(cc.image->globals_bytes?1:0);
    total=ZPU_HEADER_BYTES;total+=cc_unit_word(UH_NAME_LENGTH);total+=strlen(cc.sources->root->path);
    total+=dependency_bytes;total+=type_bytes;
    total+=count; /* One bounded runtime flag for every fragment, including data. */
    total+=export_bytes;total+=6UL*pcs;total+=(uint32_t)MAP_RECORD_BYTES*cc.entries;
    blob=(uint16_t)total;
    total+=CC_FRAGMENT_HEADER;total+=(uint32_t)CC_FRAGMENT_RECORD*count;
    total+=(uint32_t)CC_FRAGMENT_RELOCATION*all_refs;total+=cc.emitted-CC_CODE_BASE;total+=cc.image->globals_bytes;
    if(total>ZPU_ARTIFACT_LIMIT||blob>ZPU_HEADER_BYTES+ZPU_ARTIFACT_LIMIT){cc_fail(99);return 0;}
    prefix=(uint16_t)(total-(cc.emitted-CC_CODE_BASE)-cc.image->globals_bytes);
    artifact=cc_unit_artifact_page();if(!artifact)return 0;page=(uint8_t)cc_unit_word(artifact+2);
    for(at=0;at<ZPU_HEADER_BYTES;++at)buffer_set(page,at,0);
    for(i=0;i<4;++i)buffer_set(page,i,(uint8_t)"ZPU1"[i]);
    at_word(page,4,ZPU_ABI);at_word(page,6,(uint16_t)total);
    for(i=0;i<4;++i)buffer_set(page,8+i,cc_runtime_profile_get(i));
    source_hash=unit_source_fingerprint(cc.sources->root);if(cc.error->code)return 0;
    at_long(page,12,source_hash);at_word(page,16,cc.sources->root->length);
    buffer_set(page,18,(uint8_t)cc_unit_word(UH_NAME_LENGTH));buffer_set(page,19,(uint8_t)strlen(cc.sources->root->path));
    at_word(page,32,cc.emitted-CC_CODE_BASE);at_word(page,34,cc.image->globals_bytes);
    at_word(page,20,exports);at_word(page,22,pcs);at_word(page,24,cc.entries);at_word(page,26,blob);
    at_word(page,36,dependencies);at_word(page,38,cc.type_count-6);
    at_word(page,40,ZPU_HEADER_BYTES+cc_unit_word(UH_NAME_LENGTH)+(uint16_t)strlen(cc.sources->root->path));
    at_word(page,42,ZPU_HEADER_BYTES+cc_unit_word(UH_NAME_LENGTH)+(uint16_t)strlen(cc.sources->root->path)+dependency_bytes);
    at_word(page,44,ZPU_HEADER_BYTES+cc_unit_word(UH_NAME_LENGTH)+(uint16_t)strlen(cc.sources->root->path)+dependency_bytes+(uint16_t)type_bytes);
    /* Word46 is zero or a code-fragment index+1. Initializers begin at
     * fragment offset0 and are roots independently of exported routines. */
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_INITIALIZER) {
        if(buffer_get(page,46)||buffer_get(page,47)||!cc_unit_fragment(cc_unit_word(at+2),&owner_id,&off)||off){cc_fail(UNIT_FORMAT);return 0;}
        at_word(page,46,owner_id+1);
    }
    page_position=ZPU_HEADER_BYTES;
    for(i=0;i<cc_unit_word(UH_NAME_LENGTH);++i)out_byte(page,&page_position,cc_unit_byte(UH_NAME+i));
    for(i=0;cc.sources->root->path[i];++i)out_byte(page,&page_position,(uint8_t)cc.sources->root->path[i]);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_MODULE&&cc_unit_word(at+8)) {
        i=cc_unit_module_byte(at,18);
        out_byte(page,&page_position,(uint8_t)i);out_byte(page,&page_position,0);
        out_word(page,&page_position,cc_unit_word(at+8)+1);
        for(j=0;j<4;++j)out_byte(page,&page_position,cc_unit_module_byte(at,28+j));
        for(j=0;j<i;++j)out_byte(page,&page_position,cc_unit_module_byte(at,ZPU_HEADER_BYTES+j));
    }
    cc_unit_types_write(page,&page_position);cc_unit_exports_write(page,&page_position);
    /* Public metadata no longer calls the transient decoder. Its slot can
     * hold the rest of the prefix while the existing relocation ledger stays
     * alive; release the metadata arena after those fixups are serialized. */
    if(cc.error->code||!code_phase_unit_meta_release()){cc_fail(99);return 0;}
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
    cc_unit_reference_begin(&cursor);r=0;
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)if(cc_unit_word(at)==UR_FRAGMENT) {
        begin=cc_unit_word(at+2);finish=cc_unit_word(at+4);i=r;
        while(cursor.kind&&cursor.site<finish){++r;cc_unit_reference_next(&cursor);}
        out_word(page,&page_position,begin-CC_CODE_BASE);out_word(page,&page_position,finish-begin);
        out_word(page,&page_position,i);out_word(page,&page_position,r-i);
    }
    if(cc.image->globals_bytes) {
        out_word(page,&page_position,cc.emitted-CC_CODE_BASE);out_word(page,&page_position,cc.image->globals_bytes);
        out_word(page,&page_position,r);out_word(page,&page_position,0);
    }
    if(r!=all_refs){cc_fail(UNIT_FORMAT);return 0;}
    cc_unit_reference_begin(&cursor);
    while(cursor.kind) {
        if(!cc_poll())return 0;site=cursor.site;reference=cursor.record;
        if(!cc_unit_fragment(site,&owner_id,&off))return 0;
        module=1;
        if(cursor.kind==UR_EXTERNAL) {
            module=cc_unit_word(cc_unit_word(reference+4)+8)+1;
            owner_id=cc_unit_word(reference+8);delta=cc_unit_word(reference+10);
        } else if(cursor.kind==UR_GLOBAL) {
            owner_id=fragments;delta=cursor.value-GLOBAL_BASE;
            if(delta>=cc.image->globals_bytes){cc_fail(UNIT_FORMAT);return 0;}
        } else if(!cc_unit_fragment(cursor.value,&owner_id,&delta))return 0;
        out_word(page,&page_position,off);out_word(page,&page_position,module);
        out_word(page,&page_position,owner_id);out_word(page,&page_position,delta);
        cc_unit_reference_next(&cursor);
    }
    if(!cc_tables_end()){cc_fail(99);return 0;}
    if(cc.error->code||page_position!=prefix){cc_fail(UNIT_FORMAT);return 0;}
    hash=object_hash(page,prefix);
    for(i=CC_CODE_BASE;i<cc.emitted;++i) {
        if(!(i&63u)&&!cc_poll())return 0;
        hash=(hash^cc_code_get(i))*16777619UL;
    }
    for(i=0;i<cc.image->globals_bytes;++i){if(!(i&63u)&&!cc_poll())return 0;hash*=16777619UL;}
    at_long(page,28,hash);if(cc.error->code)return 0;
    for(at=0;at<total;at+=count) {
        if(!cc_poll())return 0;count=(uint16_t)total-at;if(count>sizeof(buffer))count=sizeof(buffer);
        for(i=0;i<count;++i) {
            site=at+i;
            buffer[i]=site<prefix?buffer_get(page,site):
                site-prefix<cc.emitted-CC_CODE_BASE?cc_code_get(CC_CODE_BASE+site-prefix):0;
        }
        if(!output_write_buffer(cc_units_output(),buffer,count)) {
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
/* Structural validation precedes dependency binding and type allocation. */
static uint8_t type_id(uint16_t module,uint16_t value) {
    return value<6u+cc_unit_module_word(module,38);
}
static uint8_t type_records(uint16_t module,uint16_t *cursor,uint16_t blob) {
    uint16_t type,at=*cursor,n,i,j,family,width,low,high,capacity,element,origin,wire,role,name,tag;
    uint32_t next;
    if(cc_unit_module_word(module,38)>TYPE_LIMIT-6)return 0;
    for(type=6;type<6+cc_unit_module_word(module,38);++type) {
        if(!cc_poll()||(uint32_t)at+18>blob)return 0;
        family=cc_unit_module_word(module,at);width=cc_unit_module_word(module,at+2);
        low=cc_unit_module_word(module,at+4);high=cc_unit_module_word(module,at+6);
        capacity=cc_unit_module_word(module,at+8);element=cc_unit_module_word(module,at+10);
        origin=cc_unit_module_word(module,at+12);wire=cc_unit_module_word(module,at+14);n=cc_unit_module_word(module,at+16);
        if(origin) {
            if(origin<2||origin>cc_unit_module_word(module,36)+1||wire<6||wire>=TYPE_LIMIT||n)return 0;
            for(j=0;j<12;j+=2)if(cc_unit_module_word(module,at+j))return 0;
        } else {
            if(wire||width>32767u||(family!=K_RECORD&&n))return 0;
            if(family==K_ARRAY) {
                if(low||high||!type_id(module,capacity)||!type_id(module,element))return 0;
            } else if(family==K_SET||family==K_POINTER) {
                if(low||high||capacity||!type_id(module,element))return 0;
            } else if(family==K_FILE) {
                if(width!=176||low||high||capacity>FILE_UNTYPED||
                   (capacity==FILE_TYPED?!type_id(module,element):element!=TYPE_NONE))return 0;
            } else if(family==K_RECORD) {
                if(high||capacity||element||(low!=0xffffu&&low>=n))return 0;
            } else if(family==K_WORD||family==K_LONGWORD) {
                if(capacity||element||(family==K_LONGWORD?(width!=4||low||high!=65535u):
                   ((width!=1&&width!=2)||low>high||(width==1&&high>255))))return 0;
            } else if(family==K_STRING) {
                if(low||high||element||capacity<1||capacity>255||width!=capacity+1)return 0;
            } else {
                if(capacity||element||(family!=K_INTEGER&&family!=K_BOOLEAN&&family!=K_CHAR&&
                   (family<6||!type_id(module,family)))||
                   (width!=1&&width!=2)||(int16_t)low>(int16_t)high)return 0;
                if(family==type&&(low||width!=(high<=255?1:2)))return 0;
            }
        }
        at+=18;
        for(i=0;i<n;++i) {
            if(!(i&31u)&&!cc_poll())return 0;
            if((uint32_t)at+10>blob)return 0;
            role=cc_unit_module_byte(module,at);name=cc_unit_module_byte(module,at+1);tag=cc_unit_module_word(module,at+6);
            next=(uint32_t)at+10+name;
            if(next>blob||!type_id(module,cc_unit_module_word(module,at+2))||cc_unit_module_word(module,at+8))return 0;
            if(role==S_FIELD) {
                if(!identifier(module,at+10,name)||tag!=0xffffu)return 0;
            } else if(role==S_VARIANT) {
                if(name||low!=i||(tag!=0xffffu&&tag>=i))return 0;
            } else if(role==S_VLABEL) {
                if(name||tag!=0xffffu)return 0;
            } else return 0;
            at=(uint16_t)next;
        }
    }
    *cursor=at;return 1;
}

/* Both streams are already sorted by fragment/site. Merge them once rather
 * than comparing every guard operand with every relocation in its routine. */
static uint8_t pc_sites_valid(uint16_t module,uint16_t at,uint16_t pcs,uint16_t blob,
                             uint16_t code_bytes,uint16_t count,uint16_t refbase) {
    uint16_t i,fragment,site,offset,size,last_fragment=0,last_site=0;
    uint16_t cursor=0,limit=0,p=0;
    for(i=0;i<pcs;++i,at+=6) {
        if(!cc_poll())return 0;
        fragment=cc_unit_module_word(module,at);site=cc_unit_module_word(module,at+2);offset=cc_unit_module_word(module,at+4);
        if(fragment>=count||cc_unit_module_word(module,blob+16+fragment*8)>=code_bytes)return 0;
        size=cc_unit_module_word(module,blob+16+fragment*8+2);
        if((uint32_t)site+2>size||offset>=size||
           (i&&(fragment<last_fragment||(fragment==last_fragment&&site<last_site))))return 0;
        if(!i||fragment!=last_fragment) {
            cursor=cc_unit_module_word(module,blob+16+fragment*8+4);
            limit=cursor+cc_unit_module_word(module,blob+16+fragment*8+6);
        }
        while(cursor<limit) {
            p=cc_unit_module_word(module,refbase+8*cursor);
            if(p+2>site)break;
            ++cursor;
        }
        if(cursor<limit&&p<site+2)return 0;
        last_fragment=fragment;last_site=site+2;
    }
    return !cc.error->code;
}
uint8_t cc_unit_validate_object(uint16_t module,const char *name,uint32_t stream_hash) ROUTINES {
    uint16_t n,path,at,end,blob,exports,pcs,maps,i,j,k,p,params,fragment,offset,size,old,next;
    uint16_t refbase,first,count,code_bytes;
    uint32_t expected,wide;
    uint8_t c,mode;CompilerFragmentObject object;
    end=cc_unit_word(module+4);
    if(end<ZPU_HEADER_BYTES)return 0;
    for(i=0;i<4;++i)if(cc_unit_module_byte(module,i)!=(uint8_t)"ZPU1"[i])return 0;
    if(cc_unit_module_word(module,4)!=ZPU_ABI||
       cc_unit_module_word(module,6)!=end)return 0;
    if(!cc_unit_module_word(module,16)||cc_unit_module_word(module,16)>SOURCE_LIMIT)return 0;
    for(i=0;i<4;++i)if(cc_unit_module_byte(module,8+i)!=cc_runtime_profile_get(i)){cc_fail(UNIT_STALE);return 0;}
    expected=module_long(module,28);
    if(expected!=stream_hash)return 0;
    n=cc_unit_module_byte(module,18);path=cc_unit_module_byte(module,19);
    if(!n||n>11||!path||path>127||!identifier(module,ZPU_HEADER_BYTES,n)||!equal_name(module,ZPU_HEADER_BYTES,n,name))return 0;
    for(i=0;i<path;++i)if(!cc_unit_module_byte(module,ZPU_HEADER_BYTES+n+i))return 0;
    blob=cc_unit_module_word(module,26);cc_unit_put(module+UNIT_RECORD_BYTES+10,blob);
    if((uint32_t)blob+CC_FRAGMENT_HEADER>end)return 0;
    cc_unit_descriptor(module,&object);
    if(cc_fragment_validate_source(&object)!=CF_OK)return 0;
    code_bytes=cc_unit_module_word(module,32);
    count=cc_unit_module_word(module,blob+6);refbase=blob+CC_FRAGMENT_HEADER+count*CC_FRAGMENT_RECORD;
    /* Local identities are rebound by the caller, never by mutating the
     * checksum-bound artifact bytes. Dependency IDs are resolved after loading. */
    if(cc_unit_module_word(module,blob+4)!=1)return 0;
    for(i=0;i<cc_unit_module_word(module,blob+8);++i)
        if(!cc_unit_module_word(module,refbase+i*8+2)||cc_unit_module_word(module,refbase+i*8+2)>cc_unit_module_word(module,36)+1)return 0;
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
    if(cc_unit_module_word(module,46)>k)return 0; /* Never the globals fragment. */
    at=ZPU_HEADER_BYTES+n+path;
    if(cc_unit_module_word(module,36)>=CC_FRAGMENT_OBJECTS||
       cc_unit_module_word(module,40)!=at)return 0;
    for(i=0;i<cc_unit_module_word(module,36);++i) {
        if((uint32_t)at+8>blob)return 0;
        k=cc_unit_module_byte(module,at);
        if(!k||k>11||cc_unit_module_byte(module,at+1)||cc_unit_module_word(module,at+2)!=i+2||
           (uint32_t)at+8+k>blob||!identifier(module,at+8,k))return 0;
        for(j=ZPU_HEADER_BYTES+n+path;j<at;j+=8+cc_unit_module_byte(module,j)) {
            if(cc_unit_module_byte(module,j)!=k)continue;
            for(p=0;p<k;++p)if(cc_unit_module_byte(module,j+8+p)!=cc_unit_module_byte(module,at+8+p))break;
            if(p==k)return 0;
        }
        at+=8+k;
    }
    if(cc_unit_module_word(module,42)!=at||!type_records(module,&at,blob)||cc_unit_module_word(module,44)!=at)return 0;
    cc_unit_put(module+10,at);exports=cc_unit_module_word(module,20);
    for(i=0;i<exports;++i) {
        if(!cc_poll())return 0;
        if((uint32_t)at+14>blob)return 0;
        n=cc_unit_module_byte(module,at);c=cc_unit_module_byte(module,at+1);k=cc_unit_module_word(module,at+4);
        params=cc_unit_module_word(module,at+6);fragment=cc_unit_module_word(module,at+8);offset=cc_unit_module_word(module,at+10);
        size=cc_unit_module_word(module,at+12);wide=(uint32_t)at+14+n+4UL*params+size;
        if(wide>blob||!identifier(module,at+14,n)||c>3||cc_unit_module_byte(module,at+3)||
           (cc_unit_module_byte(module,at+2)&~ZPU_CALLER_IO))return 0;
        if(c>=2) {
            if(params||fragment||cc_unit_module_byte(module,at+2)||!type_id(module,k))return 0;
            if(c==3) {if(offset||size||k==K_STRING)return 0;}
            else {
            p=at+14+n;
            if(k==K_STRING) {if(!size||size>256||size!=1+cc_unit_module_byte(module,p)||offset)return 0;}
            else if(k==K_REAL) {if(size!=6||offset)return 0;}
            else if(size&&(size!=4||k<6))return 0;
            }
        } else {
            if((c?(!type_id(module,k)||k==K_STRING):k!=TYPE_NONE)||fragment>=count||size)return 0;
            p=cc_unit_module_word(module,blob+16+fragment*8+2);
            if(offset>=p||cc_unit_module_word(module,blob+16+fragment*8)>=code_bytes)return 0;
        }
        old=cc_unit_word(module+10);
        for(j=0;j<i;++j) {
            next=old+14+cc_unit_module_byte(module,old)+4*cc_unit_module_word(module,old+6)+cc_unit_module_word(module,old+12);
            if(cc_unit_module_byte(module,old)==n) {
                for(k=0;k<n;++k)if(cc_unit_module_byte(module,old+14+k)!=cc_unit_module_byte(module,at+14+k))break;
                if(k==n)return 0;
            }
            old=next;
        }
        for(j=0;j<params;++j) {
            p=at+14+n+4*j;mode=cc_unit_module_byte(module,p);k=cc_unit_module_word(module,p+2);
            if(mode>1||cc_unit_module_byte(module,p+1)||(!type_id(module,k)&&!(mode&&k==TYPE_NONE))||k==K_STRING)return 0;
        }
        at=(uint16_t)wide;
    }
    pcs=cc_unit_module_word(module,22);maps=cc_unit_module_word(module,24);
    wide=at;wide+=6UL*pcs;wide+=8UL*maps;wide+=count;if(wide!=blob)return 0;
    if(!pc_sites_valid(module,at,pcs,blob,code_bytes,count,refbase))return 0;
    at+=6u*pcs;
    for(i=0;i<maps;++i,at+=8) {
        first=cc_unit_module_word(module,at);n=cc_unit_module_word(module,at+2);
        if(first>=n||n>code_bytes||cc_unit_module_word(module,at+4)||
           cc_unit_module_word(module,at+6)>cc_unit_module_word(module,16))return 0;
    }
    for(i=0;i<count;++i) {
        c=cc_unit_module_byte(module,at+i);
        if(c>(ZPU_REAL|ZPU_FULL_MEMORY|ZPU_HARDWARE)||(i+1==count&&cc_unit_module_word(module,34)&&c))return 0;
    }
    return !cc.error->code;
}
