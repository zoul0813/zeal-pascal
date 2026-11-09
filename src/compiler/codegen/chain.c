#include "compiler/groups/late.h"
#include "compiler/link/layout.h"
#include "runtime/files/operations.h"
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
#ifdef ZP_PROGRAM_PAGES
/* Page2 holds statics below the shared stack. Page3 is its high-address alias.
 * The heap/stack guards use that high alias as the physical end of statics.
 * Preserve the cold child's live stack and private controls before replacing
 * its OS-provided page3. Memory Run has already installed the shared mapping. */
#define PAGE_STARTUP_MAX_BYTES 192u
static void shared_startup(uint8_t pages) {
    uint16_t entry=cc.emitted,memory,interrupts,original=CC_CODE_BASE;
    cc_op16(0x3a,0xfe78u);             /* invocation mode initialized by runtime */
    cc_emit(0xb7);                    /* OR A */
    memory=cc_forward(0xc2);          /* Memory Run skips cold remapping */
    cc_emit(0xed);cc_emit(0x57);       /* LD A,I captures IFF2 */
    cc_emit(0xf3);cc_emit(0xf5);       /* DI; PUSH AF */
    cc_emit(0x3e);cc_emit(0xc0);
    cc_emit(0xdb);cc_emit(0xf3);       /* old physical page3 */
    cc_emit(0xf5);                    /* preserve old page until after copy */
    if(pages==1) {
        cc_emit(0x3e);cc_emit(0x80);cc_emit(0xdb);cc_emit(0xf2);cc_emit(0xf5);
    }
    cc_op16(0x21,0xfa00u);            /* source includes live stack + controls */
    cc_op16(0x11,pages==1?0x7a00u:0xba00u);
    cc_op16(0x01,0x0600u);
    cc_emit(0xed);cc_emit(0xb0);       /* LDIR */
    cc_emit(0x3e);cc_emit(pages==1?0x40:0x80);
    cc_emit(0xdb);cc_emit(pages==1?0xf1:0xf2);
    cc_emit(0xd3);cc_emit(0xf3);
    if(pages==1) {
        cc_emit(0xd3);cc_emit(0xf2);
        cc_emit(0xf1);cc_emit(0x47);cc_emit(0x2e);cc_emit(26);cc_emit(0xcf);
        cc_op16(0x32,0xfe7f);
    }
           /* stack now uses the copied alias */
    cc_emit(0xf1);cc_emit(0x47);       /* POP AF; LD B,A (owned old page) */
    cc_emit(0x2e);cc_emit(26);         /* native pfree */
    cc_emit(0xcf);
    if(pages==1) {cc_emit(0xb7);cc_emit(0x20);cc_emit(3);cc_op16(0x3a,0xfe7f);}
    cc_emit(0x4f);cc_emit(0xe1);       /* retain error; POP HL (saved IFF flags) */
    cc_emit(0xcb);cc_emit(0x55);       /* BIT 2,L */
    interrupts=cc_forward(0xca);
    cc_emit(0xfb);                    /* EI only if IFF2 was set */
    cc_patch(interrupts);
    cc_emit(0x79);cc_emit(0xb7);       /* LD A,C; OR A */
    cc_emit(0x28);cc_emit(4);          /* skip failure exit on success */
    cc_op16(0x21,0x010fu);cc_emit(0xcf); /* native nonzero process exit */
    cc_patch(memory);
    cc_op16(0x2a,RT_HEAPFLOOR);
    if(pages==1) {cc_op16(0x11,0x8000u);cc_emit(0x19);}
    else {
        cc_emit(0x7c);cc_emit(0xfe);cc_emit(0x80);cc_emit(0x30);cc_emit(3);
        cc_op16(0x21,0x8000u);cc_op16(0x11,0x4000u);cc_emit(0x19);
    }
    cc_op16(0x22,RT_HEAPFLOOR);
    cc_op16(0x22,RT_HEAPPTR);
    cc_address(0xc3,original);
    if(cc.emitted-entry>PAGE_STARTUP_MAX_BYTES)cc_fail(99);
    cc.page_entry=entry;
    cc.shared_stack=pages==1?2:1;
}
#endif
void cc_startup_finish(void) FRONTEND {
#ifdef ZP_PROGRAM_PAGES
#ifdef ZP_STATIC_LAYOUT
    uint32_t origin=cc.sources->options&&cc.sources->options->start?
        cc.sources->options->start:cc_data_base()+cc.image->globals_bytes;
    uint32_t end=cc.sources->options?cc.sources->options->end:0xfbffu;
    uint16_t emitted=cc.emitted,relocations=cc.relocations,links=cc.links;
    uint8_t pages;uint32_t extent;
    if(cc_layout_active()&&!cc.full_memory&&!cc.console_dynamic&&!cc.interrupt_entry&&end==0xfbffu) {
        for(pages=1;pages<=2;++pages) {
            shared_startup(pages);
            if(cc.error->code)break;
            extent=(origin+(cc.emitted-CC_CODE_BASE)+EXECUTE_ENTRY_BYTES+1u)&~1UL;
            if(pages==2&&extent<0x8000UL)extent=0x8000UL;
            if(extent+cc_required_stack()+1024u<=0x4000UL+(uint32_t)pages*PAGE_SIZE)break;
            cc.emitted=emitted;cc.relocations=relocations;cc.links=links;cc.shared_stack=0;cc.page_entry=0;
        }
    }
#else
    uint32_t origin=cc.sources->options&&cc.sources->options->start?
        cc.sources->options->start:GLOBAL_BASE+cc.image->globals_bytes;
    uint32_t end=cc.sources->options?cc.sources->options->end:0xfbffu;
    uint16_t code_bytes=cc.emitted-CC_CODE_BASE;
    uint32_t required=origin+code_bytes+PAGE_STARTUP_MAX_BYTES+EXECUTE_ENTRY_BYTES+0x4000UL+2048u;
    if(!cc.full_memory&&!cc.console_dynamic&&!cc.console_used&&!cc.keyboard_entry&&!cc.interrupt_entry&&!cc.random_entry&&
       !cc.file_entry&&!cc.standard_table_operand&&!cc.file_stream_operand&&!cc.parameter_entry&&!cc.transfer_entry&&
       end==0xfbffu&&required<=end+1u)shared_startup(2);
#endif
#endif
    asset(3);
}
static void patch(uint16_t at,uint16_t value) {
    cc_word(at,value);cc_relocate(at);
}
void cc_transfer_statement(uint16_t base) FRONTEND {
    cc_require_real();
#ifdef ZP_COMPACT_EXPERIMENT
    /* This proof has no compact transfer ABI. Never embed a coordinator
     * whose cleanup/profile operands belong to the normal runtime. */
    (void)base;cc_fail(COMPILE_PLATFORM_FAILURE);
#else
    uint16_t at;
    if(cc.suppress||cc.error->code)return;
    cc.console_dynamic=1;
    cc_reserve(256); /* caller job/path frame; coordinator C has a private stack */
    cc_op16(0x21,cc.temporaries-base-ZF_JOB_BYTES);cc_emit(0x39);
    cc_op16(0xcd,RT_IOCALL);at=cc.emitted;cc_relocate(at);
    cc_emit((uint8_t)cc.transfer_entry);cc_emit((uint8_t)(cc.transfer_entry>>8));cc_emit(cc.io_check);
    cc.transfer_entry=at;
#endif
}
/* Share one packed coordinator across Chain and Execute. Forward operands
 * share the compiler's existing fixup-chain and relocation mechanisms. */
