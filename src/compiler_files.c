#include "compiler.h"
#include "file_runtime.h"

static void index_store(uint8_t opcode,uint8_t offset) { cc_emit(0xfd); cc_emit(opcode); cc_emit(offset); }
/* This helper is shared by all file operations but retains the invocation's
 * actual source PC. No callback occurs during the checked-guard setup. */
static void guard(uint16_t bytes) {
    cc_op16(0x2a,RT_PC);
    cc_address(0x22,0x4000u+cc.emitted+8);
    cc_reserve(bytes);
}
static void engine(void) {
    uint16_t skip,asset,gate,bootstrap,ready,normal,allocated,i;
    uint16_t module_bytes=cc_file_asset_size(0),gate_bytes=cc_file_asset_size(1);
    if(cc.file_entry||cc.suppress||cc.error->code) return;
    skip=cc_forward(0xc3);
    asset=0x4000u+cc.emitted;
    for(i=0;i<module_bytes;++i) cc_emit(cc_file_asset_get(0,i));
    gate=0x4000u+cc.emitted;
    for(i=0;i<gate_bytes;++i) cc_emit(cc_file_asset_get(1,i));
    cc.file_entry=0x4000u+cc.emitted;
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
    cc_emit(0xe1); cc_emit(0xe5); cc_emit(0xaf); cc_op16(0xcd,0xfe90);
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
    cc_temporary(32); cc_op16(0x21,0); cc_emit(0x39);
    cc_op16(0x01,32); cc_emit(0xaf); cc_call(RT_FILL);
    cc_op16(0x21,operation); store(0,0);
    return before;
}
static uint8_t file_argument(uint16_t base) {
    uint8_t type=cc_reference(41);
    uint16_t skip,descriptor,width;
    cc_release(base+32);
    if(cc_family(type)!=K_FILE) { cc_fail(44); return type; }
    store(2,0);
    skip=cc_forward(0xc3); descriptor=0x4000u+cc.emitted;
    width=cc.types[type].capacity==FILE_TEXT?0:cc.types[type].capacity==FILE_UNTYPED?128:
        cc.types[cc.types[type].element].width;
    cc_emit(cc.types[type].capacity); cc_emit((uint8_t)width); cc_emit((uint8_t)(width>>8)); cc_patch(skip);
    cc_address(0x21,descriptor); store(4,0);
    return type;
}
static void invoke(uint16_t base) {
    engine();
    cc_reserve(64); cc_op16(0x21,cc.temporaries-base-32); cc_emit(0x39);
    cc_op16(0xcd,RT_IOCALL); cc_relocate(cc.emitted);
    cc_emit((uint8_t)cc.file_entry); cc_emit((uint8_t)(cc.file_entry>>8)); cc_emit(cc.io_check);
}
static void result(uint16_t base,uint8_t offset) {
    cc_op16(0x21,cc.temporaries-base-32+offset); cc_emit(0x39);
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
        cc_is("erase")?ZF_ERASE:cc_is("blockread")?ZF_BLOCKREAD:cc_is("blockwrite")?ZF_BLOCKWRITE:255;
    if(operation==255) return 0;
    cc_next(); cc_expect('(',4); base=job_start(operation); file_argument(base);
    if(operation==ZF_ASSIGN) {
        cc_expect(',',3); cc_string_value(); store(6,cc.temporaries-base-32);
    } else if(operation==ZF_SEEK) {
        cc_expect(',',3); integer(); cc_release(base+32); store(8,0);
    } else if(operation==ZF_BLOCKREAD||operation==ZF_BLOCKWRITE) {
        cc_expect(',',3); cc_reference(41); cc_release(base+32); store(6,0);
        cc_expect(',',3); integer(); cc_release(base+32); store(8,0);
        if(cc.token==',') {
            cc_next(); if(cc_reference(41)!=K_INTEGER) cc_fail(24);
            cc_release(base+32); cc_reserve(2); cc_emit(0xe5); cc.temporaries+=2;
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
    uint8_t operation;
    uint16_t base;
    if(cc.token!=T_ID||cc_lookup()>=0) return -1;
    operation=cc_is("filepos")?ZF_POSITION:cc_is("filesize")?ZF_SIZE:cc_is("eof")?ZF_EOF:255;
    if(operation==255) return -1;
    cc_next(); cc_expect('(',4); base=job_start(operation); file_argument(base);
    cc_expect(')',5); invoke(base); result(base,20); cc_release(base);
    return operation==ZF_EOF?K_BOOLEAN:K_INTEGER;
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
    uint8_t type=file_argument(base),actual,component;
    if(cc_family(type)!=K_FILE||cc.types[type].capacity!=FILE_TYPED||newline) cc_fail(66);
    component=cc.types[type].element;
    while(cc.token==','&&!cc.error->code) {
        cc_next(); actual=cc_reference(41); cc_release(base+32);
        if(actual!=component&&!
           (cc_family(actual)==K_POINTER&&cc_family(component)==K_POINTER&&cc_compatible(component,actual))&&!
           (cc_family(actual)==K_STRING&&cc_family(component)==K_STRING&&cc.types[actual].width==cc.types[component].width))
            cc_fail(44);
        store(6,0); invoke(base);
    }
    cc_expect(')',5); cc_release(base);
}
