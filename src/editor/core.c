#include "editor/editor.h"

Editor editor;

static uint8_t byte(uint16_t p) { return buffer_get(editor.source->page,p); }
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
    return c==32||c==10||(c&&strchr("<>,;.()[]'*+-/$",c)!=0);
}
static void locate(uint16_t p);
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
static void locate(uint16_t p) {
    if(start(editor.source->cursor)!=start(p)) editor.undo_valid=0;
    editor.source->cursor=p; sync(); view();
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
    for(i=a;i<z&&!structural;++i) structural=buffer_get(block->page,i)=='\n';
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
    else if(c=='f') return EDITOR_FIND;
    else if(c=='a') return EDITOR_REPLACE;
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
    else if(c==12&&editor.search.valid) result=EDITOR_REPEAT;
    else result=command(c);
    if(old_line!=start(editor.source->cursor)) editor.undo_valid=0;
    view();
    return result;
}

static uint8_t normalize(uint8_t *out,const uint8_t *text,uint8_t length) {
    uint8_t i,n=0,c;
    for(i=0;i<length;++i) {
        c=text[i];
        if(c=='\r') { c='\n'; if(i+1<length&&text[i+1]=='\n') ++i; }
        out[n++]=c;
    }
    return n;
}
uint8_t editor_search_configure(const uint8_t *pattern,uint8_t length,const uint8_t *replacement,
    uint8_t replacement_length,const uint8_t *options,uint8_t options_length,uint8_t replacing) EDITOR_CALL {
    EditorSearch next;
    uint8_t i,c;
    uint16_t count=0;
    if(!length||length>30||replacement_length>30||options_length>30) return 0;
    memset(&next,0,sizeof(next));
    next.length=normalize(next.pattern,pattern,length);
    next.replacement_length=normalize(next.replacement,replacement,replacement_length);
    for(i=0;i<next.replacement_length;++i) if(next.replacement[i]==26) return 0;
    /* TP3 scans every byte: flags can occur anywhere, unrecognized bytes are
     * ignored, and all decimal digits contribute to one wrapping word. */
    for(i=0;i<options_length;++i) {
        c=options[i]; if(c>='A'&&c<='Z') c+=32;
        if(c>='0'&&c<='9') count=(uint16_t)(count*10u+c-'0');
        else if(c=='b') next.flags|=SEARCH_BACK;
        else if(c=='g') next.flags|=SEARCH_GLOBAL;
        else if(c=='u') next.flags|=SEARCH_UPPER;
        else if(c=='w') next.flags|=SEARCH_WORD;
        else if(c=='n'&&replacing) next.flags|=SEARCH_NO_CONFIRM;
    }
    next.count=replacing&&(next.flags&SEARCH_GLOBAL)?0xffffu:count?count:1;
    next.replacing=replacing!=0; next.valid=1;
    editor.search=next; return 1;
}
uint8_t editor_search_begin(EditorSearchJob *job) EDITOR_CALL {
    Source *s=editor.source;
    memset(job,0,sizeof(*job));
    if(!editor.search.valid) return 0;
    job->search=editor.search; job->source=s; job->page=s->page; job->revision=s->revision;
    job->remaining=job->search.count; job->active=1;
    s->previous=s->cursor;
    job->point=(job->search.flags&SEARCH_GLOBAL)?
        ((job->search.flags&SEARCH_BACK)?s->length:0):s->cursor;
    if(job->search.flags&SEARCH_BACK) {
        if(!job->point) job->exhausted=1;
        else --job->point;
        if(s->length>=job->search.length&&job->point>s->length-job->search.length)
            job->point=s->length-job->search.length;
    }
    return 1;
}
static uint8_t current_job(const EditorSearchJob *job) {
    return job->source==editor.source&&job->page==editor.source->page&&job->revision==editor.source->revision;
}
static uint8_t upper(uint8_t c) { return c>='a'&&c<='z'?c-32:c; }
static uint8_t matches(const EditorSearchJob *job,uint16_t p) {
    uint8_t i,a,b;
    for(i=0;i<job->search.length;++i) {
        a=job->search.pattern[i]; b=byte(p+i);
        if(a==1) continue;
        if(job->search.flags&SEARCH_UPPER) { a=upper(a); b=upper(b); }
        if(a!=b) return 0;
    }
    if(job->search.flags&SEARCH_WORD) {
        if(p&&!delimiter(byte(p-1))) return 0;
        p+=job->search.length;
        if(p<editor.source->length&&!delimiter(byte(p))) return 0;
    }
    return 1;
}
static void search_next(EditorSearchJob *job,uint16_t p,uint8_t length) {
    if(job->search.flags&SEARCH_BACK) {
        if(!p) job->exhausted=1;
        else job->point=p-1;
    } else job->point=p+length;
}
uint8_t editor_search_step(EditorSearchJob *job) EDITOR_CALL {
    uint8_t budget=64;
    uint16_t p;
    if(!current_job(job)) return SEARCH_REJECT;
    if(!job->active) return SEARCH_DONE;
    if(job->pending) return SEARCH_MATCH;
    while(budget--) {
        if(job->exhausted||editor.source->length<job->search.length||
           job->point>editor.source->length-job->search.length) {
            job->active=0;
            return job->search.replacing&&job->visited?SEARCH_DONE:SEARCH_MISSING;
        }
        p=job->point;
        if(matches(job,p)) {
            if(job->search.replacing) {
                job->match=p; job->pending=1;
                locate((job->search.flags&SEARCH_BACK)?p:p+job->search.length);
                return SEARCH_MATCH;
            }
            ++job->visited;
            if(!--job->remaining) {
                locate((job->search.flags&SEARCH_BACK)?p:p+job->search.length);
                job->active=0; return SEARCH_DONE;
            }
            search_next(job,p,job->search.length);
        } else if(job->search.flags&SEARCH_BACK) {
            if(!p) job->exhausted=1;
            else --job->point;
        } else ++job->point;
    }
    return SEARCH_MORE;
}
uint8_t editor_search_accept(EditorSearchJob *job,uint8_t yes) EDITOR_CALL {
    uint16_t p=job->match;
    uint8_t n=job->search.length;
    if(!current_job(job)||!job->pending||!job->search.replacing) return SEARCH_REJECT;
    if(yes) {
        if(replace(p,p+n,job->search.replacement,job->search.replacement_length,1)!=EDITOR_OK)
            return SEARCH_REJECT;
        n=job->search.replacement_length;
        job->revision=editor.source->revision;
        locate((job->search.flags&SEARCH_BACK)?p:p+n);
    }
    ++job->visited; job->pending=0;
    search_next(job,p,n);
    if(!--job->remaining) { job->active=0; return SEARCH_DONE; }
    return SEARCH_MORE;
}
void editor_field_begin(EditorField *field) EDITOR_CALL {
    memset(field,0,sizeof(*field)); field->insert=1;
}
uint8_t editor_field_key(EditorField *f,uint8_t c,const uint8_t *recall,uint8_t recall_length) EDITOR_CALL {
    uint8_t p=f->cursor,i,quoted=f->quote;
    if(quoted) f->quote=0;
    else if(c==16) { f->quote=1; return FIELD_MORE; }
    else if(c==21) return FIELD_ABORT;
    else if(c==27) return FIELD_CANCEL;
    else if(c==10||c==13) return FIELD_DONE;
    else if(c==19||c==8) { if(p) --f->cursor; return FIELD_MORE; }
    else if(c==4) { if(p<f->length) ++f->cursor; return FIELD_MORE; }
    else if(c==0x80) { f->cursor=0; return FIELD_MORE; }
    else if(c==0x81) { f->cursor=f->length; return FIELD_MORE; }
    else if(c==1) {
        while(p&&delimiter(f->text[p-1])) --p;
        while(p&&!delimiter(f->text[p-1])) --p;
        f->cursor=p; return FIELD_MORE;
    } else if(c==6) {
        if(p==f->length&&recall_length) {
            if(recall_length>30) return FIELD_REJECT;
            memcpy(f->text,recall,recall_length); f->length=f->cursor=recall_length;
        } else {
            while(p<f->length&&!delimiter(f->text[p])) ++p;
            while(p<f->length&&delimiter(f->text[p])) ++p;
            f->cursor=p;
        }
        return FIELD_MORE;
    } else if(c==22) { f->insert=!f->insert; return FIELD_MORE; }
    else if(c==25) { f->cursor=f->length=0; return FIELD_MORE; }
    else if(c==7||c==127) {
        if(c==127) { if(!p) return FIELD_MORE; --p; f->cursor=p; }
        if(p<f->length) {
            --f->length; for(i=p;i<f->length;++i) f->text[i]=f->text[i+1];
        }
        return FIELD_MORE;
    } else if(c<32||c>126) return FIELD_MORE;
    if(c>127) return FIELD_REJECT;
    if((f->insert||p==f->length)&&f->length==30) return FIELD_REJECT;
    if(f->insert||p==f->length) {
        for(i=f->length;i>p;--i) f->text[i]=f->text[i-1];
        ++f->length;
    }
    f->text[p]=c; f->cursor=p+1; return FIELD_MORE;
}
