#include "compiler.h"

static uint8_t parse_type(uint8_t allow_forward);
static uint16_t metadata(uint8_t role,uint8_t owner) {
    uint16_t id=cc.symbol_count;
    if(id==SYMBOL_LIMIT) { cc_fail(98); return 0; }
    memset(&cc.symbols[id],0,sizeof(Symbol));
    cc.symbols[id].role=role; cc.symbols[id].owner=owner; ++cc.symbol_count;
    return id;
}
static uint16_t new_field(uint8_t type,uint8_t root) {
    uint16_t id;
    if(cc.token!=T_ID) { cc_fail(53); return 0; }
    if(cc_field_lookup(root)>=0) { cc_fail(43); return 0; }
    id=metadata(S_FIELD,type);
    cc.symbols[id].name_start=cc.token_start; cc.symbols[id].name_length=(uint8_t)strlen(cc.name);
    cc.symbols[id].type=255;
    return id;
}
static void extend_record(uint8_t type,uint16_t bytes) {
    if((uint32_t)cc.types[type].width+bytes>32767u) cc_fail(98);
    else cc.types[type].width+=bytes;
}
static void field_list(uint8_t type,uint8_t root) {
    uint16_t first,last,i,marker,tag=0xffffu,label,value,saved_start;
    uint16_t largest=0;
    uint8_t field_type,actual,child;
    int16_t named;
    if(++cc.depth>24) { cc_fail(98); --cc.depth; return; }
    while(cc.token==T_ID&&!cc.error->code) {
        first=cc.symbol_count; new_field(type,root); cc_next();
        while(cc.token==','&&!cc.error->code) { cc_next(); new_field(type,root); cc_next(); }
        last=cc.symbol_count; cc_expect(':',2); field_type=parse_type(0);
        for(i=first;i<last&&!cc.error->code;++i) {
            cc.symbols[i].type=field_type; cc.symbols[i].address=cc.types[type].width;
            extend_record(type,cc.types[field_type].width);
        }
        if(cc.token!=';') { if(cc.token==T_ID||cc_is("case")) cc_fail(1); break; }
        cc_next();
    }
    if(cc_is("case")&&!cc.error->code) {
        cc_next(); named=cc_named_type(); saved_start=cc.token_start; cc_next();
        if(cc.token==':') {
            cc.scan=saved_start; cc_next(); tag=new_field(type,root); cc_next(); cc_expect(':',2);
            named=cc_named_type(); cc_next();
        }
        if(named<0) cc_fail(tag==0xffffu?2:41);
        else if(!cc_ordinal((uint8_t)named)) cc_fail(30);
        field_type=named<0?K_INTEGER:(uint8_t)named;
        if(tag!=0xffffu) {
            cc.symbols[tag].type=field_type; cc.symbols[tag].address=cc.types[type].width;
            extend_record(type,cc.types[field_type].width);
        }
        marker=metadata(S_VARIANT,type); cc.symbols[marker].type=field_type;
        cc.symbols[marker].address=cc.types[type].width; cc.symbols[marker].name_start=tag;
        cc.types[type].low=(int16_t)marker; cc_keyword("of",15);
        while(!cc.error->code&&cc.token!=')'&&!cc_is("end")&&cc.token!=T_EOF) {
            child=cc_new_type(K_RECORD,0,-1,0,0);
            do {
                cc_constant(&actual,&value);
                if(cc_family(actual)!=cc_family(field_type)) cc_fail(44);
                label=metadata(S_VLABEL,type); cc.symbols[label].type=child; cc.symbols[label].address=value;
                if(cc.token!=',') break; cc_next();
            } while(!cc.error->code);
            cc_expect(':',2); cc_expect('(',4); field_list(child,root); cc_expect(')',5);
            if(cc.types[child].width>largest) largest=cc.types[child].width;
            if(cc.token!=';') break; cc_next();
        }
        extend_record(type,largest);
    }
    --cc.depth;
}
static uint8_t array_dimension(void) {
    uint8_t index,array,element;
    uint32_t width;
    if(++cc.depth>24) { cc_fail(98); --cc.depth; return 0; }
    index=parse_type(0);
    if(!cc_ordinal(index)) cc_fail(30);
    array=cc_new_type(K_ARRAY,0,0,0,index);
    if(cc.token==',') { cc_next(); element=array_dimension(); }
    else { cc_expect(']',9); cc_keyword("of",15); element=parse_type(0); }
    if(cc.error->code) { --cc.depth; return array; }
    width=((uint32_t)((int32_t)cc.types[index].high-cc.types[index].low)+1u)*cc.types[element].width;
    if(width>32767u) cc_fail(98);
    cc.types[array].width=(uint16_t)width; cc.types[array].element=element;
    --cc.depth; return array;
}
static uint8_t parse_type_body(uint8_t allow_forward) {
    int16_t id;
    uint16_t lo,hi,count,member;
    uint8_t base,other,result;
    if(cc.token=='^') {
        cc_next();
        if(cc.token!=T_ID) { cc_fail(42); return K_INTEGER; }
        id=allow_forward?-1:cc_named_type();
        if(!allow_forward&&id<0) { cc_fail(42); return K_INTEGER; }
        result=cc_new_type(K_POINTER,2,(int16_t)cc.token_start,allow_forward,cc.scope);
        cc.types[result].element=allow_forward?255:(uint8_t)id; cc_next(); return result;
    }
    if(cc_is("packed")) { cc_next(); return parse_type(allow_forward); }
    if(cc_is("set")) {
        cc_next(); cc_keyword("of",15); base=parse_type(0);
        if(!cc_ordinal(base)||cc.types[base].low<0||cc.types[base].high>255) { cc_fail(70); return 0; }
        result=cc_new_type(K_SET,(cc.types[base].high>>3)-(cc.types[base].low>>3)+1,0,0,0);
        cc.types[result].element=base; return result;
    }
    if(cc_is("array")) { cc_next(); cc_expect('[',8); return array_dimension(); }
    if(cc_is("record")) {
        result=cc_new_type(K_RECORD,0,-1,0,0); cc_next(); field_list(result,result);
        cc_keyword("end",14); return result;
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
            cc.symbols[member].type=result; cc.symbols[member].address=count++;
            cc_next(); if(cc.token!=',') break; cc_next();
        } while(!cc.error->code);
        cc_expect(')',5);
        cc.types[result].high=(int16_t)(count-1); cc.types[result].width=count<=256?1:2;
        return result;
    }
    id=cc.token==T_ID?cc_lookup():-1;
    if(id>=0&&cc.symbols[id].role==S_TYPE) { result=cc.symbols[id].type; cc_next(); return result; }
    if(id<0&&(id=cc_predefined_type())>=0) { cc_next(); return (uint8_t)id; }
    cc_constant(&base,&lo);
    if(cc_family(base)==K_STRING) cc_fail(51);
    cc_expect(T_RANGE,41); cc_constant(&other,&hi);
    if(cc_family(base)!=cc_family(other)) cc_fail(51);
    if((int16_t)lo>(int16_t)hi) cc_fail(52);
    return cc_new_type(cc_family(base),cc_family(base)==K_INTEGER?(lo<=255&&hi<=255?1:2):cc.types[base].width,
                    (int16_t)lo,(int16_t)hi,0);
}
static uint8_t parse_type(uint8_t allow_forward) {
    uint8_t type;
    if(++cc.depth>24) { cc_fail(98); --cc.depth; return 0; }
    type=parse_type_body(allow_forward); --cc.depth; return type;
}
/* Validate initializers when declared; replay their immutable source to emit
 * the typed payload after declarations. No initializer pointers escape a bank. */
