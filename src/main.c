#include "editor.h"

/* Public symbols also let the emulator acceptance driver inspect state without
 * adding a host protocol, test menu or debugger to the product. */
Source work;
Build build;
uint8_t ide_state; /* 1 menu, 2 editor, 3 error acknowledgment */
static uint8_t messages=1;
static uint8_t saved_mode;
static void block_io(uint8_t kind);
static void search_command(uint8_t kind);

static void file_error(const char *prefix,uint8_t code) {
    /* Console writes themselves update platform_error. Pass the failed job's
     * value by value before any diagnostic output can overwrite it. */
    output(prefix); output_number(code);
    if(code==22) output(" (no free space or resources)");
    else if(code==21) output(" (no free entries)");
}
static void recovery_notice(void) {
    if(*output_recovery()) { output("\nTemporary/recovery files remain: "); output(output_recovery()); }
}

static void invalidate(void) {
    if(!build.valid||build.revision!=work.revision) build_close(&build);
}
static uint8_t digits(uint16_t value) {
    uint8_t count=1;
    while(value>=10) { value/=10; ++count; }
    return count;
}
static void draw_status(uint16_t line) {
    uint16_t n,used,path,available;
    char text[129];
    output("Line "); output_number(line); output(" Col "); output_number(editor.column);
    output(editor.insert?" Insert ":" Overwrite "); output(editor.indent?"Indent ":" ");
    used=10+digits(line)+digits(editor.column)+(editor.insert?8:11)+(editor.indent?7:1);
    available=screen_width>used+(work.dirty?2:0)?screen_width-used-(work.dirty?2:0):0;
    path=(uint16_t)strlen(work.path); n=0;
    if(path>available&&available>=3) {
        text[n++]='.'; text[n++]='.'; text[n++]='.';
        path-=available-3;
    } else path=0;
    while(work.path[path]&&n<available&&n<128) text[n++]=work.path[path++];
    text[n]=0; output(text); if(work.dirty) output(" *");
}
static uint16_t draw_line(uint16_t p,uint8_t row,uint8_t selected) {
    char text[129];
    uint8_t n=0,col=0,shown=0,c,style=0,mark;
    screen_at(0,row);
    while(p<work.length) {
        c=far_get(work.page,p);
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
static void redraw(void) {
    uint16_t line,col,p=editor.top,top_line;
    uint8_t row,selected;
    source_location(&work,work.cursor,&line,&col);
    screen_mark(0); screen_clear(); draw_status(line);
    selected=editor.block_visible&&editor.block_marks==3&&editor.block_begin<editor.block_end;
    for(row=1;row<screen_height;++row) {
        p=draw_line(p,row,selected);
        if(p==work.length) break;
        ++p;
    }
    source_location(&work,editor.top,&top_line,&col);
    screen_at((uint8_t)(editor.column-1-editor.left),(uint8_t)(line-top_line+1));
}
static uint8_t suffix(uint8_t c) { return c>=1&&c<=26?c+'a'-1:c>='A'&&c<='Z'?c+32:c; }
static void edit(uint8_t at_error) {
    uint8_t result;
    uint32_t revision;
    editor_begin(&work,at_error,screen_width,screen_height); ide_state=2;
    for(;;) {
        redraw(); revision=work.revision;
        result=editor_key(key_read());
        if(result==EDITOR_READ_BLOCK||result==EDITOR_WRITE_BLOCK) block_io(result);
        if(result==EDITOR_FIND||result==EDITOR_REPLACE||result==EDITOR_REPEAT) search_command(result);
        if(work.revision!=revision) invalidate();
        if(result==EDITOR_DONE) return;
        if(result==EDITOR_REJECT||result==EDITOR_ABORT) {
            screen_at(0,screen_height-1);
            output(result==EDITOR_ABORT?"*** INTERRUPTED. Press Escape":"Edit rejected: line or source capacity. Press Escape");
            while(key_read()!=27) {}
        }
    }
}
static void diagnostic(uint8_t kind,uint16_t code,uint16_t position,uint16_t pc) {
    uint16_t line,col;
    ide_state=3;
    screen_clear();
    output(kind==0?"User break":kind==1?"Compiler error ":kind==2?"Runtime error ":"I/O error ");
    if(kind==1) output_number(code); else if(kind) output_hex(code,2);
    if(kind!=1) { output(" PC="); output_hex(pc,4); }
    source_location(&work,position,&line,&col);
    output("\n"); output(work.path); output(" Line "); output_number(line); output(" Col "); output_number(col);
    if(messages&&kind==2&&code==2) output(" Division by zero");
    output("\nPress Escape to return to source");
    while(key_read()!=27) {}
    work.cursor=position; edit(1);
}
static uint8_t save_work_as(const char *name) {
    uint8_t ok,c;
    screen_clear(); output("Saving source "); output(name?name:work.path); output("...\n");
    ok=name?save_source_as(&work,name,0):save_source(&work,0);
    if(!ok&&platform_error==15) {
        output("Existing BAK is unchanged.\nReplace BAK with the previous source (Y/N)? ");
        do { c=suffix(key_read()); } while(c!='y'&&c!='n'&&c!=27);
        if(c=='y') ok=name?save_source_as(&work,name,1):save_source(&work,1);
        else return 0;
    }
    if(ok) { output("Source saved: "); output(work.path); recovery_notice(); }
    else { file_error("Save failed, native error ",platform_error); output(". RAM source retained."); recovery_notice(); }
    output("\nPress Escape"); while(key_read()!=27) {}
    return ok;
}
static uint8_t save_work(void) { return save_work_as(0); }
static uint8_t file_field(char *text) {
    uint8_t c,n=0; char one[2]={0,0};
    text[0]=0;
    for(;;) {
        c=key_read();
        if(c==27||c==21) return c==21?2:0;
        if(c=='\n') return n!=0;
        if(c==127&&n) { text[--n]=0; output("\b \b"); }
        else if(c>=32&&c<127) {
            if(n==127) { output("\nFilename exceeds 127 bytes. Press Escape"); while(key_read()!=27) {} return 0; }
            text[n++]=c; text[n]=0; one[0]=c; output(one);
        }
    }
}
static void search_ack(const char *message) {
    output("\n"); output(message); output(". Press Escape");
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
                redraw(); screen_at(0,0); output("Replace (Y/N), Ctrl-U aborts: ");
                do { c=suffix(key_read()); } while(c!='y'&&c!='n'&&c!=21&&c!=27);
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
            ok=save_block(&work,editor.block_begin,editor.block_end,name,0);
            if(!ok&&platform_error==15) {
                output("\nOverwrite old file (Y/N), Escape cancels: ");
                do { c=suffix(key_read()); } while(c!='y'&&c!='n'&&c!=27&&c!=21);
                if(c==21) { output("\n*** INTERRUPTED. Press Escape"); while(key_read()!=27) {} return; }
                if(c==27) return;
                if(c=='n') continue;
                ok=save_block(&work,editor.block_begin,editor.block_end,name,1);
            }
            if(ok) return;
        } else {
            if(read_block(&block,name,&d)) {
                result=editor_insert_block(&block); source_close(&block);
                if(result==EDITOR_OK) return;
                output("\nBlock insertion rejected: line or source capacity. Press Escape");
                while(key_read()!=27) {} return;
            }
        }
        error=platform_error; output("\n");
        if(kind==EDITOR_READ_BLOCK&&d.code) file_error("Block source error ",d.code);
        else file_error("Block file error ",error);
        recovery_notice(); output(". Press Escape"); while(key_read()!=27) {} return;
    }
}
static void open_work(void) {
    char name[128]; uint8_t c; Diagnostic d;
    if(work.dirty) {
        output("Save current source first (Y/N), Escape cancels: ");
        do { c=suffix(key_read()); } while(c!='y'&&c!='n'&&c!=27);
        if(c==27||(c=='y'&&!save_work())) return;
    }
    screen_clear(); output("Open work file (for example B:/HELLO.PAS): ");
    if(file_field(name)!=1) return;
    output("\nLoading...\n");
    if(load_source(&work,name,&d)) { build_close(&build); editor_forget(); return; }
    file_error(d.code?"Source error ":"Native file error ",d.code?d.code:platform_error);
    output(". Current buffer retained. Press Escape"); while(key_read()!=27) {}
}
static void save_as_work(void) {
    char name[128];
    screen_clear(); output("Save source as (for example T:/untitled.pas): ");
    if(file_field(name)!=1) return;
    if(save_work_as(name)) invalidate();
}
static uint8_t compile_work(void) {
    Diagnostic d;
    build_close(&build); screen_clear(); output(saved_mode?"Compiling to native COM...\n":"Compiling to Memory...\n");
    if(!compile(&work,&build,&d)) { diagnostic(1,d.code,d.start,0); return 0; }
    if(saved_mode) {
        output("Verifying and saving executable/map...\n");
        if(!save_build(&work,&build)) {
            file_error("Output failed, native error ",platform_error);
            output(". RAM source retained."); recovery_notice();
            build_close(&build); output("\nPress Escape"); while(key_read()!=27) {} return 0;
        }
        output("Saved "); output(output_path()); output(" and .ZMP"); recovery_notice();
        output(work.dirty?"\nSource remains unsaved. Press Escape":"\nSource is saved. Press Escape"); while(key_read()!=27) {}
    }
    output("Code bytes: "); output_number(build.code_bytes);
    output(" Data bytes: "); output_number(build.globals_bytes); output("\n");
    return 1;
}
int app_main(void) {
    uint8_t c,status;
    uint16_t pc,position;
    Diagnostic startup;
    memset(&build,0,sizeof(build)); memset(build.pages,255,3); build.map_page=255;
    if(!console_open()) { output("Zeal Pascal needs the supported MMU machine and 80x24 PS/2 console.\n"); return 1; }
    if(!source_new(&work,DEFAULT_SOURCE)) { output("Not enough source memory\n"); console_close(); return 1; }
    screen_clear(); output("Zeal Pascal - first native increment\nExplanatory error messages (Y/N)? ");
    if(!load_source(&work,work.path,&startup)) {
        file_error("\nDefault source unavailable (error ",startup.code?startup.code:platform_error);
        output("); empty RAM buffer retained.\nExplanatory error messages (Y/N)? ");
    }
    do { c=suffix(key_read()); } while(c!='y'&&c!='n'); messages=c=='y';
    for(;;) {
        ide_state=1; screen_clear();
        output(saved_mode?"Zeal Pascal - native Com-file\nWork: ":"Zeal Pascal - Memory\nWork: "); output(work.path);
        output(work.dirty?" *\n":"\n"); output_number(work.length); output(" source bytes; ");
        output_number(pages_used()); output(" owned pages\nW Open  E Edit  S Save  A Save as  C Compile  R Run  O Options  Q Quit\n");
        output("Initial slice: Integer, String, Write/Writeln, ReadLn, Halt/Exit.\n");
        c=suffix(key_read());
        if(c=='e') edit(0);
        else if(c=='w') open_work();
        else if(c=='s') save_work();
        else if(c=='a') save_as_work();
        else if(c=='c') compile_work();
        else if(c=='o') {
            screen_clear(); output("Compiler options\nM Memory  C native Com-file  Q Return\n");
            do {
                c=suffix(key_read());
                if(c=='m'||c=='c') {
                    if(saved_mode!=(c=='c')) { saved_mode=c=='c'; build_close(&build); }
                    output(saved_mode?"Native Com-file selected\n":"Memory selected\n");
                }
            } while(c!='q');
        }
        else if(c=='r') {
            if((!build.valid||build.revision!=work.revision)&&!compile_work()) continue;
            screen_clear(); output("Validating and preparing program...\n");
            if(saved_mode) {
                if(!run_saved(&work,&build,&status)) file_error("Saved program failed validation/exec. Native error ",platform_error);
                else { output("\nProgram returned, status "); output_number(status); }
                output(". Press Escape"); while(key_read()!=27) {}
            } else if(!run_build(&work,&build)) { output("Native run failed; source retained. Press Escape"); while(key_read()!=27) {} }
            else if(ram_result[26]==6) {
                output("Native platform failure "); output_number(ram_result[28]);
                output(". Source retained. Press Escape"); while(key_read()!=27) {}
            } else if(ram_result[26]==3||ram_result[26]==5) {
                pc=ram_result[30]|((uint16_t)ram_result[31]<<8);
                if(build_locate(&work,&build,pc,&position)) diagnostic(ram_result[26]==5?0:ram_result[27],ram_result[28],position,pc);
                else { output("Source lookup failed. Press Escape"); while(key_read()!=27) {} }
            } else { output("\nProgram returned. Press Escape"); while(key_read()!=27) {} }
        } else if(c=='q') {
            if(work.dirty) {
                output("Save source before quitting (Y/N), Escape cancels: ");
                do { c=key_read(); } while(c!=27&&suffix(c)!='n'&&suffix(c)!='y');
                if(c==27) continue;
                if(suffix(c)=='y'&&!save_work()) continue;
            }
            break;
        }
    }
    build_close(&build); source_close(&work); console_close(); return 0;
}
