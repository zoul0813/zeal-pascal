#include "editor.h"

Editor editor;

static uint8_t byte(uint16_t p) { return far_get(editor.source->page,p); }
static uint16_t start(uint16_t p) {
    while(p&&byte(p-1)!='\n') --p;
    return p;
}
static uint16_t end(uint16_t p) {
    while(p<editor.source->length&&byte(p)!='\n') ++p;
    return p;
}
static uint16_t next(uint16_t p) {
    p=end(p); return p<editor.source->length?p+1:start(p);
}
static uint16_t previous(uint16_t p) { p=start(p); return p?start(p-1):0; }
static uint8_t delimiter(uint8_t c) {
    return c<=32||strchr("<>,;.()[]'*+-/$",c)!=0;
}
static void sync(void) { editor.column=editor.source->cursor-start(editor.source->cursor)+1; }
static void column_at(uint16_t line) {
    uint16_t z=end(line),p=line+editor.column-1;
    editor.source->cursor=p>z?z:p;
}
static uint16_t advance(uint16_t p,uint8_t count,uint8_t down) {
    while(count--) p=down?next(p):previous(p);
    return p;
}
static void view(void) {
    uint16_t p,line=start(editor.source->cursor),rows=0;
    if(editor.top>editor.source->length) editor.top=editor.source->length;
    editor.top=start(editor.top);
    if(line<editor.top) editor.top=line;
    p=editor.top;
    while(p<line) { p=next(p); ++rows; }
    while(rows>=editor.height-2) { editor.top=next(editor.top); --rows; }
    if(editor.column-1<editor.left) editor.left=editor.column-1;
    else if(editor.column-1>=editor.left+editor.width) editor.left=editor.column-editor.width;
}
void editor_forget(void) EDITOR_CALL {
    editor.source=0; editor.undo_valid=0; editor.prefix=editor.quote=0;
    editor.top=editor.left=0; editor.column=1;
    editor.block_begin=editor.block_end=editor.block_marks=0; editor.block_visible=1;
}
void editor_begin(Source *source,uint8_t at_error,uint8_t width,uint8_t height) EDITOR_CALL {
    if(!editor.initialized) { editor.insert=editor.indent=editor.initialized=1; }
    if(editor.source!=source||editor.page!=source->page) {
        editor_forget(); editor.source=source; editor.page=source->page;
    }
    editor.width=width?width:1; editor.height=height>=3?height:3;
    editor.prefix=editor.quote=0;
    if(source->cursor>source->length) source->cursor=source->length;
    if(!at_error) { source->previous=source->cursor; source->cursor=0; }
    sync();
    if(editor.undo_valid&&editor.undo_start!=start(source->cursor)) editor.undo_valid=0;
    editor.top=start(source->cursor); editor.left=0; view();
}
static void rebase(uint16_t a,uint16_t z,uint16_t n) {
    if(editor.block_marks&1) editor.block_begin=source_rebase(editor.block_begin,a,z,n,1);
    if(editor.block_marks&2) editor.block_end=source_rebase(editor.block_end,a,z,n,0);
    editor.top=source_rebase(editor.top,a,z,n,0);
}
static uint8_t replace(uint16_t a,uint16_t z,const uint8_t *text,uint16_t n,uint8_t remember) {
    uint8_t before[128],length=0,save,structural=0;
    uint16_t line=start(editor.source->cursor),p,zline=end(line);
    for(p=a;p<z&&!structural;++p) structural=byte(p)=='\n';
    for(p=0;p<n&&!structural;++p) structural=text[p]=='\n';
    save=remember&&!editor.undo_valid&&!structural;
    if(save) for(p=line;p<zline;++p) before[length++]=byte(p);
    if(!source_replace(editor.source,a,z,text,n)) return EDITOR_REJECT;
    if(structural) editor.undo_valid=0;
    if(save) {
        memcpy(editor.undo,before,length); editor.undo_length=length;
        editor.undo_start=line; editor.undo_valid=1;
    }
    rebase(a,z,n);
    return EDITOR_OK;
}
static uint8_t usable_block(void) {
    return editor.block_visible&&editor.block_marks==3&&editor.block_begin<editor.block_end
        &&editor.block_end<=editor.source->length;
}
/* Copy and read share snapshot insertion, virtual-space materialization,
 * before-line ownership and marker/cursor adoption. Text is copied, not moved
 * out of the input page; the caller still owns an external block after return. */
