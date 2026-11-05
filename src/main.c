#include "editor.h"
#include "main_display.h"
#include "sources.h"
#include "keys.h"
#include "compiler_job.h"
#include "compiler_disk.h"
#include "compiler_messages.h"
#include <zos_sys.h>
#include <zos_errors.h>
#include <zos_vfs.h>
#include <zos_keyboard.h>
#include <zos_video.h>

/* Public symbols also let the emulator acceptance driver inspect state without
 * adding a host protocol, test menu or debugger to the product. */
Source work;
Build build;
CompileOptions compile_options={0,0xfbffu,COMPILE_PROGRAM};
char main_file[128];
uint8_t ide_state; /* 1 menu, 2 editor, 3 acknowledgment */
static uint8_t messages=1;
static uint8_t saved_mode;
static uint16_t compiled_main_lines;
extern uint8_t console_scratch[256];
static void block_io(uint8_t kind);
static void search_command(uint8_t kind);
static void search_ack(const char *message);
static uint8_t save_work_auto(void);

static void output_hex(uint16_t n,uint8_t digits) {
    char text[5]; uint8_t i;
    if(digits!=2&&digits!=4) return;
    text[digits]=0;
    for(i=digits;i>0;--i) { text[i-1]="0123456789ABCDEF"[n&15]; n>>=4; }
    output(text);
}
static void file_error(const char *prefix,uint8_t code) {
    /* Console writes themselves update platform_error. Pass the failed job's
     * value by value before any diagnostic output can overwrite it. */
    output(prefix); output_number(code);
    if(code==22) output(" (no free space or resources)");
    else if(code==21) output(" (no free entries)");
}
static uint8_t publication_interrupted(void) {
    uint8_t reason=output_interrupt_reason(),error=platform_error,prior=ide_state;
    if(!reason) return 0;
    ide_state=3;
    output(reason==2?"Save interrupted: typeahead full":"*** SAVE INTERRUPTED");
    if(error) file_error(". Native I/O error ",error);

    search_ack("RAM source retained"); ide_state=prior; return 1;
}

static void invalidate(void) {
    /* Work may be an include; its revision is independent of Main's. */
    build_close(&build);
    build.code_bytes=build.globals_bytes=0;
}
static void scratch_conflict(uint8_t error) {
    if(error==ERR_ALREADY_EXIST) {
        output("\nScratch file exists: ");output(cc_disk_path());
        output("\nRemove it or rename it, then retry.\n");
    }
}
static void lookup_failure(void) {
    output("\nSource lookup failed; Work retained. Press Escape");
    while(key_read()!=27) {}
}
static uint8_t select_error_source(const char *path) {
    Diagnostic d;
    if(!strcmp(path,work.path))return 1;
    /* TP3 saves the actual dirty Work before an include replaces it. */
    if(work.dirty&&!save_work_auto())return 0;
    invalidate();
    if(!reload_source(&work,path,&d))return 0;
    editor_forget();return 1;
}
static uint16_t draw_line(uint16_t p,uint8_t row,uint8_t selected) {
    char text[129];
    uint8_t n=0,col=0,shown=0,c,style=0,mark;
    screen_at(0,row);
    while(p<work.length) {
        c=buffer_get(work.page,p);
        if(c=='\n') break;
        mark=c<32||c==127||(selected&&p>=editor.block_begin&&p<editor.block_end);
        ++p;
        if(col>=editor.left&&shown<screen_width&&shown<128) {
            if(mark!=style) {
                text[n]=0; output(text); n=0; screen_mark(mark); style=mark;
            }
            if(c<32) c+=64;
            else if(c==127) c='?';
            /* Keep byte selection and indexing separate: the pinned SDCC
             * miscompiles a combined conditional/postincrement in this loop. */
            text[n]=(char)c; ++n; ++shown;
        }
        ++col;
    }
    text[n]=0; output(text); if(style) screen_mark(0);
    return p;
}
/* The edit-loop full predicate is the source-row render dependency list.
 * Keep it in sync with draw_line/redraw when adding render state. */
static void redraw(uint8_t full) {
    uint16_t line,col,p=editor.top,top_line;
    uint8_t row,selected;
    source_location(&work,work.cursor,&line,&col);
    screen_mark(0);
    if(full)screen_clear();else screen_at(0,0);
    main_draw_status(line,!full);
    if(full) {
        selected=editor.block_visible&&editor.block_marks==3&&editor.block_begin<editor.block_end;
        for(row=1;row<screen_height;++row) {
            p=draw_line(p,row,selected);
            if(p==work.length) break;
            ++p;
        }
    }
    source_location(&work,editor.top,&top_line,&col);
    screen_at((uint8_t)(editor.column-1-editor.left),(uint8_t)(line-top_line+1));
}
static uint8_t suffix(uint8_t c) { return c>=1&&c<=26?c+'a'-1:c>='A'&&c<='Z'?c+32:c; }
/* Menu command normalization accepts control letters. Confirmation prompts
 * must retain Ctrl-U as cancellation rather than normalize it to letter U. */
