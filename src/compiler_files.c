#include "compiler.h"
#include "file_runtime.h"
typedef char text_kinds_match[(K_INTEGER==ZF_TEXT_INTEGER&&K_STRING==ZF_TEXT_STRING&&
    K_CHAR==ZF_TEXT_CHAR&&K_BYTE==ZF_TEXT_BYTE&&K_REAL==ZF_TEXT_REAL)?1:-1];

static void index_store(uint8_t opcode,uint8_t offset) { cc_emit(0xfd); cc_emit(opcode); cc_emit(offset); }
/* This helper is shared by all file operations but retains the invocation's
 * actual source PC. No callback occurs during the checked-guard setup. */
static void guard(uint16_t bytes) {
    cc_op16(0x2a,RT_PC);
    cc_address(0x22,cc.emitted+8);
    cc_reserve(bytes);
}
/* Transport leaves a numeric token in a caller-owned context. Convert only
 * after the original runtime prefix is restored; Val supplies the same grammar
 * and Real6 semantics as console input. Commit a destination only on success. */
static void number_result(void) {
    uint16_t skip[5],bad,copy,done,i;
    index_store(0x6e,22); index_store(0x66,23); cc_emit(0x7c); cc_emit(0xb5);
    skip[0]=cc_forward(0xc2);
    index_store(0x7e,0); cc_emit(0xfe); cc_emit(ZF_TEXT_READ); skip[1]=cc_forward(0xc2);
    index_store(0x7e,8); cc_emit(0xfe); cc_emit(ZF_TEXT_STRING); skip[2]=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(ZF_TEXT_CHAR); skip[3]=cc_forward(0xca);
    index_store(0x7e,10); cc_emit(0xb7); skip[4]=cc_forward(0xca);
    guard(262);
    index_store(0x6e,6); index_store(0x66,7);
    cc_op16(0x11,ZF_NUMBER_TOKEN); cc_emit(0x19); cc_emit(0xe5);
    cc_op16(0x11,ZF_NUMBER_VALUE-ZF_NUMBER_TOKEN); cc_emit(0x19); cc_emit(0xe5);
    cc_emit(0x2b); cc_emit(0x2b); cc_emit(0xe5);
    cc_op16(0x21,0); cc_emit(0x39);
    index_store(0x7e,8); cc_emit(0xfe); cc_emit(ZF_TEXT_REAL);
    cc_emit(0x3e); cc_emit(ZR_VAL_INTEGER); cc_emit(0x20); cc_emit(1); cc_emit(0x3c);
    cc_op16(0xcd,RT_VAL); cc_emit(0xc1); cc_emit(0xc1); cc_emit(0xc1);
    index_store(0x6e,6); index_store(0x66,7);
    cc_emit(0x23); cc_emit(0x23); cc_emit(0x7e); cc_emit(0x23); cc_emit(0xb6);
    bad=cc_forward(0xc2);
    index_store(0x6e,6); index_store(0x66,7);
    cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56);
    cc_emit(0x23); cc_emit(0x23); cc_emit(0x23); /* HL=value, DE=destination */
    index_store(0x7e,8); cc_emit(0xfe); cc_emit(ZF_TEXT_REAL);
    cc_op16(0x01,6); copy=cc_forward(0xca);
    cc_op16(0x01,2); cc_emit(0xfe); cc_emit(ZF_TEXT_BYTE);
    cc_emit(0x20); cc_emit(1); cc_emit(0x0b);
    cc_patch(copy); cc_emit(0xed); cc_emit(0xb0); done=cc_forward(0xc3);
    cc_patch(bad);
    cc_emit(0x3e); cc_emit(0x10); index_store(0x77,22);
    cc_emit(0xaf); index_store(0x77,23);
    cc_patch(done);
    for(i=0;i<5;++i) cc_patch(skip[i]);
}
/* The disk module resolves authority, then the restored prefix owns console
 * formatting/input. A nested I- landing catches prefix errors before restoring
 * the caller's With register and propagating the enclosing checking policy. */