void cc_transfer_finish(void) FRONTEND {
#ifdef ZP_COMPACT_EXPERIMENT
    /* Fail closed even if a deferred root is requested by another emitter. */
    if(!cc.error->code&&(cc.transfer_entry||cc_root_pending(LATE_ROOT,LATE_TRANSFER)))
        cc_fail(COMPILE_PLATFORM_FAILURE);
#else
    uint16_t module,gate,entry,i,offset,at,value,head=cc.transfer_entry;
    if(cc.error->code||(!head&&!cc_root_pending(LATE_ROOT,LATE_TRANSFER)))return;
    module=asset(0);gate=asset(1);entry=asset(2);
    for(i=0;i<TRANSFER_CALL_RELOC_COUNT;++i) {
        at=TRANSFER_CALL_RELOC_OFFSET+2*i;
        offset=(cc_transfer_asset_get(2,at)|((uint16_t)cc_transfer_asset_get(2,at+1)<<8))-CC_CODE_BASE;
        at=entry+offset;value=cc_code_get(at)|((uint16_t)cc_code_get(at+1)<<8);
        patch(at,value+entry-CC_CODE_BASE);
    }
    patch(entry+TRANSFER_CALL_MODULE_OPERAND,module);
    patch(entry+TRANSFER_CALL_GATE_OPERAND,gate);
    patch(entry+TRANSFER_CALL_FILE_OPERAND,cc.file_entry);
    cc_unpack_reference(entry+TRANSFER_CALL_UNPACK_OPERAND);
    cc_patch_chain(head,entry);cc.transfer_entry=entry;cc_root_bind(LATE_ROOT,LATE_TRANSFER,entry);
#endif
}
