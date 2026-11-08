#include "units/internal.h"

/* Compile-time metadata lives in the sealed artifact, never in emitted code.
 * Type IDs are local to a file; imported aliases retain their canonical owner. */
#define TYPE_HEADER 18u
#define MEMBER_HEADER 10u
#define EXPORT_HEADER 14u
enum { UT_VISITING=1,UT_READY,UT_POINTER };
static void put_byte(uint8_t page,uint16_t *at,uint8_t value) {
    if(*at>=ZPU_ARTIFACT_LIMIT||
       (!(*at%PAGE_SIZE)&&!buffer_reserve(page,*at+1))){cc_fail(99);return;}
    buffer_set(page,(*at)++,value);
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
static SourcePosition position(uint16_t module,uint16_t at) {
    return ((SourcePosition)(0x8000u|module)<<16)|at;
}
static uint16_t origin(TypeId type) {
    uint16_t at,end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES) {
        if(!(at&127u)&&!cc_poll())return 0;
        if(cc_unit_word(at)==UR_TYPE&&cc_unit_word(at+6)==type)return at;
    }
    return 0;
}
static uint8_t member(uint16_t symbol,TypeId owner) {
    if(!(symbol&63u)&&!cc_poll())return 0;
    return CCS(symbol,owner)==owner&&CCS(symbol,role)>=S_FIELD&&CCS(symbol,role)<=S_VLABEL;
}
uint32_t cc_unit_types_bytes(void) ROUTINES {
    uint32_t bytes=0;TypeId type;uint16_t i;
    for(type=6;type<cc.type_count;++type) {
        if(!cc_poll())return 0;
        bytes+=TYPE_HEADER;if(origin(type))continue;
        if(cc.error->code)return 0;
        if(CCT(type,family)==K_RECORD)for(i=0;i<cc.symbol_count&&!cc.error->code;++i)
            if(member(i,type))bytes+=MEMBER_HEADER+(CCS(i,role)==S_FIELD?CCS(i,name_length):0);
    }
    return bytes;
}
void cc_unit_types_write(uint8_t page,uint16_t *at) ROUTINES {
    TypeId type,family;uint16_t i,j,n,base,ref,tag;
    for(type=6;type<cc.type_count&&!cc.error->code;++type) {
        if(!cc_poll())return;
        ref=origin(type);if(cc.error->code)return;
        if(ref) {
            for(i=0;i<6;++i)put_word(page,at,0);
            put_word(page,at,cc_unit_word(cc_unit_word(ref+2)+8)+1);
            put_word(page,at,cc_unit_word(ref+4));put_word(page,at,0);continue;
        }
        family=CCT(type,family);n=0;base=0xffffu;
        if(family==K_RECORD)for(i=0;i<cc.symbol_count&&!cc.error->code;++i)if(member(i,type)) {
            if(CCS(i,role)==S_VARIANT)base=n;++n;
        }
        put_word(page,at,family);put_word(page,at,CCT(type,width));
        put_word(page,at,family==K_RECORD?base:family==K_POINTER?0:(uint16_t)CCT(type,low));
        put_word(page,at,family==K_POINTER?0:(uint16_t)CCT(type,high));
        put_word(page,at,family==K_POINTER?0:CCT(type,capacity));
        put_word(page,at,CCT(type,element));put_word(page,at,0);put_word(page,at,0);put_word(page,at,n);
        if(family==K_RECORD)for(i=0;i<cc.symbol_count&&!cc.error->code;++i)if(member(i,type)) {
            put_byte(page,at,CCS(i,role));put_byte(page,at,CCS(i,role)==S_FIELD?CCS(i,name_length):0);
            put_word(page,at,CCS(i,type));put_word(page,at,CCS(i,address));tag=0xffffu;
            if(CCS(i,role)==S_VARIANT&&CCS(i,name_start)!=0xffffu) {
                tag=0;for(j=0;j<CCS(i,name_start)&&!cc.error->code;++j)if(member(j,type))++tag;
            }
            put_word(page,at,tag);put_word(page,at,0);
            if(CCS(i,role)==S_FIELD)put_name(page,at,i);
        }
    }
}
static uint16_t literal_bytes(uint16_t symbol) {
    if(CCS(symbol,role)!=S_CONSTANT)return 0;
    if(cc_family(CCS(symbol,type))==K_REAL)return 6;
    if(cc_family(CCS(symbol,type))==K_LONGWORD)return 4;
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
        } else if(n&&cc_family(CCS(symbol,type))==K_LONGWORD) {
            uint32_t number=cc_unsigned_literal(CCS(symbol,address));
            for(i=0;i<4;++i)put_byte(page,at,(uint8_t)(number>>(8*i)));
        } else if(n) {
            put_byte(page,at,(uint8_t)cc.literal_length);for(i=0;i<cc.literal_length;++i)put_byte(page,at,cc.literal[i]);
        }
    }
}
static uint16_t type_at(uint16_t module,TypeId type) {
    uint16_t at=cc_unit_module_word(module,42),i,n;
    for(i=6;i<type;++i) {
        if(!(i&31u)&&!cc_poll())return 0;
        n=cc_unit_module_word(module,at+16);at+=TYPE_HEADER;
        while(n--){at+=MEMBER_HEADER+cc_unit_module_byte(module,at+1);}
    }
    return at;
}
static uint16_t cached(uint16_t module,TypeId wire) {
    uint16_t at,end=cc_unit_word(UH_USED);
    for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
        if(cc_unit_word(at)==UR_TYPE&&cc_unit_word(at+2)==module&&cc_unit_word(at+4)==wire)return at;
    return 0;
}
static TypeId import_type(uint16_t module,TypeId wire) {
    uint16_t at,ref,n,i,id,first,tag,marker,largest=0,value;uint32_t prefix=0;
    TypeId type,family,element,capacity,actual;int16_t low,high;uint16_t width;uint8_t role,name,variant=0;
    if(cc.error->code)return TYPE_NONE;
    if(wire<6)return wire;
    if(wire-6>=cc_unit_module_word(module,38)){cc_fail(UNIT_FORMAT);return TYPE_NONE;}
    if(!cc_stack_check())return TYPE_NONE;
    ref=cached(module,wire);
    if(ref) {
        type=cc_unit_word(ref+6);
        if(cc_unit_word(ref+8)==UT_VISITING)cc_fail(UNIT_FORMAT);
        return type;
    }
    at=type_at(module,wire);if(cc.error->code)return TYPE_NONE;
    if(cc_unit_module_word(module,at+12)) {
        actual=import_type(cc_unit_target(module,cc_unit_module_word(module,at+12)),cc_unit_module_word(module,at+14));
        cc_unit_record(UR_TYPE,module,wire,actual,UT_READY,0);return actual;
    }
    family=cc_unit_module_word(module,at);width=cc_unit_module_word(module,at+2);
    low=(int16_t)cc_unit_module_word(module,at+4);high=(int16_t)cc_unit_module_word(module,at+6);
    capacity=cc_unit_module_word(module,at+8);element=cc_unit_module_word(module,at+10);n=cc_unit_module_word(module,at+16);
    type=(family==K_LONGWORD||(family==K_WORD&&width==2&&!low&&(uint16_t)high==65535u))?
        cc_unsigned_type(family==K_LONGWORD):cc_new_type(family,width,low,high,capacity);ref=cc_unit_record(UR_TYPE,module,wire,type,UT_VISITING,0);
    if(cc.error->code)return TYPE_NONE;
    if(family==K_ARRAY) {
        capacity=import_type(module,capacity);element=import_type(module,element);
        if(cc.error->code)return TYPE_NONE;
        if(!cc_ordinal(capacity)||
           (cc_family(capacity)==K_WORD?
           ((uint32_t)(uint16_t)CCT(capacity,high)-(uint16_t)CCT(capacity,low)+1)*CCT(element,width):
           ((uint32_t)((int32_t)CCT(capacity,high)-CCT(capacity,low))+1)*CCT(element,width))!=width)cc_fail(UNIT_FORMAT);
        CCT_SET(type,capacity,capacity);CCT_SET(type,element,element);
    } else if(family==K_POINTER) {
        /* A pointer has a fixed layout independent of its target. Resolve
         * targets after the value graph, so suspended pointer paths cannot
         * be mistaken for recursive values. State3 owns an unbound wire ID. */
        if(width!=2){cc_fail(UNIT_FORMAT);return TYPE_NONE;}
        CCT_SET(type,element,element);cc_unit_put(ref+8,UT_POINTER);return type;
    } else if(family==K_SET) {
        element=import_type(module,element);if(cc.error->code)return TYPE_NONE;CCT_SET(type,element,element);
        if(!cc_ordinal(element)||CCT(element,low)<0||CCT(element,high)>255||
           (cc_family(element)==K_WORD&&(uint16_t)CCT(element,high)>255)||
           width!=(CCT(element,high)>>3)-(CCT(element,low)>>3)+1)cc_fail(UNIT_FORMAT);
    } else if(family==K_FILE) {
        if(capacity==FILE_TYPED) {
            element=import_type(module,element);if(cc.error->code)return TYPE_NONE;
            if(cc_family(element)==K_FILE)cc_fail(UNIT_FORMAT);
        }
        CCT_SET(type,element,element);
    } else if(family==K_RECORD) {
        first=cc.symbol_count;marker=0xffffu;
        if((uint32_t)first+n>SYMBOL_LIMIT){cc_fail(99);return TYPE_NONE;}
        for(i=0;i<n;++i){id=cc.symbol_count++;cc_symbol_clear(id);CCS_SET(id,owner,type);}
        at+=TYPE_HEADER;
        for(i=0;i<n&&!cc.error->code;++i) {
            if(!(i&31u)&&!cc_poll())return TYPE_NONE;
            id=first+i;role=cc_unit_module_byte(module,at);name=cc_unit_module_byte(module,at+1);
            actual=import_type(module,cc_unit_module_word(module,at+2));value=cc_unit_module_word(module,at+4);tag=cc_unit_module_word(module,at+6);
            if(cc.error->code)return TYPE_NONE;
            CCS_SET(id,role,role);CCS_SET(id,type,actual);CCS_SET(id,address,value);
            if(role==S_FIELD) {
                if(variant||value!=prefix||prefix+CCT(actual,width)>width){cc_fail(UNIT_FORMAT);break;}
                prefix+=CCT(actual,width);CCS_SET(id,name_length,name);
                CCS_SET(id,name_start,cc_source_reference(position(module,at+MEMBER_HEADER)));
            } else if(role==S_VARIANT) {
                if(variant||!cc_ordinal(actual)||value!=prefix||
                   (tag!=0xffffu&&(tag>=i||CCS(first+tag,role)!=S_FIELD||CCS(first+tag,type)!=actual))){cc_fail(UNIT_FORMAT);break;}
                variant=1;marker=i;CCS_SET(id,name_start,tag==0xffffu?tag:first+tag);
            } else if(role==S_VLABEL) {
                if(!variant||cc_family(actual)!=K_RECORD){cc_fail(UNIT_FORMAT);break;}
                if(CCT(actual,width)>largest)largest=CCT(actual,width);
            } else cc_fail(UNIT_FORMAT);
            at+=MEMBER_HEADER+name;
        }
        if((uint32_t)prefix+largest!=width||(uint16_t)low!=marker)cc_fail(UNIT_FORMAT);
        CCT_SET(type,low,marker==0xffffu?-1:(int16_t)(first+marker));
    } else if(family==K_WORD||family==K_LONGWORD) {
        if(capacity||element||(family==K_LONGWORD?(width!=4||low||(uint16_t)high!=65535u):
           ((width!=1&&width!=2)||(uint16_t)low>(uint16_t)high||(width==1&&(uint16_t)high>255))))cc_fail(UNIT_FORMAT);
    } else if(family==K_STRING) {
        if(!capacity||capacity>255||width!=capacity+1)cc_fail(UNIT_FORMAT);
    } else if(family<K_ARRAY) {
        if(family>=6) {
            actual=family==wire?type:import_type(module,family);if(cc.error->code)return TYPE_NONE;CCT_SET(type,family,actual);
            if(family!=wire&&(CCT(actual,family)!=actual||low<CCT(actual,low)||high>CCT(actual,high)||width!=CCT(actual,width)))cc_fail(UNIT_FORMAT);
        }
        if(low>high||(width!=1&&width!=2)||
           (family==K_BOOLEAN&&(low<0||high>1||width!=1))||
           (family==K_CHAR&&(low<0||high>255||width!=1))||
           (family==K_INTEGER&&width==1&&(low<0||high>255))||family==K_REAL||family==K_BYTE)cc_fail(UNIT_FORMAT);
    } else cc_fail(UNIT_FORMAT);
    if(n&&family!=K_RECORD)cc_fail(UNIT_FORMAT);
    cc_unit_put(ref+8,UT_READY);return type;
}
/* Resolving a target may append more descriptors. The arena itself is the
 * bounded worklist, including pointers reached through external aliases. */