static uint8_t confirmation_key(void) {
    uint8_t c=key_read(); return c==21?c:suffix(c);
}
static void edit(uint8_t at_error) {
    uint8_t result,key,plain,marks,visible,full=1;
    uint16_t top,left,begin,end;
    uint32_t revision,lifetime;
    editor_begin(&work,at_error,screen_width,screen_height); ide_state=2;
    for(;;) {
        redraw(full);
        key=key_read();
        revision=work.revision; lifetime=page_lifetime(work.page);
        top=editor.top; left=editor.left;
        begin=editor.block_begin; end=editor.block_end;
        marks=editor.block_marks; visible=editor.block_visible;
        plain=!editor.prefix&&!editor.quote;
        result=editor_key(key);
        /* Prefixes, dialogs and uncertain results keep the full redraw. Only
         * unchanged text, viewport and selection can reuse the source rows. */
        full=!(plain&&!editor.prefix&&!editor.quote&&result==EDITOR_OK&&
            work.revision==revision&&page_lifetime(work.page)==lifetime&&
            editor.top==top&&editor.left==left&&editor.block_begin==begin&&
            editor.block_end==end&&editor.block_marks==marks&&editor.block_visible==visible);
        if(result==EDITOR_READ_BLOCK||result==EDITOR_WRITE_BLOCK) block_io(result);
        if(result==EDITOR_FIND||result==EDITOR_REPLACE||result==EDITOR_REPEAT) search_command(result);
        if(work.revision!=revision) {
            invalidate();
        }
        if(result==EDITOR_DONE) return;
        if(result==EDITOR_REJECT||result==EDITOR_ABORT) {
            screen_at(0,screen_height-1);
            output(result==EDITOR_ABORT?"*** INTERRUPTED. Press Escape":"Edit rejected: line or source capacity. Press Escape");
            while(key_read()!=27) {}
        }
    }
}
static void diagnostic(uint8_t kind,uint16_t code,const char *path,
                       uint16_t position,uint16_t pc,uint8_t canonical) {
    uint16_t line,col;
    uint8_t located;
    ide_state=3;
    screen_mark(0);screen_clear();
    output(kind==0?"User break":kind==1?"Compiler error ":kind==2?"Runtime error ":"I/O error ");
    if(kind==1) output_number(code); else if(kind) output_hex(code,2);
    if(messages&&kind==1)compiler_message((uint8_t)code);
    if(kind!=1) { output(" PC="); output_hex(pc,4); }
    if(kind==3&&ram_io_detail) { output(" (native error "); output_number(ram_io_detail); output(")"); }
    output("\n");output(path);
    located=select_error_source(path);
    if(located) {
        if(canonical)position=source_editor_offset(&work,position);
        located=position<=work.length;
    }
    if(located) {
        source_location(&work,position,&line,&col);
        output(" Line ");output_number(line);output(" Col ");output_number(col);
    }
    if(messages&&kind==2&&code==2) output(" Division by zero");
    output(located?"\nPress Escape to return to source":"\nSource lookup failed; Work retained. Press Escape");
    while(key_read()!=27) {}
    if(located) { work.cursor=position;edit(1); }
}
static uint8_t save_work_as(const char *name,uint8_t automatic) {
    uint8_t ok;Source candidate;
    candidate=work;
    if(!automatic)screen_clear();
    output("Saving source "); output(name?name:work.path); output(" (Ctrl-U cancels)...\n");
    ok=name?save_source_as(&candidate,name,1,1):save_source(&candidate,1,1);
    if(ok) {
        if(candidate.revision!=work.revision)invalidate();
        work=candidate;output("Source saved: ");output(work.path);output("\n");
    }
    else if(publication_interrupted()) return 0;
    else { file_error("Save failed, native error ",platform_error); output(". RAM source retained."); }
    if(!ok)search_ack("");
    return ok;
}
static uint8_t save_work(void) { return save_work_as(0,0); }
static uint8_t save_work_auto(void) { return save_work_as(0,1); }
static uint8_t file_field(char *text) {
    uint8_t c,n=0; char one[2]={0,0};
    text[0]=0;
    for(;;) {
        c=key_read();
        if(c==27||c==21) return c==21?2:0;
        if(c=='\n') return n?1:3;
        if(c==127&&n) { text[--n]=0; output("\b \b"); }
        else if(c>=32&&c<127) {
            if(n==127) { output("\nFilename exceeds 127 bytes. Press Escape"); while(key_read()!=27) {} return 0; }
            text[n++]=c; text[n]=0; one[0]=c; output(one);
        }
    }
}
static void search_ack(const char *message) {
    output("\n"); if(*message) { output(message); output(". "); } output("Press Escape");
    /* Acknowledgment reads fresh events; it must not discard retained typing
     * from the interrupted operation while waiting for Escape. */
    key_typeahead_hold(1);
    while(key_read()!=27) {}
    key_typeahead_hold(0);
}
static void field_draw(const char *prompt,const EditorField *field) {
    char blank[129],one[2];
    uint8_t i,c;
    memset(blank,' ',screen_width<128?screen_width:128); blank[screen_width<128?screen_width:128]=0;
    screen_at(0,0); screen_mark(0); output(blank); screen_at(0,0); output(prompt);
    one[1]=0;
    for(i=0;i<field->length;++i) {
        c=field->text[i]; screen_mark(c<32||c==127);
        if(c<32) c+=64;
        else if(c==127) c='?';
        one[0]=(char)c; output(one);
    }
    screen_mark(0); screen_at((uint8_t)(strlen(prompt)+field->cursor),0);
}
static uint8_t search_field(const char *prompt,EditorField *field,const uint8_t *recall,uint8_t length) {
    uint8_t result;
    editor_field_begin(field);
    for(;;) {
        field_draw(prompt,field);
        result=editor_field_key(field,key_read(),recall,length);
        if(result==FIELD_DONE) return 1;
        if(result==FIELD_CANCEL) return 0;
        if(result==FIELD_ABORT) { search_ack("*** INTERRUPTED"); return 0; }
        if(result==FIELD_REJECT) { search_ack("Field exceeds30 bytes or unsupported control"); }
    }
}
static uint8_t search_setup(uint8_t replacing) {
    EditorField pattern,replacement,options;
    if(!search_field("Find: ",&pattern,editor.search.pattern,editor.search.length)||!pattern.length) return 0;
    editor_field_begin(&replacement);
    if(replacing&&!search_field("Replace with: ",&replacement,
        editor.search.replacement,editor.search.replacement_length)) return 0;
    editor_field_begin(&options);
    for(;;) {
        field_draw("Options: ",&options);
        switch(editor_field_key(&options,key_read(),0,0)) {
        case FIELD_CANCEL: return 0;
        case FIELD_ABORT: search_ack("*** INTERRUPTED"); return 0;
        case FIELD_REJECT: search_ack("Field exceeds30 bytes"); break;
        case FIELD_DONE:
            if(editor_search_configure(pattern.text,pattern.length,replacement.text,replacement.length,
                options.text,options.length,replacing)) return 1;
            search_ack("Invalid options or replacement"); break;
        }
    }
}
static void search_command(uint8_t kind) {
    EditorSearchJob job;
    uint8_t result,c,interrupt;
    if(kind!=EDITOR_REPEAT&&!search_setup(kind==EDITOR_REPLACE)) return;
    if(!editor_search_begin(&job)) return;
    for(;;) {
        interrupt=key_interrupt();
        if(interrupt) {
            search_ack(interrupt==1?"*** INTERRUPTED":"Search interrupted: typeahead full"); return;
        }
        result=editor_search_step(&job);
        if(result==SEARCH_MORE) continue;
        if(result==SEARCH_MATCH) {
            c='y';
            if(!(job.search.flags&SEARCH_NO_CONFIRM)) {
                redraw(1); screen_at(0,0); output("Replace (Y/N), Ctrl-U aborts: ");
                do { c=confirmation_key(); } while(c!='y'&&c!='n'&&c!=21&&c!=27);
                if(c==27) return;
                if(c==21) { search_ack("*** INTERRUPTED"); return; }
            }
            result=editor_search_accept(&job,c=='y');
        }
        if(result==SEARCH_DONE) return;
        if(result==SEARCH_MISSING) { search_ack("Search string not found"); return; }
        if(result==SEARCH_REJECT) { search_ack("Edit rejected: line or source capacity"); return; }
    }
}
static void block_io(uint8_t kind) {
    char name[128],blank[129];
    Source block;
    Diagnostic d;
    uint8_t field,c,ok,result,error;
    memset(blank,' ',screen_width<128?screen_width:128); blank[screen_width<128?screen_width:128]=0;
    for(;;) {
        screen_at(0,0); output(blank); screen_at(0,0);
        output(kind==EDITOR_READ_BLOCK?"Read block from file: ":"Write block to file: ");
        field=file_field(name);
        if(field!=1) {
            if(field==2) {
                output("\n*** INTERRUPTED. Press Escape"); while(key_read()!=27) {}
            }
            return;
        }
        if(kind==EDITOR_WRITE_BLOCK) {
            ok=save_block(&work,editor.block_begin,editor.block_end,name,0,1);
            if(!ok&&platform_error==15&&!output_interrupt_reason()) {
                output("\nOverwrite old file (Y/N), Escape cancels: ");
                do { c=confirmation_key(); } while(c!='y'&&c!='n'&&c!=27&&c!=21);
                if(c==21) { output("\n*** INTERRUPTED. Press Escape"); while(key_read()!=27) {} return; }
                if(c==27) return;
                if(c=='n') continue;
                ok=save_block(&work,editor.block_begin,editor.block_end,name,1,1);
            }
            if(ok||publication_interrupted()) return;
        } else {
            if(read_block(&block,name,&d)) {
                result=editor_insert_block(&block); source_discard(&work,&block);
                if(result==EDITOR_OK) return;
                output("\nBlock insertion rejected: line or source capacity. Press Escape");
                while(key_read()!=27) {} return;
            }
            source_discard(&work,&block);
        }
        error=platform_error; output("\n");
        if(kind==EDITOR_READ_BLOCK&&d.code) file_error("Block source error ",d.code);
        else file_error("Block file error ",error);
        output(". Press Escape"); while(key_read()!=27) {} return;
    }
}
static void open_work(void) {
    char name[128],path[128];uint8_t c=0;Diagnostic d;
    if(work.dirty) {
        output("Save current source first (Y/N), Escape cancels: ");
        do { c=confirmation_key(); } while(c!='y'&&c!='n'&&c!=27&&c!=21);
        if(c==27||c==21||(c=='y'&&!save_work()))return;
    }
    screen_clear();output("Open work file (for example T:/HELLO.PAS): ");
    if(file_field(name)!=1)return;
    if(!source_path(name,path)) { lookup_failure();return; }
    output("\nLoading...\n");
    invalidate();
    if(!load_source(&work,path,&d)) {
        file_error(d.code?"Source error ":"Native file error ",d.code?d.code:platform_error);
        output(". Current buffer retained. Press Escape");while(key_read()!=27) {}return;
    }
    editor_forget();
}
static void select_main(void) {
    char name[128],path[128];uint8_t field;
    screen_clear();output("Main file (empty uses Work): ");
    field=file_field(name);
    if(field==3)path[0]=0;
    else if(field!=1)return;
    else if(!source_path(name,path)) { lookup_failure();return; }
    if(strcmp(path,main_file)) { strcpy(main_file,path);invalidate(); }
}
static uint8_t quit_work(void) {
    uint8_t c;
    if(work.dirty) {
        output("Save ");output(work.path);output(" before quitting (Y/N), Escape cancels: ");
        do { c=confirmation_key(); } while(c!='y'&&c!='n'&&c!=27&&c!=21);
        if(c==27||c==21)return 0;
        if(c=='y'&&!save_work())return 0;
    }
    return 1;
}
static void save_as_work(void) {
    char name[128];
    screen_clear(); output("Save source as (for example T:/untitled.pas): ");
    if(file_field(name)!=1) return;
    if(save_work_as(name,0)) invalidate();
}
static uint8_t compile_work(uint8_t running) {
    Diagnostic d;CompileFailure failure;Source root;uint8_t ok;
    const char *path=main_file[0]?main_file:work.path;
#ifdef ZP_UNITS_EXPERIMENT
    if(saved_mode==COMPILER_UNIT_MODE) {
        if(running){screen_clear();search_ack("Compile units with C; run a program that uses the unit");return 0;}
        /* Imports bind matching source for the existing error-location contract. */
        if(work.dirty&&!save_work_auto())return 0;
        if(!strcmp(path,work.path)) {
            /* Source publication trims line tails. Compile that same text so
             * imported fingerprints and error offsets agree with the editor. */
            if(!load_source(&work,work.path,&d)) {
                file_error(d.code?"Source error ":"Native file error ",d.code?d.code:platform_error);
                search_ack("Source retained");return 0;
            }
            editor_forget();
        }
    }
#endif
    invalidate();screen_clear();
    if(!strcmp(path,work.path))root=work;
    else {
        /* With a different Main, TP3 saves Work before loading Main. */
        if(work.dirty&&!save_work_auto())return 0;
        memset(&root,0,sizeof(root));root.page=255;strcpy(root.path,path);
    }
#ifdef ZP_UNITS_EXPERIMENT
    if(saved_mode==COMPILER_UNIT_MODE)output("Compiling unit (Ctrl-U cancels)...\n");
    else
#endif
    output(saved_mode==2?"Compiling native CHN (Ctrl-U cancels)...\n":saved_mode?"Compiling to native COM (Ctrl-U cancels)...\n":"Compiling to Memory (Ctrl-U cancels)...\n");
    ok=compile_request(&root,&build,&failure,saved_mode,saved_mode?&compile_options:0);
    d=failure.diagnostic;
    if(!ok) {
        build.code_bytes=build.globals_bytes=0;
        if(d.code==COMPILE_PLATFORM_FAILURE) {
            uint8_t error=platform_error;
            file_error("Compile failed, native error ",error);
            if(saved_mode!=COMPILER_UNIT_MODE)scratch_conflict(error);
            search_ack("Source retained");return 0;
        }
        if(d.code==COMPILE_INTERRUPTED) {
            ide_state=3;

            search_ack(failure.interrupted==2?"Compile interrupted: typeahead full":"*** COMPILE INTERRUPTED");return 0;
        }
        if(strcmp(failure.path,root.path)&&!main_file[0])strcpy(main_file,root.path);
        diagnostic(1,d.code,failure.path,d.start,0,0);return 0;
    }
#ifdef ZP_UNITS_EXPERIMENT
    if(saved_mode==COMPILER_UNIT_MODE) {
        output("Wrote ");output(failure.path);output("\n");return 0;
    }
#endif
    if(!sources_get(&build.sources,0,&root)) { cancel_prepared_build();lookup_failure();return 0; }
    if(saved_mode) {
        output("Saving executable (Ctrl-U cancels)...\n");
        if(!save_build(&root,&build,1)) {
            ok=platform_error;
            cancel_prepared_build();
            if(ok)platform_error=ok;
            if(output_interrupt_reason()) {
                ok=platform_error; invalidate(); platform_error=ok;
                publication_interrupted(); return 0;
            }
            file_error("Output failed, native error ",platform_error);
            output(". RAM source retained.");
            invalidate();search_ack("");return 0;
        }
        output("Saved ");output(output_path());
        output("\n");
    }
    compiled_main_lines=build.source_lines;
    if(saved_mode&&!running)build_close(&build);
    return 1;
}
static uint8_t hexadecimal(const char *text,uint16_t *value) {
    uint8_t i,digit;
    if(!text[0]||strlen(text)>4)return 0;
    *value=0;
    for(i=0;text[i];++i) {
        digit=suffix((uint8_t)text[i]);
        if(digit>='0'&&digit<='9')digit-='0';
        else if(digit>='a'&&digit<='f')digit-=('a'-10);
        else return 0;
        *value=(*value<<4)|digit;
    }
    return 1;
}
static void options_screen(void) {
    CompileLimits limits;
    screen_clear();output("Compiler options\nM Memory  C Com-file  H cHn-file  ");
#ifdef ZP_UNITS_EXPERIMENT
    output("U Unit-file  ");
#endif
    if(saved_mode!=COMPILER_UNIT_MODE)output("F Find run-time error  ");
    output("Q Return\n");
#ifdef ZP_UNITS_EXPERIMENT
    if(saved_mode==COMPILER_UNIT_MODE){output("Compiled unit (.ZPU) selected\n");return;}
#endif
    if(!saved_mode) {output("Memory selected\n");return;}
    compile_limits(build.code_bytes?build.globals_bytes:0,build.code_bytes,&compile_options,&limits);
    output(saved_mode==2?"Native cHn-file selected\nS Start  E End\nStart: ":"Native Com-file selected\nS Start  E End\nStart: ");
    if(compile_options.start)output_hex(compile_options.start,4);else output("AUTO");
    output(" (min ");output_hex(limits.minimum_start,4);output(" max ");output_hex(limits.maximum_start,4);
    output(")\nEnd: ");output_hex(compile_options.end,4);
    output(" (min ");output_hex(limits.minimum_end,4);output(" max FBFF)\n");
    output(build.code_bytes?"Last compiled program bounds\n":"Platform bounds; program extents checked at Compile\n");
}
static void option_address(uint8_t start) {
    char text[128];CompileOptions proposed;CompileLimits limits;
    uint16_t value=0;uint8_t field;
    for(;;) {
        output(start?"Start hex (empty or 0=AUTO): ":"End hex (empty=FBFF): ");
        field=file_field(text);if(field!=1&&field!=3)return;
        proposed=compile_options;
        if(field==3||hexadecimal(text,&value)) {
            if(field==3)value=start?0:0xfbffu;
            if(start)proposed.start=value;else proposed.end=value;
            if(compile_limits(build.code_bytes?build.globals_bytes:0,build.code_bytes,&proposed,&limits)) {
                if(proposed.start!=compile_options.start||proposed.end!=compile_options.end) {
                    compile_options=proposed;build_close(&build);
                }
                return;
            }
        }
        output("\nInvalid address or program bounds; value unchanged.\n");
    }
}
static uint8_t find_saved(void) {
    char text[128],root[128],found_path[128];uint16_t found_position=0;
    Source source,located;Diagnostic error;
    uint16_t pc,line,column;uint8_t field,file,loaded=0,ok,interrupted,native_error;
    strcpy(root,main_file[0]?main_file:work.path);
    for(;;) {
        screen_clear();output("Find run-time error\nEnter PC: ");
        field=file_field(text);
        if(field!=1)return 0;
        if(hexadecimal(text,&pc))break;
        output("\nInvalid hexadecimal PC. Press Escape to retry");while(key_read()!=27) {}
    }
    /* TP3 recovers the location from source, including in a fresh session.
     * Reuse the compiler's statement mapping; no executable or sidecar is an
     * input, and this read-only compile never publishes output files. */
    invalidate();
    if(!strcmp(root,work.path))source=work;
    else {
        if(work.dirty&&!save_work_auto())return 0;
        memset(&source,0,sizeof(source));source.page=255;
        if(!read_block(&source,root,&error)){source_discard(&work,&source);lookup_failure();return 0;}
        loaded=1;
    }
    output("\nFinding position from source (Ctrl-U cancels)...\n");
    compiler_disk_requested=saved_mode!=0;
    ok=compile_project(&source,&build,&error,1,&compile_options);
    compiler_disk_requested=0;
    native_error=error.code==COMPILE_PLATFORM_FAILURE?platform_error:0;
    interrupted=error.code==COMPILE_INTERRUPTED?compile_interrupt_reason():0;
    if(ok)ok=build_location(&source,&build,pc,&file,&found_position)&&
        sources_get(&build.sources,file,&located);
    if(ok)strcpy(found_path,located.path);
    build_close(&build);
    if(loaded)source_discard(&work,&source);
    if(!ok){
        if(interrupted)search_ack(interrupted==2?"Lookup interrupted: typeahead full":"*** LOOKUP INTERRUPTED");
        else {scratch_conflict(native_error);lookup_failure();}
        return 0;
    }
    if(!select_error_source(found_path)) {lookup_failure();return 0;}
    if(strcmp(found_path,root)&&!main_file[0])strcpy(main_file,root);
    pc=source_editor_offset(&work,found_position);
    source_location(&work,pc,&line,&column);ide_state=3;
    output("Run-time error position found\n");output(found_path);
    output(" Line ");output_number(line);output(" Col ");output_number(column);
    output("\nPress Escape to return to source");while(key_read()!=27) {}
    work.cursor=pc;edit(1);return 1;
}
/* Resume console service without repeating startup: console_open also resets
 * the page-version ledger and remembers the original shell cursor. */
