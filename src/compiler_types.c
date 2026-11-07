#include "compiler.h"

static TypeId parse_type(uint8_t allow_forward);
static uint16_t metadata(uint8_t role,TypeId owner) {
    uint16_t id=cc.symbol_count;
    if(id==SYMBOL_LIMIT) { cc_fail(99); return 0; }
    cc_symbol_clear(id);
    CCS_SET(id,role,role); CCS_SET(id,owner,owner); ++cc.symbol_count;
    return id;
}
static uint16_t new_field(TypeId type,TypeId root) {
    uint16_t id;
    if(cc.token!=T_ID) { cc_fail(53); return 0; }
    if(cc_field_lookup(root)>=0) { cc_fail(43); return 0; }
    id=metadata(S_FIELD,type);
    CCS_SET(id,name_start,cc_source_reference(cc.token_start)); CCS_SET(id,name_length,(uint8_t)strlen(cc.name));
    CCS_SET(id,type,TYPE_NONE);
    return id;
}
static void extend_record(TypeId type,uint16_t bytes) {
    if((uint32_t)CCT(type,width)+bytes>32767u) cc_fail(98);
    else CCT_SET(type,width,CCT(type,width)+(bytes));
}
static void field_list(TypeId type,TypeId root) {
    uint16_t first,last,i,marker,tag=0xffffu,label,value;
    SourceCursor tag_cursor;
    uint16_t largest=0;
    TypeId field_type;TypeId actual;TypeId child;
    int16_t named;
    if(++cc.depth==0||!cc_stack_check()) { cc_fail(99); --cc.depth; return; }
    while(cc.token==T_ID&&!cc.error->code) {
        first=cc.symbol_count; new_field(type,root); cc_next();
        while(cc.token==','&&!cc.error->code) { cc_next(); new_field(type,root); cc_next(); }
        last=cc.symbol_count; cc_expect(':',2); field_type=parse_type(0);
        for(i=first;i<last&&!cc.error->code;++i) {
            CCS_SET(i,type,field_type); CCS_SET(i,address,CCT(type,width));
            extend_record(type,CCT(field_type,width));
        }
        if(cc.token!=';') { if(cc.token==T_ID||cc_is("case")) cc_fail(1); break; }
        cc_next();
    }
    if(cc_is("case")&&!cc.error->code) {
        cc_next();named=cc_named_type();cc_source_save(&tag_cursor);cc_next();
        if(cc.token==':') {
            cc_source_restore(&tag_cursor);tag=new_field(type,root); cc_next(); cc_expect(':',2);
            named=cc_named_type(); cc_next();
        }
        if(named<0) cc_fail(tag==0xffffu?2:41);
        else if(!cc_ordinal((TypeId)named)) cc_fail(30);
        field_type=named<0?K_INTEGER:(TypeId)named;
        if(tag!=0xffffu) {
            CCS_SET(tag,type,field_type); CCS_SET(tag,address,CCT(type,width));
            extend_record(type,CCT(field_type,width));
        }
        marker=metadata(S_VARIANT,type); CCS_SET(marker,type,field_type);
        CCS_SET(marker,address,CCT(type,width)); CCS_SET(marker,name_start,tag);
        CCT_SET(type,low,(int16_t)marker); cc_keyword("of",15);
        while(!cc.error->code&&cc.token!=')'&&!cc_is("end")&&cc.token!=T_EOF) {
            child=cc_new_type(K_RECORD,0,-1,0,0);
            do {
                cc_constant(&actual,&value);
                if(cc_family(actual)!=cc_family(field_type)) cc_fail(44);
                label=metadata(S_VLABEL,type); CCS_SET(label,type,child); CCS_SET(label,address,value);
                if(cc.token!=',') break; cc_next();
            } while(!cc.error->code);
            cc_expect(':',2); cc_expect('(',4); field_list(child,root); cc_expect(')',5);
            if(CCT(child,width)>largest) largest=CCT(child,width);
            if(cc.token!=';') break; cc_next();
        }
        extend_record(type,largest);
    }
    --cc.depth;
}
static TypeId array_dimension(void) {
    TypeId index;TypeId array;TypeId element;
    uint32_t width;
    if(++cc.depth==0||!cc_stack_check()) { cc_fail(99); --cc.depth; return 0; }
    index=parse_type(0);
    if(!cc_ordinal(index)) cc_fail(30);
    array=cc_new_type(K_ARRAY,0,0,0,index);
    if(cc.token==',') { cc_next(); element=array_dimension(); }
    else { cc_expect(']',9); cc_keyword("of",15); element=parse_type(0); }
    if(cc.error->code) { --cc.depth; return array; }
    width=((uint32_t)((int32_t)CCT(index,high)-CCT(index,low))+1u)*CCT(element,width);
    if(width>32767u) cc_fail(98);
    CCT_SET(array,width,(uint16_t)width); CCT_SET(array,element,element);
    --cc.depth; return array;
}
static TypeId parse_type_body(uint8_t allow_forward) {
    int16_t id;
    uint16_t lo,hi,count,member;
    TypeId base;TypeId other;TypeId result;
    if(cc.token=='^') {
        cc_next();
        if(cc.token!=T_ID) { cc_fail(42); return K_INTEGER; }
        id=allow_forward?-1:cc_named_type();
        if(!allow_forward&&id<0) { cc_fail(42); return K_INTEGER; }
        result=cc_new_type(K_POINTER,2,(int16_t)cc_source_reference(cc.token_start),allow_forward,cc.scope);
        CCT_SET(result,element,allow_forward?TYPE_NONE:(TypeId)id); cc_next();
#ifdef ZP_UNITS_EXPERIMENT
        if(allow_forward&&cc.token=='.') {
            CCT_SET(result,high,2); /* Pending qualified target. */
            cc_next();if(cc.token!=T_ID){cc_fail(42);return result;}cc_next();
        }
#endif
        return result;
    }
    if(cc_is("packed")) { cc_next(); return parse_type(allow_forward); }
    if(cc_is("set")) {
        cc_next(); cc_keyword("of",15); base=parse_type(0);
        if(!cc_ordinal(base)) { cc_fail(30); return 0; }
        if(CCT(base,low)<0||CCT(base,high)>255) { cc_fail(70); return 0; }
        result=cc_new_type(K_SET,(CCT(base,high)>>3)-(CCT(base,low)>>3)+1,0,0,0);
        CCT_SET(result,element,base); return result;
    }
    if(cc_is("array")) { cc_next(); cc_expect('[',8); return array_dimension(); }
    if(cc_is("record")) {
        result=cc_new_type(K_RECORD,0,-1,0,0); cc_next(); field_list(result,result);
        cc_keyword("end",14); return result;
    }
    if(cc_is("file")) {
        cc_next(); base=TYPE_NONE;
        if(cc_is("of")) {
            cc_next(); base=parse_type(0);
            if(cc_family(base)==K_FILE) cc_fail(68);
        }
        result=cc_new_type(K_FILE,176,0,0,base==TYPE_NONE?FILE_UNTYPED:FILE_TYPED);
        CCT_SET(result,element,base);
        return result;
    }
    if(cc_is("string")) {
        cc_next(); cc_expect('[',8); cc_constant(&base,&lo);
        if(cc_family(base)!=K_INTEGER||lo<1||lo>255) cc_fail(49);
        cc_expect(']',9);
        return cc_new_type(K_STRING,lo+1,0,0,(uint8_t)lo);
    }
    if(cc.token=='(') {
        result=cc_new_type(cc.type_count,1,0,0,0); count=0; cc_next();
        do {
            member=cc_new_symbol(S_CONSTANT);
            CCS_SET(member,type,result); CCS_SET(member,address,count++);
            cc_next(); if(cc.token!=',') break; cc_next();
        } while(!cc.error->code);
        cc_expect(')',5);
        CCT_SET(result,high,(int16_t)(count-1)); CCT_SET(result,width,count<=256?1:2);
        return result;
    }
    id=cc.token==T_ID?cc_lookup():-1;
    if(id>=0&&CCS(id,role)==S_TYPE) { result=CCS(id,type); cc_next(); return result; }
    if(id<0&&(id=cc_predefined_type())>=0) { cc_next(); return (TypeId)id; }
    cc_constant(&base,&lo);
    if(cc_family(base)==K_STRING) cc_fail(51);
    else if(!cc_ordinal(base)) cc_fail(30);
    cc_expect(T_RANGE,41); cc_constant(&other,&hi);
    if(cc_family(base)!=cc_family(other)) cc_fail(51);
    if((int16_t)lo>(int16_t)hi) cc_fail(52);
    return cc_new_type(cc_family(base),cc_family(base)==K_INTEGER?(lo<=255&&hi<=255?1:2):CCT(base,width),
                    (int16_t)lo,(int16_t)hi,0);
}
static TypeId parse_type(uint8_t allow_forward) {
    TypeId type;
    if(++cc.depth==0||!cc_stack_check()) { cc_fail(99); --cc.depth; return 0; }
    type=parse_type_body(allow_forward); --cc.depth; return type;
}
/* Validate initializers when declared; replay their immutable source to emit
 * the typed payload after declarations. No initializer pointers escape a bank. */