static uint8_t copy_block(const Source *block,uint16_t a,uint16_t z) {
    Source *s=editor.source;
    uint8_t before[128],length=0,structural=0,save;
    uint16_t p=s->cursor,line=start(p),i,spaces=editor.column-1-(p-line),n=z-a;
    if(!n) {
        editor.block_begin=editor.block_end=p;
        editor.block_marks=3; editor.block_visible=1; return EDITOR_OK;
    }
    for(i=a;i<z&&!structural;++i) structural=far_get(block->page,i)=='\n';
    save=!editor.undo_valid&&!structural;
    if(save) for(i=line;i<end(line);++i) before[length++]=byte(i);
    if(!source_insert_from(s,p,block,a,z,spaces)) return EDITOR_REJECT;
    editor.page=s->page;
    rebase(p,p,n+spaces);
    if(structural) editor.undo_valid=0;
    else if(save) {
        memcpy(editor.undo,before,length); editor.undo_length=length;
        editor.undo_start=line; editor.undo_valid=1;
    }
    editor.block_begin=p+spaces; editor.block_end=p+spaces+n;
    editor.block_marks=3; editor.block_visible=1;
    s->cursor=editor.block_begin; sync();
    return EDITOR_OK;
}
uint8_t editor_insert_block(const Source *block) EDITOR_CALL {
    uint8_t result=copy_block(block,0,block->length);
    view(); return result;
}
static uint8_t block_command(uint8_t c) {
    Source *s=editor.source;
    uint16_t p=s->cursor,a=editor.block_begin,z=editor.block_end,d,spaces,n;
    if(c=='b'||c=='k') {
        if(c=='b') { editor.block_begin=p; editor.block_marks|=1; }
        else { editor.block_end=p; editor.block_marks|=2; }
        editor.block_visible=1;
    } else if(c=='h') editor.block_visible=!editor.block_visible;
    else if(c=='t') {
        if(p==s->length&&p) --p;
        while(p&&delimiter(byte(p))) --p;
        if(p<s->length&&!delimiter(byte(p))) {
            a=p; while(a&&!delimiter(byte(a-1))) --a;
            z=p; while(z<s->length&&!delimiter(byte(z))) ++z;
            editor.block_begin=a; editor.block_end=z;
            editor.block_marks=3; editor.block_visible=1;
        }
    } else if(c=='r') return EDITOR_READ_BLOCK;
    else if(usable_block()) {
        if(c=='w') return EDITOR_WRITE_BLOCK;
        if(c=='c') return copy_block(s,a,z);
        if(c=='v') {
            n=z-a; spaces=editor.column-1-(p-start(p));
            if((p>=a&&p<z)||(p==z&&!spaces)) return EDITOR_OK;
            if(!source_move(s,a,z,p,spaces)) return EDITOR_REJECT;
            editor.page=s->page;
            /* The view follows the same deletion/insertion boundaries as the
             * remembered position. Explicit selection adoption follows it. */
            d=p>=z?p-n:p;
            rebase(a,z,0); rebase(d,d,n+spaces);
            editor.block_begin=d+spaces; editor.block_end=d+spaces+n;
            s->cursor=editor.block_begin; sync(); editor.undo_valid=0;
        } else if(c=='y') {
            d=source_rebase(p,a,z,0,0);
            if(replace(a,z,0,0,0)!=EDITOR_OK) return EDITOR_REJECT;
            s->cursor=d; sync(); editor.undo_valid=0;
        }
    }
    return EDITOR_OK;
}
static uint8_t insert(uint8_t c,uint8_t literal) {
    uint8_t text[256],result;
    uint16_t line=start(editor.source->cursor),z=end(line),p=editor.source->cursor,n=0,i;
    for(i=z-line+1;i<editor.column;++i) text[n++]=' ';
    if(c==13) c='\n';
    text[n++]=c;
    if(c=='\n'&&!literal&&editor.indent) {
        for(i=line;i<p&&byte(i)==' ';++i) text[n++]=' ';
        if(i==p) for(i=z-line+1;i<editor.column;++i) text[n++]=' ';
    }
    result=replace(p,!editor.insert&&c!='\n'&&p<z?p+1:p,text,n,1);
    if(!result) sync();
    return result;
}
static void word_left(void) {
    uint16_t p=editor.source->cursor,line=start(p);
    if(p==line) { if(p) editor.source->cursor=p-1; }
    else {
        while(p>line&&delimiter(byte(p-1))) --p;
        while(p>line&&!delimiter(byte(p-1))) --p;
        editor.source->cursor=p;
    }
    sync();
}
static void word_right(void) {
    uint16_t p=editor.source->cursor,z=end(p);
    if(p==z) {
        while(p<editor.source->length&&delimiter(byte(p))) ++p;
    }
    else {
        while(p<z&&!delimiter(byte(p))) ++p;
        while(p<z&&delimiter(byte(p))) ++p;
    }
    editor.source->cursor=p; sync();
}
static uint8_t delete_word(void) {
    uint16_t p=editor.source->cursor,z=p;
    uint8_t kind;
    if(p==editor.source->length) return EDITOR_OK;
    if(byte(p)=='\n') ++z;
    else {
        kind=delimiter(byte(p));
        while(z<editor.source->length&&byte(z)!='\n'&&delimiter(byte(z))==kind) ++z;
    }
    return replace(p,z,0,0,1);
}
static uint8_t tab(void) {
    uint16_t line=start(editor.source->cursor),p,z,first=0,target=0,col;
    uint8_t text[127],result;
    if(!line) return EDITOR_OK;
    p=previous(line); z=end(p); line=p;
    while(p<z) {
        while(p<z&&delimiter(byte(p))) ++p;
        if(p==z) break;
        col=p-line+1; if(!first) first=col;
        if(!target&&col>editor.column) target=col;
        while(p<z&&!delimiter(byte(p))) ++p;
    }
    col=z-line+1;
    if(!target&&col>editor.column) target=col;
    if(!target) target=first?first:col;
    if(target<=editor.column) { editor.column=target; column_at(start(editor.source->cursor)); return EDITOR_OK; }
    /* One transaction for the entire automatic tab, including virtual space. */
    line=start(editor.source->cursor); p=editor.source->cursor; z=end(p);
    col=target-1-(p-line);
    memset(text,' ',col);
    result=replace(p,!editor.insert&&p+col<z?p+col:!editor.insert?z:p,text,col,1);
    if(!result) sync();
    return result;
}
static uint8_t command(uint8_t c) {
    Source *s=editor.source;
    uint16_t a,z,p;
    uint8_t result=EDITOR_OK;
    if(c==19||c==8) { if(editor.column>1) --editor.column; column_at(start(s->cursor)); }
    else if(c==4) { if(editor.column<128) ++editor.column; column_at(start(s->cursor)); }
    else if(c==1) word_left();
    else if(c==6) word_right();
    else if(c==5||c==24) column_at(c==5?previous(s->cursor):next(s->cursor));
    else if(c==18||c==3) {
        column_at(advance(start(s->cursor),editor.height-2,c==3));
        editor.top=advance(editor.top,editor.height-2,c==3);
    } else if(c==23||c==26) {
        editor.top=c==23?previous(editor.top):next(editor.top);
        p=advance(editor.top,editor.height-3,1);
        if(s->cursor<editor.top) column_at(editor.top);
        else if(start(s->cursor)>p) column_at(p);
    } else if(c==0x80) { s->cursor=start(s->cursor); sync(); }
    else if(c==0x81) { s->cursor=end(s->cursor); sync(); }
    else if(c==22) editor.insert=!editor.insert;
    else if(c==16) editor.quote=1;
    else if(c==9) result=tab();
    else if(c==127) {
        a=start(s->cursor); z=end(a);
        if(editor.column>z-a+1) --editor.column;
        else if(s->cursor) { result=replace(s->cursor-1,s->cursor,0,0,1); if(!result) sync(); }
    } else if(c==7) {
        p=s->cursor;
        if(p<end(p)) { result=replace(p,p+1,0,0,1); s->cursor=p; }
    } else if(c==25) {
        a=start(s->cursor); z=end(a); if(z<s->length) ++z;
        result=replace(a,z,0,0,0);
        if(!result) { editor.column=1; editor.undo_valid=0; }
    } else if(c==20) { result=delete_word(); if(!result) sync(); }
    else if(c==14) {
        p=start(s->cursor); a=editor.column;
        result=insert('\n',1); if(!result) { editor.column=a; column_at(p); }
    } else if(c=='\n'||c==13||(c>=32&&c<127)) result=insert(c,0);
    return result;
}
static uint8_t quick(uint8_t c) {
    Source *s=editor.source;
    uint16_t p=s->cursor;
    if(c=='s') { s->cursor=start(p); sync(); }
    else if(c=='d') { s->cursor=end(p); sync(); }
    else if(c=='r') { s->cursor=0; sync(); }
    else if(c=='c') { s->cursor=s->length; sync(); }
    else if(c=='p') { s->cursor=s->previous>s->length?s->length:s->previous; sync(); }
    else if(c=='b'&&(editor.block_marks&1)) { s->cursor=editor.block_begin; sync(); }
    else if(c=='k'&&(editor.block_marks&2)) { s->cursor=editor.block_end; sync(); }
    else if(c=='e'||c=='x') column_at(c=='e'?editor.top:advance(editor.top,editor.height-3,1));
    else if(c=='i') editor.indent=!editor.indent;
    else if(c=='y') return replace(p,end(p),0,0,1);
    else if(c=='l'&&editor.undo_valid) {
        uint8_t result;
        uint16_t col=editor.column;
        p=start(p);
        result=replace(p,end(p),editor.undo,editor.undo_length,0);
        if(!result) { editor.column=col; column_at(p); }
        return result;
    }
    return EDITOR_OK;
}
uint8_t editor_key(uint8_t c) EDITOR_CALL {
    uint16_t old_line=start(editor.source->cursor);
    uint8_t prefix=editor.prefix,result=EDITOR_OK;
    if(editor.quote) {
        editor.quote=0;
        if(c==26||c>127) result=EDITOR_REJECT;
        else result=insert(c,1);
    } else if(c==21) { editor.prefix=0; return EDITOR_ABORT; }
    else if(prefix) {
        editor.prefix=0;
        if(c>=1&&c<=26) c+='a'-1;
        else if(c>='A'&&c<='Z') c+=32;
        if(prefix==11&&c=='d') { editor.source->previous=editor.source->cursor; return EDITOR_DONE; }
        if(prefix==11) result=block_command(c);
        if(prefix==17) result=quick(c);
    } else if(c==11||c==17) editor.prefix=c;
    else result=command(c);
    if(old_line!=start(editor.source->cursor)) editor.undo_valid=0;
    view();
    return result;
}
