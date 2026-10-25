#include "compiler.h"

static uint8_t parse_type(void);
static uint8_t array_dimension(void) {
    uint8_t index,array,element;
    uint32_t width;
    if(++cc.depth>24) { cc_fail(98); --cc.depth; return 0; }
    index=parse_type();
    if(!cc_ordinal(index)) cc_fail(30);
    array=cc_new_type(K_ARRAY,0,0,0,index);
    if(cc.token==',') { cc_next(); element=array_dimension(); }
    else { cc_expect(']',9); cc_keyword("of",15); element=parse_type(); }
    if(cc.error->code) { --cc.depth; return array; }
    width=((uint32_t)((int32_t)cc.types[index].high-cc.types[index].low)+1u)*cc.types[element].width;
    if(width>32767u) cc_fail(98);
    cc.types[array].width=(uint16_t)width; cc.types[array].element=element;
    --cc.depth; return array;
}
static uint8_t parse_type_body(void) {
    int16_t id;
    uint16_t lo,hi,count,member;
    uint8_t base,other,result;
    if(cc_is("packed")) { cc_next(); return parse_type(); }
    if(cc_is("array")) { cc_next(); cc_expect('[',8); return array_dimension(); }
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
static uint8_t parse_type(void) {
    uint8_t type;
    if(++cc.depth>24) { cc_fail(98); --cc.depth; return 0; }
    type=parse_type_body(); --cc.depth; return type;
}
/* Validate initializers when declared; replay their immutable source to emit
 * the typed payload after declarations. No initializer pointers escape a bank. */
static void initializer(uint8_t type,uint8_t emit) {
    uint16_t value,count,i;
    uint8_t actual,element;
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
                count=cc.symbol_count-first; cc_expect(':',2); type=parse_type();
                for(i=first;i<first+count&&!cc.error->code;++i) {
                    cc.symbols[i].type=type; cc_storage(i,cc.types[type].width);
                }
            } else if(section==S_TYPE) {
                cc_expect('=',6); type=parse_type(); cc.symbols[id].type=type; cc.symbols[id].role=S_TYPE;
            } else {
                if(cc.token==':') {
                    cc_next(); type=parse_type(); cc_expect('=',6);
                    if(cc_family(type)==K_ARRAY) {
                        value=cc.token_start; initializer(type,0); initial=255;
                    } else cc_constant(&initial,&value);
                    if(cc_family(type)==K_ARRAY) {
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
                    cc_expect('=',6); cc_constant(&type,&value);
                    cc.symbols[id].type=type; cc.symbols[id].role=S_CONSTANT;
                }
                cc.symbols[id].address=value;
            }
            cc_expect(';',1);
        }
    }
}
void cc_typed_storage(uint16_t first) FRONTEND {
    uint16_t i,j,value,jump=0,saved_scan,saved_token,start,end;
    uint8_t range,absolute,strict;
    for(i=first;i<cc.symbol_count&&!cc.error->code;++i) if(cc.symbols[i].role==S_TYPED) {
        if(!jump) jump=cc_forward(0xc3);
        value=cc.symbols[i].address; cc.symbols[i].address=0x4000u+cc.emitted;
        if(cc_family(cc.symbols[i].type)==K_ARRAY) {
            saved_scan=cc.scan; saved_token=cc.token; start=cc.token_start; end=cc.token_end;
            range=cc.range_check; absolute=cc.absolute_mode; strict=cc.var_strict;
            cc.scan=value; cc_next(); initializer(cc.symbols[i].type,1);
            /* cc_next needs the token's start, not its following scan. Restore
             * by re-lexing that token so name/literal metadata are coherent. */
            cc.scan=start; cc_next();
            cc.scan=saved_scan; cc.token=saved_token; cc.token_start=start; cc.token_end=end;
            cc.range_check=range; cc.absolute_mode=absolute; cc.var_strict=strict;
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
