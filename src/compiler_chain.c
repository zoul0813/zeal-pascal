#include "compiler.h"
#include "file_runtime.h"
#include "transfer_asset.h"

uint16_t cc_transfer_asset_size(uint8_t kind) FRONTEND {
    return kind==0?TRANSFER_MODULE_BYTES:kind==1?TRANSFER_GATE_BYTES:
        kind==2?TRANSFER_CALL_BYTES:EXECUTE_ENTRY_BYTES;
}
static uint16_t asset(uint8_t kind) {
    uint16_t at=cc.emitted,i,n=cc_transfer_asset_size(kind);
    for(i=0;i<n;++i)cc_emit(cc_transfer_asset_get(kind,i));
    return at;
}
/* The template contains only relative branches and fixed native ABI addresses.
 * Appending it leaves all Pascal/source-map entries and the root entry intact. */
void cc_startup_finish(void) FRONTEND {asset(3);}
static void patch(uint16_t at,uint16_t value) {
    cc_word(at,value);cc_relocate(at);
}
void cc_chain_statement(uint16_t base) FRONTEND {
    uint16_t skip,module,gate,entry,i,offset,at,value;
    if(cc.suppress||cc.error->code)return;
    if(!cc.chain_entry) {
        skip=cc_forward(0xc3);
        module=asset(0);gate=asset(1);entry=asset(2);cc.chain_entry=entry;
        for(i=0;i<TRANSFER_CALL_RELOC_COUNT;++i) {
            at=TRANSFER_CALL_RELOC_OFFSET+2*i;
            offset=(cc_transfer_asset_get(2,at)|((uint16_t)cc_transfer_asset_get(2,at+1)<<8))-CC_CODE_BASE;
            at=entry+offset;value=cc_code_get(at)|((uint16_t)cc_code_get(at+1)<<8);
            patch(at,value+entry-CC_CODE_BASE);
        }
        patch(entry+TRANSFER_CALL_MODULE_OPERAND,module);
        patch(entry+TRANSFER_CALL_GATE_OPERAND,gate);
        patch(entry+TRANSFER_CALL_FILE_OPERAND,cc.file_entry);
        cc_patch(skip);
    }
    cc_reserve(256);
    cc_op16(0x21,cc.temporaries-base-ZF_JOB_BYTES);cc_emit(0x39);
    cc_op16(0xcd,RT_IOCALL);cc_relocate(cc.emitted);
    cc_emit((uint8_t)cc.chain_entry);cc_emit((uint8_t)(cc.chain_entry>>8));cc_emit(cc.io_check);
}