static void initializer(uint8_t type,uint8_t emit);
static void set_initializer(uint8_t type,uint8_t emit) {
    uint8_t bits[32],actual,base=cc.types[type].element;
    uint16_t low,high,i;
    memset(bits,0,sizeof(bits)); cc_expect('[',8);
    while(cc.token!=']'&&!cc.error->code) {
        cc_constant(&actual,&low);
        if(cc_family(actual)!=cc_family(base)) cc_fail(44);
        if(cc.token==T_RANGE) {
            cc_next(); cc_constant(&actual,&high);
            if(cc_family(actual)!=cc_family(base)) cc_fail(44);
        } else high=low;
        if((int16_t)low<cc.types[base].low||(int16_t)low>cc.types[base].high||
           (int16_t)high<cc.types[base].low||(int16_t)high>cc.types[base].high) cc_fail(45);
        for(i=low;i<=high&&!cc.error->code;++i) bits[i>>3]|=(uint8_t)(1u<<(i&7));
        if(cc.token!=',') break; cc_next();
    }
    cc_expect(']',9);
    if(emit) for(i=0;i<cc.types[type].width;++i) cc_emit(bits[(cc.types[base].low>>3)+i]);
}
static void record_initializer(uint8_t type,uint8_t emit) {
    uint16_t i,bytes=0;
    int16_t field;
    uint8_t used=0,child=255;
    for(i=0;i<cc.symbol_count&&!cc.error->code;++i) if(cc.symbols[i].role==S_FIELD&&cc.symbols[i].owner==type) {
        if(used&&cc.token==')') break;
        if(used) cc_expect(';',1);
        field=cc.token==T_ID?cc_field_lookup(type):-1;
        if(field<0) cc_fail(41); else if(field!=(int16_t)i) cc_fail(69);
        cc_next(); cc_expect(':',2); initializer(cc.symbols[i].type,emit);
        bytes+=cc.types[cc.symbols[i].type].width; used=1;
    }
    if(cc.types[type].low>=0) {
        if(used&&cc.token==';') cc_next();
        if(cc.token==T_ID) {
            for(i=0;i<cc.symbol_count;++i) if(cc.symbols[i].role==S_VLABEL&&cc.symbols[i].owner==type&&
                cc_field_lookup(cc.symbols[i].type)>=0) { child=cc.symbols[i].type; break; }
            if(child==255) cc_fail(41);
            else { record_initializer(child,emit); bytes+=cc.types[child].width; }
        }
    }
    if(emit) while(bytes<cc.types[type].width&&!cc.error->code) { cc_emit(0); ++bytes; }
}
static void initializer(uint8_t type,uint8_t emit) {
    uint16_t value,count,i;
    uint8_t actual,element;
    if(cc_family(type)==K_SET) { set_initializer(type,emit); return; }
    if(cc_family(type)==K_RECORD) {
        cc_expect('(',4);
        if(!cc.types[type].width) cc_fail(41);
        record_initializer(type,emit); cc_expect(')',5); return;
    }
    if(cc_family(type)==K_ARRAY) {
        element=cc.types[type].element;
        count=(uint16_t)((int32_t)cc.types[cc.types[type].capacity].high-
                        cc.types[cc.types[type].capacity].low+1);
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
    if(cc_family(type)==K_POINTER) { cc_fail(61); return; }
    cc_constant(&actual,&value);
    if(!cc_compatible(type,actual)) cc_fail(44);
    if(cc_family(type)==K_STRING) {
        if(cc_family(actual)==K_CHAR) { cc.literal[0]=(uint8_t)value; cc.literal_length=1; }
        else cc_constant_literal(value);
        if(cc.literal_length>cc.types[type].capacity) cc.literal_length=cc.types[type].capacity;
        if(emit) {
            cc_emit((uint8_t)cc.literal_length);
            for(i=0;i<cc.types[type].capacity;++i) cc_emit(i<cc.literal_length?cc.literal[i]:0);
        }
    } else {
        if(cc_family(type)==K_CHAR&&cc_family(actual)==K_STRING) {
            cc_constant_literal(value);
            if(cc.literal_length!=1) cc_fail(50);
            value=cc.literal[0];
        }
        if((int16_t)value<cc.types[type].low||(int16_t)value>cc.types[type].high) cc_fail(44);
        if(emit) { cc_emit((uint8_t)value); if(cc.types[type].width==2) cc_emit((uint8_t)(value>>8)); }
    }
}
static void resolve_pointers(void) {
    uint8_t i;
    uint16_t start=cc.token_start;
    int16_t target;
    for(i=5;i<cc.type_count&&!cc.error->code;++i)
        if(cc.types[i].family==K_POINTER&&cc.types[i].high&&cc.types[i].capacity==cc.scope) {
            cc.scan=(uint16_t)cc.types[i].low; cc_next(); target=cc_named_type();
            if(target<0) { cc_fail(42); return; }
            cc.types[i].element=(uint8_t)target; cc.types[i].high=0;
        }
    cc.scan=start; cc_next();
}
void cc_declarations(void) FRONTEND {
    uint16_t id,first,count,i,value;
    uint8_t section,type,initial;
    while((cc_is("var")||cc_is("const")||cc_is("type"))&&!cc.error->code) {
        section=cc_is("var")?S_VARIABLE:cc_is("const")?S_CONSTANT:S_TYPE; cc_next();
        while(cc.token==T_ID&&!cc.error->code) {
            first=cc.symbol_count;
            id=cc_new_symbol(section==S_VARIABLE?S_VARIABLE:S_PENDING); cc_next();
            if(section==S_VARIABLE) {
                while(cc.token==','&&!cc.error->code) { cc_next(); cc_new_symbol(S_VARIABLE); cc_next(); }
                count=cc.symbol_count-first; cc_expect(':',2); type=parse_type(0);
                for(i=first;i<first+count&&!cc.error->code;++i) {
                    cc.symbols[i].type=type; cc_storage(i,cc.types[type].width);
                }
            } else if(section==S_TYPE) {
                cc_expect('=',6); type=parse_type(1); cc.symbols[id].type=type; cc.symbols[id].role=S_TYPE;
            } else {
                if(cc.token==':') {
                    cc_next(); type=parse_type(0);
                    if(cc_family(type)==K_POINTER) cc_fail(61);
                    cc_expect('=',6);
                    if(cc_aggregate(type)||cc_family(type)==K_SET) {
                        value=cc.token_start; initializer(type,0); initial=255;
                    } else cc_constant(&initial,&value);
                    if(cc_aggregate(type)||cc_family(type)==K_SET) {
                        cc.symbols[id].type=type; cc.symbols[id].role=S_TYPED;
                        cc.symbols[id].address=value; cc_expect(';',1); continue;
                    }
                    if(!cc_compatible(type,initial)) cc_fail(44);
                    if(cc_family(type)==K_CHAR&&cc_family(initial)==K_STRING) {
                        cc_constant_literal(value);
                        if(cc.literal_length!=1) cc_fail(50);
                        value=cc.literal[0]; initial=K_CHAR;
                    }
                    if(cc_family(type)!=K_STRING&&((int16_t)value<cc.types[type].low||(int16_t)value>cc.types[type].high)) cc_fail(44);
                    cc.symbols[id].type=type; cc.symbols[id].initial_type=initial; cc.symbols[id].role=S_TYPED;
                } else {
                    cc_expect('=',6);
                    if(cc.token=='['||cc_is("nil")) cc_fail(1);
                    cc_constant(&type,&value);
                    cc.symbols[id].type=type; cc.symbols[id].role=S_CONSTANT;
                }
                cc.symbols[id].address=value;
            }
            cc_expect(';',1);
        }
        if(!cc.error->code) resolve_pointers();
    }
}
void cc_typed_storage(uint16_t first) FRONTEND {
    uint16_t i,j,value,jump=0,saved_scan,saved_token,start,end;
    uint8_t range,absolute,strict,limit;
    for(i=first;i<cc.symbol_count&&!cc.error->code;++i) if(cc.symbols[i].role==S_TYPED) {
        if(!jump) jump=cc_forward(0xc3);
        value=cc.symbols[i].address; cc.symbols[i].address=0x4000u+cc.emitted;
        if(cc_aggregate(cc.symbols[i].type)||cc_family(cc.symbols[i].type)==K_SET) {
            saved_scan=cc.scan; saved_token=cc.token; start=cc.token_start; end=cc.token_end;
            range=cc.range_check; absolute=cc.absolute_mode; strict=cc.var_strict;
            limit=cc.with_limit;
            cc.scan=value; cc_next(); initializer(cc.symbols[i].type,1);
            /* cc_next needs the token's start, not its following scan. Restore
             * by re-lexing that token so name/literal metadata are coherent. */
            cc.scan=start; cc_next();
            cc.scan=saved_scan; cc.token=saved_token; cc.token_start=start; cc.token_end=end;
            cc.range_check=range; cc.absolute_mode=absolute; cc.var_strict=strict;
            cc.with_limit=limit;
        } else if(cc_family(cc.symbols[i].type)==K_STRING) {
            if(cc_family(cc.symbols[i].initial_type)==K_CHAR) { cc.literal[0]=(uint8_t)value; cc.literal_length=1; }
            else cc_constant_literal(value);
            if(cc.literal_length>cc.types[cc.symbols[i].type].capacity) cc.literal_length=cc.types[cc.symbols[i].type].capacity;
            cc_emit((uint8_t)cc.literal_length);
            for(j=0;j<cc.types[cc.symbols[i].type].capacity;++j) cc_emit(j<cc.literal_length?cc.literal[j]:0);
        } else {
            cc_emit((uint8_t)value); if(cc.types[cc.symbols[i].type].width==2) cc_emit((uint8_t)(value>>8));
        }
    }
    if(jump) cc_patch(jump);
}
