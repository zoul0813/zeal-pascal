#ifdef __SDCC
#define compile_project compiler_compile_project
#endif
#include "compiler/internal.h"
#include "compiler/disk.h"
#include "compiler/groups/native.h"
#include "compiler/groups/late.h"
#ifdef ZP_UNITS_EXPERIMENT
#include "units/internal.h"
#include "platform/banks/loader.h"
#endif
#if defined(__SDCC) || defined(ZP_COMPILER_JOBS)
#include "compiler/jobs/job.h"
#endif

/* Compilation owns transient sources, staging, relocation and sealing until
 * this phase returns. Run may then release its immutable code bank. */
static uint16_t pc(void) {return cc.emitted-CC_CODE_BASE;}
static uint8_t disk_prefix(uint8_t chain) {
    uint8_t bytes[256];uint16_t at,n;
    /* Disk flushes own console_scratch. Keep pending template bytes on the
     * retained stack until every byte in this block has been emitted. */
    for(at=0;at<PAGE_SIZE;at+=256) {
        if(!cc_poll())return 0;
        memset(bytes,0,sizeof(bytes));
        if(!chain&&at<cc_runtime_size()) {
            n=cc_runtime_size()-at;if(n>sizeof(bytes))n=sizeof(bytes);
            if(!cc_runtime_read(at,bytes,n)){cc_fail(99);return 0;}
        }
        for(n=0;n<sizeof(bytes);++n)cc_put(at+n,bytes[n]);
        if(cc.error->code)return 0;
    }
    return 1;
}

static uint8_t release_stages(void) {
    if(cc.stage_page!=255) {
        if(!page_drop(cc.stage_page)){cc_fail(99);return 0;}cc.stage_page=255;
    }
    if(cc.primary_stage!=255) {
        if(!page_drop(cc.primary_stage)){cc_fail(99);return 0;}cc.primary_stage=255;
    }
    return 1;
}