static void stream_result(void) {
    uint16_t disk,done,operand,begin,read,write,end,string_target,char_target;
    index_store(0x7e,34); cc_emit(0xb7); disk=cc_forward(0xca);
    guard(512);
    cc_op16(0xcd,RT_IOCALL); operand=cc.emitted; cc_relocate(operand);
    cc_emit(0); cc_emit(0); cc_emit(0); /* private helper, always I- */
    cc_op16(0x3a,RT_IOSTATUS); index_store(0x77,22);
    cc_emit(0xaf); index_store(0x77,23); done=cc_forward(0xc3);
    cc_word(operand,cc.emitted);
    index_store(0x7e,0); cc_emit(0xfe); cc_emit(ZF_TEXT_BEGIN); begin=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(ZF_TEXT_READ); read=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(ZF_TEXT_WRITE); write=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(ZF_TEXT_READLN); end=cc_forward(0xca);
    cc_emit(0x3e); cc_emit(13); cc_op16(0xcd,RT_CHAR);
    cc_emit(0x3e); cc_emit(10); cc_op16(0xc3,RT_CHAR);
    cc_patch(begin); index_store(0x7e,8);
    cc_emit(0xcb); cc_emit(0x57); cc_emit(0xc0); /* ReadLn retains the current line. */
    cc_op16(0xc3,RT_INPUT_BEGIN);
    cc_patch(end); cc_op16(0xc3,RT_INPUT_END);
    cc_patch(write); index_store(0x6e,6); index_store(0x66,7);
    cc_op16(0x11,0); cc_op16(0xc3,RT_FIELD);
    cc_patch(read); index_store(0x6e,6); index_store(0x66,7);
    index_store(0x46,8); index_store(0x4e,9);
    cc_emit(0x78); cc_emit(0xfe); cc_emit(K_STRING); string_target=cc_forward(0xca);
    cc_emit(0xfe); cc_emit(K_CHAR); char_target=cc_forward(0xca);
    cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
    cc_patch(string_target); cc_patch(char_target); cc_op16(0xc3,RT_INPUT_VALUE);
    cc_patch(disk); number_result(); cc_patch(done);
}
static void engine(void) {
    uint16_t skip,asset,gate,bootstrap,ready,normal,allocated,i;
    uint16_t module_bytes=cc_file_asset_size(0),gate_bytes=cc_file_asset_size(1);
    if(cc.file_entry||cc.suppress||cc.error->code) return;
    skip=cc_forward(0xc3);
    asset=cc.emitted;
    for(i=0;i<module_bytes;++i) cc_emit(cc_file_asset_get(0,i));
    gate=cc.emitted;
    for(i=0;i<gate_bytes;++i) cc_emit(cc_file_asset_get(1,i));
    cc.file_entry=cc.emitted;
    cc_op16(0x3a,RT_IOSTATUS); cc_emit(0xb7); bootstrap=cc_forward(0xca);
    cc_op16(0xc3,RT_IOFAIL); /* pending status skips resource acquisition/I/O */
    cc_patch(bootstrap);
    cc_emit(0xfd); cc_emit(0xe5); cc_emit(0xe5); /* old IY and caller job */
    cc_op16(0x3a,0xfe74); cc_emit(0xb7); ready=cc_forward(0xc2);
    guard(1024); /* first load, before increasing the active emergency budget */
    cc_op16(0x3a,0xfe78); cc_emit(0xb7); normal=cc_forward(0xca);
    cc_emit(0xaf); cc_op16(0x11,0); cc_op16(0xcd,0xfda0); allocated=cc_forward(0xc3);
    cc_patch(normal);
    cc_emit(0x2e); cc_emit(25); cc_emit(0xcf); /* supported native palloc */
    cc_emit(0x58); cc_emit(0x16); cc_emit(0); /* DE=physical page */
    cc_patch(allocated);
    cc_emit(0xb7); allocated=cc_forward(0xca);
    cc_emit(0xe1); cc_emit(0xfd); cc_emit(0xe1);
    cc_op16(0x21,RT_PC); cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
    cc_op16(0xc3,RT_STACK);
    cc_patch(allocated);
    cc_emit(0x3e); cc_emit(0x40); cc_emit(0xdb); cc_emit(0xf1); cc_op16(0x32,0xfe76);
    cc_emit(0x7b); cc_op16(0x32,0xfe74); cc_emit(0xd3); cc_emit(0xf1);
    cc_op16(0x21,0x4000); cc_op16(0x11,0x4001); cc_op16(0x01,0x3fff);
    cc_emit(0x36); cc_emit(0); cc_emit(0xed); cc_emit(0xb0);
    cc_address(0x21,asset); cc_op16(0x11,0x4000); cc_op16(0x01,module_bytes);
    cc_emit(0xed); cc_emit(0xb0);
    cc_op16(0x3a,0xfe76); cc_emit(0xd3); cc_emit(0xf1);
    cc_address(0x21,gate); cc_op16(0x11,0xfe90); cc_op16(0x01,gate_bytes);
    cc_emit(0xed); cc_emit(0xb0);
    cc_op16(0x21,1024); cc_op16(0x22,0xfe7c);
    cc_patch(ready);
    guard(64);
    cc_emit(0xe1); cc_emit(0xe5); cc_emit(0xe5); cc_emit(0xfd); cc_emit(0xe1); /* IY=job */
    cc_emit(0xdd); cc_emit(0xe5); cc_emit(0xe1);
    index_store(0x75,12); index_store(0x74,13);
    cc_op16(0x2a,RT_HEAPFLOOR); index_store(0x75,14); index_store(0x74,15);
    cc_op16(0x2a,RT_HEAPPTR); index_store(0x75,16); index_store(0x74,17);
    cc_op16(0x2a,0x401c); index_store(0x75,18); index_store(0x74,19);
    cc_op16(0x2a,RT_PC); index_store(0x75,26); index_store(0x74,27);
    cc_op16(0x21,0); cc_emit(0x39); index_store(0x75,28); index_store(0x74,29);
    cc_op16(0x2a,0x4014); index_store(0x75,30); index_store(0x74,31);
    cc.standard_table_operand=cc.emitted+1; cc_address(0x21,0);
    index_store(0x75,32); index_store(0x74,33);
    cc_emit(0xe1); cc_emit(0xe5); cc_emit(0xaf); cc_op16(0xcd,0xfe90);
    stream_result();
    index_store(0x6e,24); index_store(0x66,25); cc_op16(0x22,0xfe7a);
    index_store(0x5e,22); index_store(0x56,23);
    cc_emit(0xe1); cc_emit(0xfd); cc_emit(0xe1); /* restore With before any nonlocal error */
    cc_emit(0x7a); cc_emit(0xb7); ready=cc_forward(0xca);
    cc_emit(0x7b); cc_emit(0x06); cc_emit(2); cc_op16(0xc3,RT_ERROR);
    cc_patch(ready); cc_emit(0x7b); cc_emit(0xb7); cc_emit(0xc8); cc_op16(0xc3,RT_IOFAIL);
    cc_patch(skip);
}
static void store(uint8_t offset,uint16_t extra) {
    cc_emit(0x44); cc_emit(0x4d); cc_op16(0x21,offset+extra); cc_emit(0x39);
    cc_emit(0x71); cc_emit(0x23); cc_emit(0x70);
}
static uint16_t job_start(uint8_t operation) {
    uint16_t before=cc.temporaries;
    cc_temporary(ZF_JOB_BYTES); cc_op16(0x21,0); cc_emit(0x39);
    cc_op16(0x01,ZF_JOB_BYTES); cc_emit(0xaf); cc_call(RT_FILL);
    cc_op16(0x21,operation); store(0,0);
    return before;
}
static uint8_t file_argument(uint16_t base) {
    uint8_t type=cc_reference(41);
    uint16_t skip,descriptor,width;
    cc_release(base+ZF_JOB_BYTES);
    if(cc_family(type)!=K_FILE) { cc_fail(44); return type; }
    store(2,0);
    skip=cc_forward(0xc3); descriptor=cc.emitted;
    width=cc.types[type].capacity==FILE_TEXT?0:cc.types[type].capacity==FILE_UNTYPED?128:
        cc.types[cc.types[type].element].width;
    cc_emit(cc.types[type].capacity); cc_emit((uint8_t)width); cc_emit((uint8_t)(width>>8)); cc_patch(skip);
    cc_address(0x21,descriptor); store(4,0);
    return type;
}
static void invoke(uint16_t base) {
    engine();
    cc_reserve(64); cc_op16(0x21,cc.temporaries-base-ZF_JOB_BYTES); cc_emit(0x39);
    cc_op16(0xcd,RT_IOCALL); cc_relocate(cc.emitted);
    cc_emit((uint8_t)cc.file_entry); cc_emit((uint8_t)(cc.file_entry>>8)); cc_emit(cc.io_check);
}
static void result(uint16_t base,uint8_t offset) {
    cc_op16(0x21,cc.temporaries-base-ZF_JOB_BYTES+offset); cc_emit(0x39);
    cc_emit(0x5e); cc_emit(0x23); cc_emit(0x56); cc_emit(0xeb);
}
static void integer(void) {
    if(cc_family(cc_expression())!=K_INTEGER) cc_fail(23);
}
uint8_t cc_file_statement(void) FRONTEND {
    uint8_t operation,actual=0;
    uint16_t base;
    if(cc.token!=T_ID||cc_lookup()>=0) return 0;
    operation=cc_is("assign")?ZF_ASSIGN:cc_is("reset")?ZF_RESET:cc_is("rewrite")?ZF_REWRITE:
        cc_is("close")?ZF_CLOSE:cc_is("seek")?ZF_SEEK:cc_is("flush")?ZF_FLUSH:
        cc_is("erase")?ZF_ERASE:cc_is("rename")?ZF_RENAME:
        cc_is("blockread")?ZF_BLOCKREAD:cc_is("blockwrite")?ZF_BLOCKWRITE:255;
    if(operation==255) return 0;
    cc_next(); cc_expect('(',4); base=job_start(operation); file_argument(base);
    if(operation==ZF_ASSIGN||operation==ZF_RENAME) {
        cc_expect(',',3); cc_string_value(); store(6,cc.temporaries-base-ZF_JOB_BYTES);
    } else if(operation==ZF_SEEK) {
        cc_expect(',',3); integer(); cc_release(base+ZF_JOB_BYTES); store(8,0);
    } else if(operation==ZF_BLOCKREAD||operation==ZF_BLOCKWRITE) {
        cc_expect(',',3); cc_reference(41); cc_release(base+ZF_JOB_BYTES); store(6,0);
        cc_expect(',',3); integer(); cc_release(base+ZF_JOB_BYTES); store(8,0);
        if(cc.token==',') {
            cc_next(); if(cc_reference(41)!=K_INTEGER) cc_fail(24);
            cc_release(base+ZF_JOB_BYTES); cc_reserve(2); cc_emit(0xe5); cc.temporaries+=2;
            actual=1; cc_op16(0x21,operation+1); store(0,2);
        }
    }
    cc_expect(')',5); invoke(base);
    if(actual) {
        result(base,10); cc_emit(0xeb); cc_emit(0xe1); cc.temporaries-=2;
        cc_emit(0x73); cc_emit(0x23); cc_emit(0x72);
    }
    cc_release(base); return 1;
}
int16_t cc_file_function(void) FRONTEND {
    uint8_t operation,type;
    uint16_t base;
    if(cc.token!=T_ID||cc_lookup()>=0) return -1;
    operation=cc_is("filepos")?ZF_POSITION:cc_is("filesize")?ZF_SIZE:cc_is("eof")?ZF_EOF:
        cc_is("eoln")?ZF_EOLN:cc_is("seekeof")?ZF_SEEKEOF:cc_is("seekeoln")?ZF_SEEKEOLN:255;
    if(operation==255) return -1;
    cc_next(); cc_expect('(',4); base=job_start(operation);
    type=file_argument(base);
    if(cc_family(type)==K_FILE&&cc.types[type].capacity!=FILE_TEXT&&operation>=ZF_EOLN) cc_fail(66);
    cc_expect(')',5); invoke(base); result(base,20); cc_release(base);
    return operation==ZF_POSITION||operation==ZF_SIZE?K_INTEGER:K_BOOLEAN;
}
/* Selection-only lookahead does not evaluate an index twice at runtime. The
 * shared suppressed parser validates its type; re-lexing restores literal/name
 * state before the ordinary expression or file-argument parse. */