static uint8_t bind_pointers(void) {
    uint16_t at,module;TypeId type,target;
    for(at=UNIT_RECORD_BASE;at<cc_unit_word(UH_USED)&&!cc.error->code;at+=UNIT_RECORD_BYTES) {
        if(!(at&127u)&&!cc_poll())return 0;
        if(cc_unit_word(at)!=UR_TYPE||cc_unit_word(at+8)!=UT_POINTER)continue;
        module=cc_unit_word(at+2);type=cc_unit_word(at+6);
        target=import_type(module,CCT(type,element));if(cc.error->code)return 0;
        CCT_SET(type,element,target);cc_unit_put(at+8,UT_READY);
    }
    return !cc.error->code;
}
/* Validate the complete type graph and all signatures, including unused
 * exports. Only explicitly named exports acquire lexical symbols. */
uint8_t cc_unit_types_ready(uint16_t module) ROUTINES {
    uint16_t i,at=cc_unit_word(module+10),count=cc_unit_module_word(module,20),n,p,value,data;
    uint8_t role,mode;TypeId type;
    for(i=6;i<6+cc_unit_module_word(module,38)&&!cc.error->code;++i)import_type(module,i);
    if(!bind_pointers())return 0;
    for(i=0;i<count&&!cc.error->code;++i) {
        if(!cc_poll())return 0;
        role=cc_unit_module_byte(module,at+1);type=cc_unit_module_word(module,at+4);
        if(type!=TYPE_NONE)type=import_type(module,type);
        if(cc.error->code)return 0;
        n=cc_unit_module_word(module,at+6);p=at+EXPORT_HEADER+cc_unit_module_byte(module,at);
        value=cc_unit_module_word(module,at+10);data=cc_unit_module_word(module,at+12);
        if(role==2) {
            if(type==K_STRING) {if(!data)cc_fail(UNIT_FORMAT);}
            else if(type==K_REAL) {if(data!=6)cc_fail(UNIT_FORMAT);}
            else if(cc_family(type)==K_LONGWORD){if(data!=4||value)cc_fail(UNIT_FORMAT);}
            else if(cc_family(type)==K_WORD){if(data||value<(uint16_t)CCT(type,low)||value>(uint16_t)CCT(type,high))cc_fail(UNIT_FORMAT);}
            else if(!cc_ordinal(type)||data||(int16_t)value<CCT(type,low)||(int16_t)value>CCT(type,high))cc_fail(UNIT_FORMAT);
        } else if(role==3) {
            if(type==K_STRING)cc_fail(UNIT_FORMAT);
        } else if(role==1&&(type==K_STRING||cc_aggregate(type)||cc_family(type)==K_SET||cc_family(type)==K_FILE))cc_fail(UNIT_FORMAT);
        for(value=0;value<n&&!cc.error->code;++value) {
            if(!(value&31u)&&!cc_poll())return 0;
            mode=cc_unit_module_byte(module,p+4*value);type=cc_unit_module_word(module,p+4*value+2);
            if(type!=TYPE_NONE)type=import_type(module,type);
            if(type==K_STRING||(!mode&&type==TYPE_NONE)||(!mode&&cc_family(type)==K_FILE))cc_fail(UNIT_FORMAT);
        }
        at=p+4*n+data;
    }
    return !cc.error->code;
}
