/* The same behavioral cases execute on the host and in an actual Zeal bank. */
#include "editor.h"

void check(uint8_t ok);
static void key(uint8_t c) { check(editor_key(c)==EDITOR_OK); }
static void quick(uint8_t c) { key(17); key(c); }
static void position(Source *s,uint16_t line,uint16_t col) {
    uint16_t actual_line,actual_col;
    source_location(s,s->cursor,&actual_line,&actual_col);
    check(actual_line==line); check(editor.column==col);
    check(actual_col<=col);
}
static void text(Source *s,const char *expected) {
    uint16_t i,n=(uint16_t)strlen(expected);
    uint8_t same=s->length==n;
    for(i=0;i<n&&same;++i) same=far_get(s->page,i)==(uint8_t)expected[i];
    check(same);
}
static void load(Source *s,const char *value) {
    Diagnostic d;
    check(source_import(s,(const uint8_t*)value,(uint16_t)strlen(value),&d));
    memset(&editor,0,sizeof(editor)); editor_begin(s,0,80,24);
    check(editor.insert&&editor.indent&&editor.column==1);
}
static void filled(Source *s,uint16_t count,uint8_t lines) {
    SourceImport in;
    Diagnostic d;
    uint16_t i;
    uint8_t ok=import_begin(&in,s->path,&d);
    check(ok); if(!ok) return;
    for(i=0;i<count&&ok;++i) ok=import_byte(&in,lines&&i%128==127?'\n':'x',&d);
    check(ok); check(import_finish(&in,s,&d));
    memset(&editor,0,sizeof(editor)); editor_begin(s,0,80,24);
}
void editor_checks(Source *s) {
    static const char original[]="alpha beta\ngamma delta\n  indented\nlast\n";
    uint16_t i,used=pages_used();
    uint32_t revision,version;
    Source saved;
    Editor state;
    uint8_t longline[128];
    load(s,original); revision=s->revision;
    key(4); key(4); position(s,1,3);
    key(6); position(s,1,7); key(1); position(s,1,1);
    quick('d'); position(s,1,11); key(24); position(s,2,11);
    quick('s'); key(5); position(s,1,1);
    quick('c'); position(s,5,1); quick('r'); position(s,1,1);
    quick('d'); key(6); position(s,2,1); key(1); position(s,1,11); key(1); position(s,1,7);
    key(8); position(s,1,6); check(!s->dirty&&s->revision==revision); text(s,original);
    key(11); check(editor_key('d')==EDITOR_DONE);
    editor_begin(s,0,80,24); quick('p'); position(s,1,6);
    s->cursor=12; editor_begin(s,1,80,24); position(s,2,2);

    load(s,original); key('X'); key(22); key('Z'); key(22); key(7); key(127);
    text(s,"Xpha beta\ngamma delta\n  indented\nlast\n");
    key(20); text(s,"X beta\ngamma delta\n  indented\nlast\n");
    quick('y'); text(s,"X\ngamma delta\n  indented\nlast\n");
    quick('l'); text(s,original); position(s,1,2); check(s->dirty);
    key(24); quick('l'); text(s,original);
    load(s,original); key('X'); key(24); quick('l'); text(s,"Xalpha beta\ngamma delta\n  indented\nlast\n");
    load(s,original); key('X'); key(25); quick('l'); text(s,"gamma delta\n  indented\nlast\n");
    load(s,original); quick('d'); key(20); text(s,"alpha betagamma delta\n  indented\nlast\n");
    quick('l'); text(s,"alpha betagamma delta\n  indented\nlast\n");
    load(s,"alpha beta\n  gamma\n"); quick('d'); key(6); position(s,2,3);
    load(s,"alpha\n\n  gamma\n"); quick('d'); key(6); position(s,3,3);
    load(s,"alpha beta\n  gamma\n"); for(i=0;i<6;++i) key(4); key(6); position(s,1,11);
    load(s,"alpha  beta,\n  gamma\n"); for(i=0;i<5;++i) key(4); key(20);
    text(s,"alphabeta,\n  gamma\n");
    load(s,"alpha beta,\n"); quick('d'); key(19); key(20); text(s,"alpha beta\n");
    load(s,"alpha beta\n"); for(i=0;i<5;++i) key(4); key(14); quick('l'); text(s,"alpha\n beta\n");

    load(s,original); key(9); position(s,1,1); check(!s->dirty);
    key(24); quick('s'); key(9); position(s,2,7);
    text(s,"alpha beta\n      gamma delta\n  indented\nlast\n");
    load(s,original); quick('c'); key(9); position(s,5,5);
    load(s,original); key(24); quick('d'); revision=s->revision; key(9);
    position(s,2,1); check(s->revision==revision);
    load(s,"alpha beta\n123456789"); key(24); key(22); key(9);
    text(s,"alpha beta\n      789"); position(s,2,7);

    load(s,"    abc"); key(4); key(4); key('\n'); text(s,"  \n    abc"); position(s,2,3);
    load(s,"    abc"); quick('i'); quick('d'); key('\n'); text(s,"    abc\n"); position(s,2,1);
    load(s,"x"); for(i=0;i<9;++i) key(4); key(14);
    text(s,"x        \n"); position(s,1,10); check(s->cursor==9);
    quick('l'); text(s,"x        \n"); position(s,1,10); check(s->cursor==9);
    load(s,""); for(i=1;i<128;++i) key(4); check(editor.left==48);
    key('\n'); check(s->length==255); position(s,2,128);
    check(far_get(s->page,127)=='\n'&&s->cursor==255);
    for(i=0;i<s->length;++i) if(i!=127) check(far_get(s->page,i)==' ');

    load(s,""); key(16); key(9); check(s->length==1&&far_get(s->page,0)==9);
    key(16); key(8); check(s->length==2&&far_get(s->page,1)==8);
    key(16); key(27); check(s->length==3&&far_get(s->page,2)==27);
    key(16); key(0); check(s->length==4&&far_get(s->page,3)==0);
    memcpy(&saved,s,sizeof(saved)); key(16); check(editor_key(26)==EDITOR_REJECT);
    check(!memcmp(&saved,s,sizeof(saved))&&!editor.quote);
    key(16); key(13); check(s->length==5&&far_get(s->page,4)=='\n');
    key(17); memcpy(&saved,s,sizeof(saved)); check(editor_key(21)==EDITOR_ABORT);
    check(!memcmp(&saved,s,sizeof(saved))&&!editor.prefix);

    memset(longline,'x',127); longline[127]=0; load(s,(const char*)longline); quick('d');
    memcpy(&saved,s,sizeof(saved)); memcpy(&state,&editor,sizeof(state)); version=page_versions[s->page];
    check(editor_key('z')==EDITOR_REJECT); check(!memcmp(&saved,s,sizeof(saved)));
    check(!memcmp(&state,&editor,sizeof(state))); check(page_versions[s->page]==version);
    load(s,"x\ny"); s->cursor=2; editor_begin(s,1,80,24); key(127); text(s,"xy"); position(s,1,2);
    {
        SourceImport in; Diagnostic d;
        check(import_begin(&in,s->path,&d));
        for(i=0;i<127;++i) check(import_byte(&in,'x',&d));
        check(import_byte(&in,'\n',&d)); check(import_byte(&in,'y',&d)); check(import_finish(&in,s,&d));
        editor_begin(s,0,80,24); key(24);
        memcpy(&saved,s,sizeof(saved)); memcpy(&state,&editor,sizeof(state));
        check(editor_key(127)==EDITOR_REJECT); check(!memcmp(&saved,s,sizeof(saved)));
        check(!memcmp(&state,&editor,sizeof(state)));
    }
    filled(s,SOURCE_LIMIT,1); quick('c'); version=page_versions[s->page];
    memcpy(&saved,s,sizeof(saved)); check(editor_key('\n')==EDITOR_REJECT);
    check(!memcmp(&saved,s,sizeof(saved))&&page_versions[s->page]==version);

    {
        SourceImport in; Diagnostic d;
        check(import_begin(&in,s->path,&d));
        for(i=0;i<50;++i) { check(import_byte(&in,'x',&d)); check(import_byte(&in,'\n',&d)); }
        check(import_finish(&in,s,&d)); memset(&editor,0,sizeof(editor)); editor_begin(s,0,80,24);
        revision=s->revision;
        key(3); position(s,23,1); check(editor.top==44);
        key(18); position(s,1,1); check(editor.top==0);
        key(26); position(s,2,1); check(editor.top==2);
        key(23); position(s,2,1); check(editor.top==0);
        quick('x'); position(s,22,1); quick('e'); position(s,1,1);
        for(i=0;i<22;++i) key(24);
        position(s,23,1); check(editor.top==2);
        quick('c'); position(s,51,1); key(24); position(s,51,1);
        key(23); position(s,50,1); key(26); position(s,50,1);
        check(!s->dirty&&s->revision==revision);
    }
    load(s,"short\nlonger"); for(i=1;i<128;++i) key(4);
    position(s,1,128); check(s->cursor==5&&editor.left==48);
    key(24); position(s,2,128); check(s->cursor==12&&editor.left==48);
    key(24); position(s,2,128); key(5); position(s,1,128);
    key(0x80); position(s,1,1); check(editor.left==0);
    key(0x81); position(s,1,6);
    key(22); quick('i');
    {
        Diagnostic d;
        check(source_import(s,(const uint8_t*)"new",3,&d));
        editor_begin(s,0,80,24); check(!editor.insert&&!editor.indent&&!editor.undo_valid);
        editor_forget(); editor_begin(s,0,80,24); check(!editor.insert&&!editor.indent);
    }
    load(s,"abc"); s->previous=2; key('X'); check(s->previous==3);
    quick('s'); key(7); check(s->previous==2); text(s,"abc");
    check(source_rebase(0,0,0,3,0)==0); check(source_rebase(0,0,0,3,1)==3);
    check(source_rebase(9,2,5,1,1)==7); check(source_rebase(4,2,5,1,0)==2);
    check(source_rebase(4,2,5,1,1)==3); check(pages_used()==used);
}