static void initializer(TypeId type,uint8_t emit);
static void set_initializer(TypeId type,uint8_t emit) {
    uint8_t bits[32];TypeId actual;TypeId base=CCT(type,element);
    uint16_t low,high,i;
    memset(bits,0,sizeof(bits)); cc_expect('[',8);
    while(cc.token!=']'&&!cc.error->code) {
        cc_constant(&actual,&low);
        if(cc_family(actual)!=cc_family(base)) cc_fail(44);
        if(cc.token==T_RANGE) {
            cc_next(); cc_constant(&actual,&high);
            if(cc_family(actual)!=cc_family(base)) cc_fail(44);
        } else high=low;
        if((int16_t)low<CCT(base,low)||(int16_t)low>CCT(base,high)||
           (int16_t)high<CCT(base,low)||(int16_t)high>CCT(base,high)) cc_fail(45);
        for(i=low;i<=high&&!cc.error->code;++i) bits[i>>3]|=(uint8_t)(1u<<(i&7));
        if(cc.token!=',') break; cc_next();
    }
    cc_expect(']',9);
    if(emit) for(i=0;i<CCT(type,width);++i) cc_emit(bits[(CCT(base,low)>>3)+i]);
}
static void record_initializer(TypeId type,uint8_t emit) {
    uint16_t i,bytes=0;
    int16_t field;
    uint8_t used=0;TypeId child=TYPE_NONE;
    for(i=0;i<cc.symbol_count&&!cc.error->code;++i) if(CCS(i,role)==S_FIELD&&CCS(i,owner)==type) {
        if(used&&cc.token==')') break;
        if(used) cc_expect(';',1);
        field=cc.token==T_ID?cc_field_lookup(type):-1;
        if(field<0) cc_fail(41); else if(field!=(int16_t)i) cc_fail(69);
        cc_next(); cc_expect(':',2); initializer(CCS(i,type),emit);
        bytes+=CCT(CCS(i,type),width); used=1;
    }
    if(CCT(type,low)>=0) {
        if(used&&cc.token==';') cc_next();
        if(cc.token==T_ID) {
            for(i=0;i<cc.symbol_count;++i) if(CCS(i,role)==S_VLABEL&&CCS(i,owner)==type&&
                cc_field_lookup(CCS(i,type))>=0) { child=CCS(i,type); break; }
            if(child==TYPE_NONE) cc_fail(41);
            else { record_initializer(child,emit); bytes+=CCT(child,width); }
        }
    }
    if(emit) while(bytes<CCT(type,width)&&!cc.error->code) { cc_emit(0); ++bytes; }
}
static void initializer(TypeId type,uint8_t emit) {
    uint16_t value,count,i;
    TypeId actual;TypeId element;
    if(cc_family(type)==K_SET) { set_initializer(type,emit); return; }
    if(cc_family(type)==K_RECORD) {
        cc_expect('(',4);
        if(!CCT(type,width)) cc_fail(41);
        record_initializer(type,emit); cc_expect(')',5); return;
    }
    if(cc_family(type)==K_ARRAY) {
        element=CCT(type,element);
        count=(uint16_t)((int32_t)CCT(CCT(type,capacity),high)-
                        CCT(CCT(type,capacity),low)+1);
        if(cc_char_array(type)&&cc.token!='(') {
            cc_constant(&actual,&value);
            if(actual!=K_STRING) { cc_fail(60); return; }
            cc_constant_literal(value);
            if(cc.literal_length!=count) cc_fail(60);
            if(emit) for(i=0;i<count&&!cc.error->code;++i) cc_emit(cc.literal[i]);
        } else {
            cc_expect('(',4);
            for(i=0;i<count&&!cc.error->code;++i) {
                if(i) cc_expect(',',3);
                initializer(element,emit);
            }
            cc_expect(')',5);
        }
        return;
    }
    if(cc_family(type)==K_POINTER||cc_family(type)==K_FILE) { cc_fail(61); return; }
    cc_constant(&actual,&value);
    if(!cc_compatible(type,actual)) cc_fail(44);
    if(cc_family(type)==K_REAL) {
        cc_real_constant(actual,value);
        if(emit) for(i=0;i<6;++i) cc_emit(zr_result[i]);
    } else if(cc_family(type)==K_STRING) {
        if(cc_family(actual)==K_CHAR) { cc.literal[0]=(uint8_t)value; cc.literal_length=1; }
        else cc_constant_literal(value);
        if(cc.literal_length>CCT(type,capacity)) cc.literal_length=CCT(type,capacity);
        if(emit) {
            cc_emit((uint8_t)cc.literal_length);
            for(i=0;i<CCT(type,capacity);++i) cc_emit(i<cc.literal_length?cc.literal[i]:0);
        }
    } else {
        if(cc_family(type)==K_CHAR&&cc_family(actual)==K_STRING) {
            cc_constant_literal(value);
            if(cc.literal_length!=1) cc_fail(50);
            value=cc.literal[0];
        }
        if((int16_t)value<CCT(type,low)||(int16_t)value>CCT(type,high)) cc_fail(44);
        if(emit) { cc_emit((uint8_t)value); if(CCT(type,width)==2) cc_emit((uint8_t)(value>>8)); }
    }
}
static void resolve_pointers(void) {
    TypeId i;
    SourceCursor cursor;
    int16_t target;SourcePosition position;
    cc_source_save(&cursor);
    for(i=5;i<cc.type_count&&!cc.error->code;++i)
        if(CCT(i,family)==K_POINTER&&CCT(i,high)&&CCT(i,capacity)==cc.scope) {
            position=cc_reference_position((uint16_t)CCT(i,low));
            cc_source_seek(position);cc_next();target=cc_named_type();
            if(target<0||(CCT(i,high)==2&&cc.token_start==position)) { cc_fail(42); return; }
            CCT_SET(i,element,(TypeId)target); CCT_SET(i,high,0);
        }
    cc_source_restore(&cursor);
}
void cc_declarations(void) FRONTEND {
    uint16_t id,first,count,i,value,total;
    int16_t alias;
    uint8_t section;TypeId type;TypeId initial;
    while((cc_is("var")||cc_is("const")||cc_is("type"))&&!cc.error->code) {
        section=cc_is("var")?S_VARIABLE:cc_is("const")?S_CONSTANT:S_TYPE; cc_next();
        while(cc.token==T_ID&&!cc.error->code) {
            first=cc.symbol_count;
            id=cc_new_symbol(section==S_VARIABLE?S_VARIABLE:S_PENDING); cc_next();
            if(section==S_VARIABLE) {
                while(cc.token==','&&!cc.error->code) { cc_next(); cc_new_symbol(S_VARIABLE); cc_next(); }
                count=cc.symbol_count-first; cc_expect(':',2); type=parse_type(0);
                if(cc_is("absolute")) {
                    if(count!=1) cc_fail(75);
                    cc_next(); total=cc.symbol_count;
                    /* A declaration cannot bind its own unfinished symbol.
                     * Older lexical declarations remain visible while parsing. */
                    cc.symbol_count=first;
                    alias=cc.token==T_ID?cc_lookup():-1;
                    if(cc_variable(alias)&&CCS(alias,role)!=S_RESULT) {
                        while(CCS(alias,role)==S_ALIAS) alias=(int16_t)CCS(alias,address);
                        value=(uint16_t)alias; initial=S_ALIAS; cc_next();
                    } else {
                        cc_constant(&initial,&value);
                        if(cc_family(initial)!=K_INTEGER) cc_fail(22);
                        initial=S_ADDRESS;
                    }
                    cc.symbol_count=total;
                    CCS_SET(first,type,type); CCS_SET(first,role,initial);
                    CCS_SET(first,address,value);
                } else for(i=first;i<first+count&&!cc.error->code;++i) {
                    CCS_SET(i,type,type); cc_storage(i,CCT(type,width));
                }
            } else if(section==S_TYPE) {
                cc_expect('=',6); type=parse_type(1); CCS_SET(id,type,type); CCS_SET(id,role,S_TYPE);
            } else {
                if(cc.token==':') {
                    cc_next(); type=parse_type(0);
                    if(cc_family(type)==K_POINTER||cc_family(type)==K_FILE) cc_fail(61);
                    cc_expect('=',6);
                    if(cc_aggregate(type)||cc_family(type)==K_SET) {
                        value=cc_source_reference(cc.token_start);initializer(type,0); initial=255;
                    } else cc_constant(&initial,&value);
                    if(cc_aggregate(type)||cc_family(type)==K_SET) {
                        CCS_SET(id,type,type); CCS_SET(id,role,S_TYPED);
                        CCS_SET(id,address,value); cc_expect(';',1); continue;
                    }
                    if(!cc_compatible(type,initial)) cc_fail(44);
                    if(cc_family(type)==K_CHAR&&cc_family(initial)==K_STRING) {
                        cc_constant_literal(value);
                        if(cc.literal_length!=1) cc_fail(50);
                        value=cc.literal[0]; initial=K_CHAR;
                    }
                    if(cc_ordinal(type)&&((int16_t)value<CCT(type,low)||(int16_t)value>CCT(type,high))) cc_fail(44);
                    CCS_SET(id,type,type); CCS_SET(id,initial_type,initial); CCS_SET(id,role,S_TYPED);
                } else {
                    cc_expect('=',6);
                    if(cc.token=='['||cc_is("nil")) cc_fail(1);
                    cc_constant(&type,&value);
                    CCS_SET(id,type,type); CCS_SET(id,role,S_CONSTANT);
                }
                CCS_SET(id,address,value);
            }
            cc_expect(';',1);
        }
        if(!cc.error->code) resolve_pointers();
    }
}
void cc_typed_storage(uint16_t first) FRONTEND {
    uint16_t i,j,value,jump=0;
    SourceCursor cursor;
    for(i=first;i<cc.symbol_count&&!cc.error->code;++i) if(CCS(i,role)==S_TYPED) {
        if(!jump) jump=cc_forward(0xc3);
        value=CCS(i,address); CCS_SET(i,address,cc.emitted);
        if(cc_aggregate(CCS(i,type))||cc_family(CCS(i,type))==K_SET) {
            cc_source_save(&cursor);
            cc_reference_seek(value);cc_next();initializer(CCS(i,type),1);
            cc_source_restore(&cursor);
        } else if(cc_family(CCS(i,type))==K_REAL) {
            cc_real_constant(CCS(i,initial_type),value);
            for(j=0;j<6;++j) cc_emit(zr_result[j]);
        } else if(cc_family(CCS(i,type))==K_STRING) {
            if(cc_family(CCS(i,initial_type))==K_CHAR) { cc.literal[0]=(uint8_t)value; cc.literal_length=1; }
            else cc_constant_literal(value);
            if(cc.literal_length>CCT(CCS(i,type),capacity)) cc.literal_length=CCT(CCS(i,type),capacity);
            cc_emit((uint8_t)cc.literal_length);
            for(j=0;j<CCT(CCS(i,type),capacity);++j) cc_emit(j<cc.literal_length?cc.literal[j]:0);
        } else {
            cc_emit((uint8_t)value); if(CCT(CCS(i,type),width)==2) cc_emit((uint8_t)(value>>8));
        }
    }
    if(jump) cc_patch(jump);
}

