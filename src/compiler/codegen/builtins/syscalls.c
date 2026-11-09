#include "compiler/link/layout.h"
#include "units/internal.h"
#include "runtime/syscalls/abi.h"
#include "zos-unit.h"
/* This parser shares the retained device-generator bank. Implicit loading
 * below also preserves any outer file-generator frames and their page tokens. */
static void store(uint16_t base,uint8_t offset) {
    cc_emit(0x44);cc_emit(0x4d);
    cc_op16(0x21,cc.temporaries-base-ZS_JOB_BYTES+offset);cc_emit(0x39);
    cc_emit(0x71);cc_emit(0x23);cc_emit(0x70);
}
static void constant(uint16_t base,uint8_t offset,uint16_t n) {cc_op16(0x21,n);store(base,offset);}
static uint16_t dispatch(void) {
    char name[12];uint16_t module,export;int16_t symbol;uint8_t keep;
    strcpy(name,"zos");
    if(!cc_units_begin())return 0;
    module=cc_units_find(name);
    if(!module){
        keep=cc.unit_load_preserve;cc.unit_load_preserve=1;
        module=cc_units_graph(name,0);cc.unit_load_preserve=keep;
    }
    if(!module)return 0;
    if((cc_unit_module_word(module,28)|((uint32_t)cc_unit_module_word(module,30)<<16))!=ZOS_CHECKSUM){cc_fail(UNIT_STALE);return 0;}
    strcpy(name,"dispatch");export=cc_unit_export(module,name);
    if(!export){cc_fail(UNIT_FORMAT);return 0;}
    {
        uint16_t at,end=cc_unit_word(UH_USED);
        for(at=UNIT_RECORD_BASE;at<end;at+=UNIT_RECORD_BYTES)
            if(cc_unit_word(at)==UR_IMPORT&&cc_unit_word(at+4)==module&&cc_unit_word(at+6)==export)
                return CCS(cc_unit_word(at+10),initial_type);
    }
    symbol=cc_unit_import_symbol(module,export);
    return symbol<0?0:CCS(symbol,initial_type);
}
static void argument(uint16_t base,uint8_t slot,char kind) {
    TypeId type,family;uint16_t size=0;
    if(kind=='s'){cc_string_value();}
    else if(kind=='b'||kind=='v'||kind=='c') {
        type=cc_expression();family=cc_family(type);
        if((family!=K_INTEGER&&family!=K_WORD)&&(kind!='c'||family!=K_CHAR))cc_fail(44);
    } else {
        type=cc_reference(41);
        if(cc.error->code)return;
        if(type==TYPE_NONE&&kind!='r'){cc_fail(44);return;}
        family=type==TYPE_NONE?TYPE_NONE:cc_family(type);size=type==TYPE_NONE?65535u:CCT(type,width);
        if(kind=='w'&&(family!=K_WORD||size!=2))cc_fail(44);
        if(kind=='l'&&(family!=K_LONGWORD||size!=4))cc_fail(44);
        if(kind=='y'&&(family!=K_INTEGER||size!=1||CCT(type,low)!=0||CCT(type,high)!=255))cc_fail(44);
        if(kind=='p') {if(family!=K_STRING)cc_fail(44);size=CCT(type,capacity);}
        if(kind=='g'&&size!=6)cc_fail(44);
        if(kind=='d'&&size<8)cc_fail(44);
        if(kind=='t'&&size<29)cc_fail(44);
        if(kind=='e'&&size<17)cc_fail(44);
    }
    store(base,4+2*slot);constant(base,14+2*slot,size);
}
int16_t cc_zos_function(void) BACKEND {
    static const char *const signatures[]={"brw","brw","sb","b","bt","st","blb","bbvw","s","s","p","s","be","s","bcb","b","bssy","bb","v","bv","bw","d","d",0,"bb","y","b"};
    uint8_t mode,i,slot;uint16_t base,call=0,scope;TypeId type;const char *signature;
    if(cc.token!=T_ID||cc_lookup()>=0)return -1;
    mode=cc_is("zos")?0:cc_is("zosbuffer")?1:cc_is("zosregisters")?2:255;
    if(mode==255)return -1;
    cc_require_full_memory();cc_require_hardware();
    cc_next();cc_expect('(',4);
    /* Load only on demand, before allocating the invocation's expression data. */
    scope=cc.suppress?0:dispatch();
    base=cc.temporaries;cc_temporary(ZS_JOB_BYTES);
    cc_op16(0x21,0);cc_emit(0x39);cc_op16(0x01,ZS_JOB_BYTES);cc_emit(0xaf);cc_call(RT_FILL);
    constant(base,2,mode);
    if(mode==2) {
        type=cc_expression();if(cc_family(type)!=K_INTEGER&&cc_family(type)!=K_WORD)cc_fail(44);
        store(base,0);signature="g";
    } else {
        cc_constant(&type,&call);
        if((cc_family(type)!=K_INTEGER&&cc_family(type)!=K_WORD)||call>=27||call==23||(mode==1&&call!=7))cc_fail(44);
        constant(base,0,call);signature=mode==1?"bbrvw":call<27?signatures[call]:0;
    }
    if(!signature)signature="";
    for(i=0;signature[i]&&!cc.error->code;++i) {
        cc_expect(',',3);slot=mode==1?(i==2?4:i==3?2:i==4?3:i):i;
        argument(base,slot,signature[i]);

    }
    cc_expect(')',5);
    cc_op16(0x2a,RT_HEAPFLOOR);store(base,26);
    cc_op16(0x2a,RT_HEAPPTR);store(base,28);
    cc_op16(0x2a,0x401c);store(base,30);
    cc_op16(0x21,0);cc_emit(0x39);store(base,32);
    cc_op16(0x2a,0x4018);store(base,34);
    cc_runtime_op(0x21,cc_runtime_private(RP_SYSCALL_SCRATCH));store(base,36);
    cc_native_reserve(2048);
    cc_op16(0x21,cc.temporaries-base-ZS_JOB_BYTES);cc_emit(0x39);
    if(!mode&&call==15) {
        /* Halt's terminal owner closes files/console and handles RAM runs. */
        cc_op16(0x11,4);cc_emit(0x19);cc_emit(0x5e);cc_emit(0x23);cc_emit(0x56);cc_emit(0xeb);
        {
            uint16_t invalid,done;
            cc_emit(0x7c);cc_emit(0xb7);invalid=cc_forward(0xc2);
            cc_runtime_op(0xcd,cc_runtime_private(RP_SYSCALL_EXIT));
            done=cc_forward(0xc3);cc_patch(invalid);constant(base,24,6);cc_patch(done);
        }
    } else if(scope)cc_units_call(0xcd,scope);
    cc_op16(0x21,cc.temporaries-base-ZS_JOB_BYTES+24);cc_emit(0x39);
    cc_emit(0x5e);cc_emit(0x23);cc_emit(0x56);cc_emit(0xeb);cc_release(base);
    return K_INTEGER;
}