#ifdef ZP_OVERLAYS
static uint8_t want_overlays;
static uint8_t compile_once(const Source *s,Build *b,Diagnostic *d,uint8_t interruptible,const CompileOptions *options) {
#else
uint8_t compile_project(const Source *s,Build *b,Diagnostic *d,uint8_t interruptible,const CompileOptions *options) {
#endif
    uint16_t i,j,n;uint8_t native_error,chain=options&&options->kind==COMPILE_CHAIN;
    SourcePosition begin;
    Source active;
    CompilerSources sources;
    memset(&cc,0,sizeof(cc));memset(&sources,0,sizeof(sources));
    sources.root=s;sources.options=options;
    sources.memory=!options;
#if defined(__SDCC) || defined(ZP_COMPILER_JOBS)
    if(compiler_invocation)sources.memory=compiler_memory_mode;
#endif
    cc.input=&active;cc.sources=&sources;cc.image=b;cc.error=d;
#ifdef ZP_UNITS_EXPERIMENT
    sources.unit_page=255;sources.unit_mode=cc_units_requested();
#endif
    cc.stage_page=255;cc.primary_stage=255;
    memset(d,0,sizeof(*d));memset(b,0,sizeof(*b));
    memset(b->pages,255,3);b->map_page=255;b->sources.page=255;
    cc.polling=interruptible;
#ifdef ZP_COMPACT_EXPERIMENT
    /* A CHN borrows a runtime and shared globals. The compact proof does
     * not define that contract and must not publish a chain artifact. */
    if(chain) {d->code=COMPILE_PLATFORM_FAILURE;return 0;}
#endif
#ifdef ZP_UNITS_EXPERIMENT
    if(!cc_units_close())goto failed;
#endif
    if(!cc_tables_begin()){d->code=99;goto failed;}
    if(!cc_poll())goto failed;
    if(!cc_sources_prepare(s))goto failed;
    source_location(s,s->length,&b->source_lines,&j);
    if(compiler_disk_requested&&!cc_disk_begin(b,s,options&&options->kind==COMPILE_CHAIN)) {
        d->code=COMPILE_PLATFORM_FAILURE;goto failed;
    }
    if(cc_disk_active(b)) {
        if(!disk_prefix(chain))goto failed;
    } else for(i=0;i<3;++i) {
        /* The middle page holds final globals/code placement, never parser
         * staging. Materialize it on a global initializer's first write or
         * after relocation releases its workspace. */
        if(i==1)continue;
#ifdef ZP_UNITS_EXPERIMENT
        /* Defer the untouched runtime prefix until first mutation/finalization.
         * A compiled unit never needs an executable prefix. */
        if(!i)continue;
#endif
        b->pages[i]=page_new(PAGE_IMAGE);
        if(b->pages[i]==255){d->code=99;goto failed;}
        /* Initialize the runtime prefix directly, then clear its tail. Both
         * loops retain the original page's bounded polling before page2 is
         * allocated; no byte is marked initialized until it has been stored. */
        j=0;
        if(!i&&!chain){if(!cc_runtime_copy(b->pages[i]))goto failed;j=cc_runtime_size();}
        for(;j<PAGE_SIZE;j+=n) {
            if(!(j&255)&&!cc_poll())goto failed;
            /* End at the next existing polling boundary, including the
             * partial tail after the verified runtime prefix. */
            n=256u-(j&255u);
            if(!far_fill(b->pages[i],j,0,n)){d->code=99;goto failed;}
        }
        page_initialized(b->pages[i],PAGE_SIZE);
    }
    b->map_page=page_new(PAGE_MAP);
    if(b->map_page==255){d->code=99;goto failed;}
    buffer_init(b->map_page);

    cc.absolute_mode=1;cc.var_strict=1;cc.index_speed=1;cc.with_limit=4;
    cc.input_fresh=1;cc.io_check=1;cc.console_control=1;cc.text_type=TYPE_NONE;
    cc_new_type(K_INTEGER,2,-32768,32767,0);
    cc_new_type(K_STRING,0,0,0,0);
    cc_new_type(K_BOOLEAN,1,0,1,0);
    cc_new_type(K_CHAR,1,0,255,0);
    cc_new_type(K_INTEGER,1,0,255,0);
    cc_new_type(K_REAL,6,0,0,0);
    cc_system_init();
    cc_next();
#ifdef ZP_UNITS_EXPERIMENT
    b->code_origin=CC_CODE_BASE;cc.emitted=CC_CODE_BASE;
    if(cc_units_requested()) {
        sources.unit_mode=1;cc_units_header();
    } else
#endif
    if(cc_is("program")) {
        cc_next();cc_expect(T_ID,53);
        if(cc.token=='('){cc_next();cc_expect(T_ID,53);while(cc.token==','){cc_next();cc_expect(T_ID,53);}cc_expect(')',5);}
        cc_expect(';',1);
    }
    b->code_origin=CC_CODE_BASE;
#ifndef ZP_UNITS_EXPERIMENT
    cc.emitted=CC_CODE_BASE;
#else
    if(!sources.unit_mode&&cc_is("uses"))cc_units_imports();
#endif
#ifdef ZP_OVERLAYS
    if(want_overlays) {
        if(sources.memory||sources.unit_mode||(options&&options->kind==COMPILE_CHAIN)||!cc_disk_active(b)) {cc_fail(UNIT_UNSUPPORTED);goto failed;}
        if(!code_phase_overlays()||!cc_overlays_begin())goto failed;
    }
#endif
    if(options&&options->kind==COMPILE_CHAIN)cc_activation_begin();
#ifdef ZP_UNITS_EXPERIMENT
    if(!sources.unit_mode)cc_units_initializers();
#endif
    begin=cc_block();j=pc();cc_expect('.',10);
    if(cc.token!=T_EOF)cc_fail(58);
    cc_finish(RT_DONE);cc_mapping(j,pc(),begin,begin);
#ifdef ZP_UNITS_EXPERIMENT
    if(sources.unit_mode) {
        if(d->code||!cc_units_write()||!cc_units_close()||!cc_sources_finish()||!release_stages())goto failed;
        cc.polling=0;return 1;
    }
#endif
    if(d->code||!cc_sources_finish())goto failed;
#ifdef ZP_UNITS_EXPERIMENT
    if(d->code||!cc_units_link()||!cc_units_close())goto failed;
#endif
#ifdef ZP_OPTIONAL_REAL
    /* Parsing has written only CBreak in the prefix. Upgrade immutable code
     * before finalization installs keyboard, hook and error-handler bindings. */
    if(!cc_real_runtime_finish())goto failed;
#endif
#ifdef ZP_UNITS_EXPERIMENT
    /* Helpers inspect initial runtime state, including a CHN's empty prefix.
     * Place CBreak after import/link peak RAM and before helper selection. */
    if(!cc_disk_active(b)&&!cc_image_page(0))goto failed;
    cc_put(RT_CBREAK-0x4000u,sources.unit_cbreak);
#endif
    cc_params_finish();cc_random_finish();
    cc_file_engine_finish();cc_keyboard_finish();cc_interrupt_finish();
    cc_transfer_finish();cc_file_stream_finish();cc_hooks_finish();cc_console_finish();cc_file_finish();cc_error_finish();
#ifdef ZP_OVERLAYS
    /* Dispatch needs the decoder, but no late journal is needed merely to
     * parse resident declarations. Imported Unit metadata is gone here. */
    if(sources.overlays)cc_root_request(LATE_ROOT,LATE_UNPACK);
#endif
    cc_unpack_finish();
#ifdef ZP_STATIC_LAYOUT
    if(!cc_file_native_prepare())goto failed;
#endif
#ifdef ZP_OVERLAYS
    if(sources.overlays&&(!code_phase_overlay_asset()||!cc_overlay_finish()))goto failed;
#endif
    if(!options||options->kind!=COMPILE_CHAIN)cc_startup_finish();
    if(d->code||!cc_place_code())goto failed;
    if(!release_stages())goto failed;
    if(!cc_seal(s,options))goto failed;
    return 1;
failed:
    native_error=platform_error;cc.polling=0;
#ifdef ZP_UNITS_EXPERIMENT
    cc_units_close();
#endif
    if(cc.primary_stage!=255&&page_drop(cc.primary_stage))cc.primary_stage=255;
    if(cc.stage_page!=255&&(page_drop(cc.stage_page)))cc.stage_page=255;
    /* Error replies still need source identities, but not the abandoned
     * executable. Reclaim emission storage before exporting a bounded table. */
    build_discard_image(b);
#ifdef ZP_UNITS_EXPERIMENT
    if(sources.unit_mode) {
        /* A late write failure can consume both lent slots. Retry only after
         * every abandoned output/table page has had its release attempt. */
        cc_tables_end();code_phase_unit_finish_end();platform_error=native_error;
    }
#endif
    if(d->code!=COMPILE_INTERRUPTED
#ifdef ZP_OVERLAYS
       &&d->code!=COMPILE_OVERLAY_RETRY
#endif
    ) {
#if defined(__SDCC) || defined(ZP_COMPILER_JOBS)
        if(compiler_invocation)compiler_capture_failure();else
#endif
        cc_sources_error();
    }
    build_close(b);
#ifdef ZP_UNITS_EXPERIMENT
    /* Final linking also lends both generator slots. Retry their restoration
     * after terminal cleanup, when abandoned tables and images are gone. */
    code_phase_unit_finish_end();
#endif
    if(native_error)platform_error=native_error;return 0;
}

#ifdef ZP_OVERLAYS
uint8_t compile_project(const Source *s,Build *b,Diagnostic *d,uint8_t interruptible,const CompileOptions *options) {
    uint8_t ok;want_overlays=0;ok=compile_once(s,b,d,interruptible,options);
    if(!ok&&d->code==COMPILE_OVERLAY_RETRY){want_overlays=1;ok=compile_once(s,b,d,interruptible,options);}
    want_overlays=0;return ok;
}
#endif