TypeId cc_new_type(TypeId family,uint16_t width,int16_t low,int16_t high,TypeId capacity) FRONTEND {
    TypeId id=cc.type_count;
    if(id==TYPE_LIMIT) { cc_fail(99); return 0; }
    CCT_SET(id,family,family); CCT_SET(id,width,width);
    CCT_SET(id,low,low); CCT_SET(id,high,high); CCT_SET(id,capacity,capacity);
    CCT_SET(id,element,0);
    ++cc.type_count; return id;
}
TypeId cc_family(TypeId type) FRONTEND {
    if(type>=K_SET_EXPR&&type<=K_EMPTY_SET) return K_SET;
    if(type==K_ANY_POINTER||type==K_NIL) return K_POINTER;
    return type<cc.type_count?CCT(type,family):TYPE_NONE;
}
static uint8_t text_type(TypeId type) { return cc_family(type)==K_CHAR||cc_family(type)==K_STRING; }
uint8_t cc_compatible(TypeId left,TypeId right) FRONTEND {
    if(cc_family(left)==K_FILE||cc_family(right)==K_FILE)
        return cc_family(left)==K_FILE&&cc_family(right)==K_FILE&&
            CCT(left,capacity)==CCT(right,capacity)&&
            (CCT(left,capacity)!=FILE_TYPED||CCT(left,element)==CCT(right,element));
    if(cc_family(left)==K_REAL&&cc_family(right)==K_INTEGER) return 1;
    if(cc_family(left)==K_POINTER||cc_family(right)==K_POINTER) {
        if(cc_family(left)!=K_POINTER||cc_family(right)!=K_POINTER) return 0;
        return left==K_ANY_POINTER||right==K_ANY_POINTER||left==K_NIL||right==K_NIL||
            CCT(left,element)==CCT(right,element);
    }
    if(cc_family(left)==K_SET||cc_family(right)==K_SET) {
        if(cc_family(left)!=K_SET||cc_family(right)!=K_SET) return 0;
        return cc_set_base(left)==TYPE_NONE||cc_set_base(right)==TYPE_NONE||cc_set_base(left)==cc_set_base(right);
    }
    if(cc_aggregate(left)||cc_aggregate(right))
        return left==right||((cc_char_array(left)||text_type(left))&&(cc_char_array(right)||text_type(right)));
    return cc_family(left)==cc_family(right)||(text_type(left)&&text_type(right));
}