static uint8_t external_console(void) {
    uint8_t error;
    extern uint8_t console_scratch[256];
    key_reset();
    error=ioctl(DEV_STDIN,KB_CMD_SET_MODE,(void*)KB_MODE_RAW);
    if(error)return error;
    /* GET_AREA reports fixed geometry even after a child selects graphics.
     * The supported reset restores the real text mode and palette. */
    error=ioctl(DEV_STDOUT,CMD_RESET_SCREEN,0);
    if(error)return error;
    error=ioctl(DEV_STDOUT,CMD_GET_AREA,console_scratch);
    if(error)return error;
    if(console_scratch[0]<80||console_scratch[1]<24)return ERR_NOT_SUPPORTED;
    screen_width=console_scratch[0];screen_height=console_scratch[1];
    screen_mark(0);return platform_error;
}
static void execute_work(void) {
    char name[128],path[128],directory[128];
    uint8_t error,status=255,restore=0;
    extern uint8_t console_scratch[256];
    screen_clear();output("Execute native program: ");
    if(file_field(name)!=1)return;
    if(!native_path(name,path))error=platform_error;
    else {
        error=curdir((char*)console_scratch);
        if(!error) {
            strcpy(directory,(char*)console_scratch);
            error=ioctl(DEV_STDIN,KB_CMD_SET_MODE,(void*)(KB_MODE_RAW|KB_READ_NON_BLOCK));
            if(!error) {
                error=execute_native(path,&status);
                if(!error) {
                    strcpy((char*)console_scratch,directory);
                    restore=chdir((char*)console_scratch);
                }
            }
            {uint8_t console=external_console();if(!restore)restore=console;}
        }
    }
    ide_state=3;
    if(error)file_error("\nExecute failed, native error ",error);
    else {output("\nProgram returned, status ");output_number(status);}
    if(restore)file_error("\nIDE restore failed, native error ",restore);
    output(". Press Escape");while(key_read()!=27) {}
}
static void show_directory(void) {
    char path[128];uint8_t error;
    error=curdir((char*)console_scratch);
    if(!error&&!memchr(console_scratch,0,128))error=ERR_INVALID_PATH;
    if(!error)memcpy(path,console_scratch,128);
    output("Directory: ");
    if(error)file_error("unavailable, native error ",error);else output(path);
    output("\n");
}
static void logged_drive(void) {
    char name[128],root[4];uint8_t n,c,error;
    screen_clear();output("Logged drive (A-Z, empty keeps current): ");
    if(file_field(name)!=1)return;
    n=(uint8_t)strlen(name);c=(uint8_t)name[0];if(c>='a'&&c<='z')c-=32;
    if(c<'A'||c>'Z'||n>3||(n>1&&name[1]!=':')||(n>2&&name[2]!='/'))error=ERR_INVALID_PATH;
    else {
        root[0]=(char)c;root[1]=':';root[2]='/';root[3]=0;
        strcpy((char*)console_scratch,root);error=chdir((char*)console_scratch);
    }
    if(!error)return;
    ide_state=3;file_error("\nLogged drive failed, native error ",error);
    output(". Press Escape");while(key_read()!=27) {}
}
static uint8_t directory_match(const char *pattern,const char *name) {
    const char *star=0,*retry=0;
    while(*name) {
        if(*pattern=='*') {star=++pattern;retry=name;}
        else if(*pattern=='?'||*pattern==*name) {++pattern;++name;}
        else if(star) {pattern=star;name=++retry;}
        else return 0;
    }
    while(*pattern=='*')++pattern;
    return !*pattern;
}
static int8_t directory_open(const char *path) {
    strcpy((char*)console_scratch,path);return opendir((char*)console_scratch);
}
static void directory_header(const char *path,const char *mask) {
    screen_clear();output("Directory: ");output(path);
    output("\nMask: ");output(mask);output("\n");
}
static void directory_list(void) {
    char name[128],path[128],mask[16];zos_dir_entry_t entry;
    uint8_t field,error=0,closing,wild=0,trailing,any=0,rows=0,c,interrupted=0;
    uint16_t n,base=3,i;
    int8_t fd=-1;
    screen_clear();output("Directory path/mask (empty lists all): ");
    field=file_field(name);if(field==0||field==2)return;
    if(field==3)strcpy(name,".");
    n=(uint16_t)strlen(name);trailing=name[n-1]=='/';
    error=curdir((char*)console_scratch);
    if(!error&&!memchr(console_scratch,0,128))error=ERR_INVALID_PATH;
    if(error)goto done;
    if(!sources_directory(name,(char*)console_scratch,path)) {error=ERR_INVALID_PATH;goto done;}
    n=(uint16_t)strlen(path);
    for(i=3;i<n;++i)if(path[i]=='/')base=i+1;
    for(i=3;i<n;++i)if(path[i]=='*'||path[i]=='?') {
        if(i<base||trailing) {error=ERR_INVALID_PATH;goto done;}
        wild=1;
    }
    if(!wild) {
        fd=directory_open(path);
        if(fd<0&&(-fd!=ERR_NO_SUCH_ENTRY&&-fd!=ERR_NOT_A_DIR||trailing||n==3)) {
            error=(uint8_t)-fd;goto done;
        }
    }
    if(fd>=0)strcpy(mask,"*");
    else {
        strcpy(mask,path+base);path[base>3?base-1:3]=0;
        fd=directory_open(path);if(fd<0) {error=(uint8_t)-fd;goto done;}
    }
    directory_header(path,mask);
    for(;;) {
        error=readdir(fd,(zos_dir_entry_t*)console_scratch);
        if(error==ERR_NO_MORE_ENTRIES) {error=0;break;}
        if(error)break;
        memcpy(&entry,console_scratch,sizeof(entry));
        if(!entry.d_name[0]||!memchr(entry.d_name,0,sizeof(entry.d_name))) {error=ERR_ENTRY_CORRUPTED;break;}
        for(i=0;entry.d_name[i];++i)if((uint8_t)entry.d_name[i]<32||entry.d_name[i]==127||
            entry.d_name[i]=='/'||entry.d_name[i]==':')break;
        if(entry.d_name[i]) {error=ERR_ENTRY_CORRUPTED;break;}
        if(!strcmp(entry.d_name,".")||!strcmp(entry.d_name,"..")||!directory_match(mask,entry.d_name))continue;
        if(rows==screen_height-5) {
            output("More: Space/Enter continues, Escape/Ctrl-U stops");
            do {c=key_read();}while(c!=' '&&c!='\n'&&c!=27&&c!=21);
            if(c==27||c==21) {interrupted=1;break;}
            directory_header(path,mask);rows=0;
        }
        output(entry.d_name);if(D_ISDIR(entry.d_flags))output("/");output("\n");
        ++rows;any=1;
    }
    closing=close(fd);fd=-1;if(!error)error=closing;
done:
    if(interrupted&&!error)return;
    ide_state=3;
    if(error) {file_error("\nDirectory failed, native error ",error);output("\n");}
    else if(!any)output("No matching entries\n");
    output("Free space unavailable\nPress Escape");while(key_read()!=27) {}
}
int app_main(void) {
    uint8_t c,status,file,menu=1;
    uint16_t pc,position;Source root,failed;
    Diagnostic startup;
    memset(&build,0,sizeof(build)); memset(build.pages,255,3); build.map_page=255;
    if(!console_open()) { output("Zeal Pascal needs the supported MMU machine and 80x24 PS/2 console.\n"); return 1; }
    if(!source_new(&work,DEFAULT_SOURCE)) { output("Not enough source memory\n"); console_close(); return 1; }
    screen_clear(); output("Zeal Pascal\nExplanatory error messages (Y/N)? ");
    if(!load_source(&work,work.path,&startup)) {
        file_error("\nDefault source unavailable (error ",startup.code?startup.code:platform_error);
        output("); empty RAM buffer retained.\nExplanatory error messages (Y/N)? ");
    }
    do { c=suffix(key_read()); } while(c!='y'&&c!='n'); messages=c=='y';
    for(;;) {
        ide_state=1;
        if(menu) {
            screen_clear();show_directory();
#ifdef ZP_UNITS_EXPERIMENT
            if(saved_mode==COMPILER_UNIT_MODE)output("Zeal Pascal - Unit-file\nWork: ");
            else
#endif
            output(saved_mode==2?"Zeal Pascal - native cHn-file\nWork: ":saved_mode?"Zeal Pascal - native Com-file\nWork: ":"Zeal Pascal - Memory\nWork: "); output(work.path);
            output(work.dirty?" *\n":"\n");output("Main: ");
            if(main_file[0])output(main_file);else output("(Work)");
            output("\n");output_number(work.length); output(" source bytes; ");
            output_number(pages_used()); output(" owned pages; free memory unavailable\n");
            if(build.code_bytes) {
                output("Last compile - main lines: ");output_number(compiled_main_lines);
                output("\nCode bytes: ");output_number(build.code_bytes);
                output(" Data bytes: ");output_number(build.globals_bytes);
                output("\nFree space unavailable\n");
            }
            output("L Logged drive  D Dir  W Open  M Main  E Edit\nS Save  A Save as  C Compile  R Run  X Execute  O Options  Q Quit\n");
            menu=0;
        }
        output("\n>");c=suffix(key_read());
        if(c=='\n') {menu=1;continue;}
        if(c=='e') {edit(0);menu=1;}
        else if(c=='w') open_work();
        else if(c=='m') select_main();
        else if(c=='s') save_work();
        else if(c=='a') save_as_work();
        else if(c=='c') compile_work(0);
        else if(c=='l') logged_drive();
        else if(c=='d') directory_list();
        else if(c=='x') execute_work();
        else if(c=='o') {
            menu=1;options_screen();
            do {
                c=suffix(key_read());
                if(c=='m'||c=='c'||c=='h'
#ifdef ZP_UNITS_EXPERIMENT
                   ||c=='u'
#endif
                ) {
                    status=c=='m'?0:c=='c'?1:c=='h'?2:COMPILER_UNIT_MODE;
                    if(saved_mode!=status) {
                        if(saved_mode==COMPILER_UNIT_MODE||status==COMPILER_UNIT_MODE||
                           compile_options.kind!=(status==2?COMPILE_CHAIN:COMPILE_PROGRAM))build.code_bytes=build.globals_bytes=0;
                        saved_mode=status;compile_options.kind=status==2?COMPILE_CHAIN:COMPILE_PROGRAM;build_close(&build);
                    }
                    options_screen();
                }
                else if(saved_mode&&saved_mode!=COMPILER_UNIT_MODE&&(c=='s'||c=='e')) {option_address(c=='s');options_screen();}
                else if(c=='f'&&saved_mode!=COMPILER_UNIT_MODE) {
                    if(find_saved())break;
                    options_screen();
                }
            } while(c!='q');
        }
        else if(c=='r') {
            uint8_t ran,error;
            if((saved_mode||!build.valid||!sources_get(&build.sources,0,&root)||!build_ready(&root,&build))&&!compile_work(1))continue;
            if(!sources_get(&build.sources,0,&root)) { lookup_failure();continue; }
            if(saved_mode==2) {
                build_close(&build);
                screen_clear();search_ack("Run CHN files with Chain(F) from a saved Pascal program");continue;
            }
            screen_clear(); output("Validating and preparing program...\n");
            if(saved_mode) {
                ran=run_saved(&root,&build,&status);error=platform_error;
                screen_mark(0);platform_error=error;
                if(!ran) {
                    file_error("Saved program failed validation/exec. Native error ",platform_error);search_ack("");
                } else { output("\nProgram returned, status "); output_number(status);output("\n"); }
            } else {
                ran=run_build(&root,&build);error=platform_error;
                screen_mark(0);platform_error=error;
                if(!ran) { output("Native run failed; source retained. Press Escape"); while(key_read()!=27) {} }
                else if(ram_result[26]==6) {
                    output("Native platform failure "); output_number(ram_result[28]);
                    output(". Source retained. Press Escape"); while(key_read()!=27) {}
                } else if(ram_result[26]==3||ram_result[26]==5) {
                    pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
                    if(build_location(&root,&build,pc,&file,&position)&&
                       sources_get(&build.sources,file,&failed)) {
                        if(file&&!main_file[0])strcpy(main_file,root.path);
                        diagnostic(ram_result[26]==5?0:ram_result[27],ram_result[28],failed.path,position,pc,1);
                    }
                    else { output("Source lookup failed. Press Escape"); while(key_read()!=27) {} }
                } else { output("\nProgram returned.\n"); }
            }
        } else if(c=='q'&&quit_work())break;
    }
    build_close(&build);source_close(&work);console_close(); return 0;
}