uint8_t cc_file_prefix(void) FRONTEND {
    uint16_t start=cc.token_start,before=cc.temporaries;
    uint8_t type,suppress=cc.suppress;
    int16_t id=cc.token==T_ID?cc_lookup():-1;
    if(!cc_variable(id)||cc.symbols[id].role==S_RESULT) return 0;
    ++cc.suppress; type=cc_selection((uint16_t)id);
    cc.suppress=suppress; cc.temporaries=before; cc.scan=start; cc_next();
    return cc_family(type)==K_FILE;
}
void cc_file_transfer(uint8_t writing,uint8_t newline) FRONTEND {
    uint16_t base=job_start(writing?ZF_WRITE:ZF_READ);
    uint8_t type=file_argument(base),actual,component,kind;
    if(cc_family(type)!=K_FILE) { cc_fail(66); return; }
    if(cc.types[type].capacity==FILE_TEXT) {
        if(!writing) {
            uint8_t checking=cc.io_check;
            cc_op16(0x21,ZF_TEXT_BEGIN); store(0,0);
            cc_op16(0x21,newline?4:cc.token==')'?2:0); store(8,0);
            /* Preparation cannot raise a pending I/O error before destination
             * expressions run. The actual field/ReadLn owns checked delivery. */
            if(cc.token==','||newline) cc.io_check=0;
            invoke(base); cc.io_check=checking;
        }
        while(cc.token==','&&!cc.error->code) {
            cc_next();
            cc_op16(0x21,writing?ZF_TEXT_WRITE:ZF_TEXT_READ); store(0,0);
            if(writing) {
                cc_formatted_value(); store(6,cc.temporaries-base-ZF_JOB_BYTES);
            } else {
                actual=cc_reference(41); kind=cc_family(actual); cc_release(base+ZF_JOB_BYTES);
                if(kind!=K_STRING&&kind!=K_CHAR&&kind!=K_INTEGER&&kind!=K_REAL) { cc_fail(66); break; }
                if(kind==K_INTEGER&&cc.types[actual].width==1) kind=K_BYTE;
                if(kind==K_STRING||kind==K_CHAR) store(6,0);
                else {
                    cc_temporary(ZF_NUMBER_BYTES);
                    cc_emit(0xeb); cc_emit(0x73); cc_emit(0x23); cc_emit(0x72); /* captured target */
                    cc_op16(0x21,0); cc_emit(0x39); store(6,ZF_NUMBER_BYTES);
                }
                cc_op16(0x21,kind|((uint16_t)(kind==K_STRING?cc.types[actual].capacity:0)<<8));
                store(8,cc.temporaries-base-ZF_JOB_BYTES);
            }
            invoke(base); cc_release(base+ZF_JOB_BYTES);
        }
        cc_expect(')',5);
        if(newline) {
            cc_op16(0x21,writing?ZF_TEXT_WRITELN:ZF_TEXT_READLN); store(0,0); invoke(base);
        }
        cc_release(base); return;
    }
    if(cc.types[type].capacity!=FILE_TYPED||newline) cc_fail(66);
    component=cc.types[type].element;
    while(cc.token==','&&!cc.error->code) {
        cc_next(); actual=cc_reference(41); cc_release(base+ZF_JOB_BYTES);
        if(actual!=component&&!
           (cc_family(actual)==K_POINTER&&cc_family(component)==K_POINTER&&cc_compatible(component,actual))&&!
           (cc_family(actual)==K_STRING&&cc_family(component)==K_STRING&&cc.types[actual].width==cc.types[component].width))
            cc_fail(44);
        store(6,0); invoke(base);
    }
    cc_expect(')',5); cc_release(base);
}
void cc_file_finish(void) FRONTEND {
    uint16_t i,count=0;
    if(!cc.file_entry||cc.error->code) return;
    cc_word(cc.standard_table_operand,cc.emitted);
    for(i=0;i<cc.symbol_count;++i)
        if(cc.symbols[i].role==S_STANDARD&&cc.symbols[i].address) ++count;
    cc_emit((uint8_t)count); cc_emit(0);
    for(i=0;i<cc.symbol_count;++i) if(cc.symbols[i].role==S_STANDARD&&cc.symbols[i].address) {
        cc_emit((uint8_t)cc.symbols[i].address); cc_emit((uint8_t)(cc.symbols[i].address>>8));
        cc_emit(cc.symbols[i].initial_type); cc_emit(0);
    }
}
