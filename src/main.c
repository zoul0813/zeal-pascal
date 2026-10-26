#include "zp.h"

/* Public symbols also let the emulator acceptance driver inspect state without
 * adding a host protocol, test menu or debugger to the product. */
Source work;
Build build;
uint8_t ide_state; /* 1 menu, 2 editor, 3 error acknowledgment */
static uint8_t messages=1,insert_mode=1,indent_mode=1;
static uint8_t saved_mode;
static uint16_t virtual_column=1,top;

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
static uint16_t line_start(uint16_t p) {
    while(p&&far_get(work.page,p-1)!='\n') --p;
    return p;
}
static uint16_t line_end(uint16_t p) {
    while(p<work.length&&far_get(work.page,p)!='\n') ++p;
    return p;
}
static void column_sync(void) { virtual_column=work.cursor-line_start(work.cursor)+1; }
static void redraw(void) {
    uint16_t line,col,p=top,n;
    uint8_t row;
    char text[129];
    source_location(&work,work.cursor,&line,&col);
    if(work.cursor<top) top=line_start(work.cursor);
    p=top;
    screen_clear();
    output("Line "); output_number(line); output(" Col "); output_number(virtual_column);
    output(insert_mode?" Insert ":" Overwrite "); output(indent_mode?"Indent ":" ");
    output(work.path); if(work.dirty) output(" *");
    for(row=1;row<screen_height;++row) {
        n=0;
        while(p<work.length&&far_get(work.page,p)!='\n') {
            uint8_t c=far_get(work.page,p++);
            if(n<screen_width) text[n++]=c<32?c+64:(char)c;
        }
        text[n]=0; screen_at(0,row); output(text);
        if(p==work.length) break;
        ++p;
    }
    /* Scroll to the cursor by logical line, with the specified H-2 advance. */
    source_location(&work,top,&n,&col);
    if(line-n>=screen_height-2) {
        top=line_start(work.cursor);
        redraw(); return;
    }
    screen_at((uint8_t)((virtual_column-1)%screen_width),(uint8_t)(line-n+1));
}
static uint8_t suffix(uint8_t c) { return c>=1&&c<=26?c+'a'-1:c>='A'&&c<='Z'?c+32:c; }
static void edit(uint8_t at_error) {
    uint8_t c,prefix=0;
    uint16_t a,z,n,i;
    uint8_t text[128];
    if(!at_error) { work.previous=work.cursor; work.cursor=0; }
    top=line_start(work.cursor); column_sync(); ide_state=2;
    for(;;) {
        redraw(); c=key_read();
        if(prefix) {
            c=suffix(c);
            if(prefix==11&&c=='d') { work.previous=work.cursor; return; }
            if(prefix==17) {
                if(c=='s') work.cursor=line_start(work.cursor);
                if(c=='d') work.cursor=line_end(work.cursor);
                if(c=='r') work.cursor=0;
                if(c=='c') work.cursor=work.length;
                if(c=='p') work.cursor=work.previous;
                if(c=='i') indent_mode=!indent_mode;
                if(c=='y') {
                    z=line_end(work.cursor);
                    if(source_replace(&work,work.cursor,z,0,0)) invalidate();
                }
                column_sync();
            }
            prefix=0; continue;
        }
        if(c==11||c==17) { prefix=c; continue; }
        if(c==19) { if(virtual_column>1) --virtual_column; }
        else if(c==4) { if(virtual_column<128) ++virtual_column; }
        else if(c==5||c==24) {
            a=line_start(work.cursor);
            if(c==5&&a) a=line_start(a-1);
            else if(c==24&&line_end(a)<work.length) a=line_end(a)+1;
            work.cursor=a;
        } else if(c==0x80) { work.cursor=line_start(work.cursor); virtual_column=1; }
        else if(c==0x81) { work.cursor=line_end(work.cursor); column_sync(); }
        else if(c==22) insert_mode=!insert_mode;
        else if(c==127) {
            a=line_start(work.cursor); z=line_end(work.cursor);
            if(virtual_column>z-a+1) --virtual_column;
            else if(work.cursor&&source_replace(&work,work.cursor-1,work.cursor,0,0)) { invalidate(); column_sync(); }
        } else if(c==7) {
            if(work.cursor<line_end(work.cursor)&&source_replace(&work,work.cursor,work.cursor+1,0,0)) invalidate();
        } else if(c==25) {
            a=line_start(work.cursor); z=line_end(a); if(z<work.length) ++z;
            if(source_replace(&work,a,z,0,0)) { invalidate(); virtual_column=1; }
        } else if(c=='\n'||c==14||c>=32&&c<127) {
            a=line_start(work.cursor); z=line_end(work.cursor); n=0;
            for(i=z-a+1;i<virtual_column;++i) text[n++]=' ';
            text[n++]=c==14?'\n':c;
            if(c=='\n'&&indent_mode)
                for(i=a;i<z&&far_get(work.page,i)==' '&&n<128;++i) text[n++]=' ';
            a=work.cursor;
            if(!source_replace(&work,a,(!insert_mode&&c!='\n'&&c!=14&&a<z)?a+1:a,text,n)) {
                screen_at(0,screen_height-1); output("Edit rejected: line or source capacity. Press Escape");
                while(key_read()!=27) {}
            } else { invalidate(); if(c==14) work.cursor=a; column_sync(); }
        }
        a=line_start(work.cursor); z=line_end(a);
        work.cursor=a+virtual_column-1;
        if(work.cursor>z) work.cursor=z;
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
        if(c==27||c==21) return 0;
        if(c=='\n') return n!=0;
        if(c==127&&n) { text[--n]=0; output("\b \b"); }
        else if(c>=32&&c<127) {
            if(n==127) { output("\nFilename exceeds 127 bytes. Press Escape"); while(key_read()!=27) {} return 0; }
            text[n++]=c; text[n]=0; one[0]=c; output(one);
        }
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
    if(!file_field(name)) return;
    output("\nLoading...\n");
    if(load_source(&work,name,&d)) { build_close(&build); return; }
    file_error(d.code?"Source error ":"Native file error ",d.code?d.code:platform_error);
    output(". Current buffer retained. Press Escape"); while(key_read()!=27) {}
}
static void save_as_work(void) {
    char name[128];
    screen_clear(); output("Save source as (for example T:/untitled.pas): ");
    if(!file_field(name)) return;
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
