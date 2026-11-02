#include "editor.h"
void check(uint8_t ok);
static void load(Source *s,const char *text) {
    Diagnostic d;
    check(source_import(s,(const uint8_t*)text,(uint16_t)strlen(text),&d));
    memset(&editor,0,sizeof(editor)); editor_begin(s,0,80,24);
}
static void text(Source *s,const char *expected) {
    uint16_t i,n=(uint16_t)strlen(expected);
    uint8_t ok=s->length==n;
    for(i=0;i<n&&ok;++i) ok=buffer_get(s->page,i)==(uint8_t)expected[i];
    check(ok);
}
/* Capture literals before entering a different code bank. The product's
 * prompt buffers already live on its retained stack. */
static uint8_t setup(const uint8_t *p,uint8_t plen,const uint8_t *r,uint8_t rlen,
    const uint8_t *o,uint8_t olen,uint8_t replacing) {
    uint8_t pattern[31],replacement[31],options[31];
    if(plen&&plen<=31) memcpy(pattern,p,plen);
    if(rlen&&rlen<=31) memcpy(replacement,r,rlen);
    if(olen&&olen<=31) memcpy(options,o,olen);
    return editor_search_configure(pattern,plen,replacement,rlen,options,olen,replacing);
}
static void config(const char *pattern,const char *replacement,const char *options,uint8_t replacing) {
    check(setup((const uint8_t*)pattern,(uint8_t)strlen(pattern),
        (const uint8_t*)replacement,(uint8_t)strlen(replacement),
        (const uint8_t*)options,(uint8_t)strlen(options),replacing));
}
static uint8_t run(EditorSearchJob *job,const char *answers) {
    uint16_t guard=0;
    uint8_t result;
    check(editor_search_begin(job));
    do {
        result=editor_search_step(job);
        if(result==SEARCH_MATCH) {
            uint8_t yes=!answers||*answers!='n';
            if(answers&&*answers) ++answers;
            result=editor_search_accept(job,yes);
        }
    } while(result==SEARCH_MORE&&++guard<60000);
    check(guard<60000); return result;
}
static void field_key(EditorField *f,uint8_t c) {
    check(editor_field_key(f,c,0,0)==FIELD_MORE);
}
void editor_search_checks(Source *s) {
    static const char sample[]="alpha beta\nBETA alpha\nalphabet beta\n";
    static const char *const options[]={"0","N","X","1U","4294967296","G000","U-1","1 2","2U1","2,3","65538","131074"};
    static const uint16_t counts[]={1,1,1,1,1,1,1,12,21,23,2,2};
    EditorSearchJob job;
    EditorField field,saved_field;
    uint8_t recall[10];
    Source saved;
    uint32_t revision;
    uint16_t i;
    uint8_t result;
    load(s,"foo\tbar baz\n");
    check(editor_key(11)==EDITOR_OK); check(editor_key('t')==EDITOR_OK);
    check(editor.block_begin==0&&editor.block_end==7);
    check(editor_key(6)==EDITOR_OK); check(s->cursor==8);
    check(editor_key(1)==EDITOR_OK); check(s->cursor==0);
    config("foo","","W",0); check(run(&job,0)==SEARCH_MISSING);
    load(s,sample); config("beta","","",0); revision=s->revision;
    check(run(&job,0)==SEARCH_DONE); check(s->cursor==10&&s->previous==0);
    check(run(&job,0)==SEARCH_DONE); check(s->cursor==35&&s->previous==10);
    check(!s->dirty&&s->revision==revision); text(s,sample);
    check(editor_key(17)==EDITOR_OK); check(editor_key('p')==EDITOR_OK); check(s->cursor==10);
    s->cursor=0; config("beta","","U2",0); check(run(&job,0)==SEARCH_DONE); check(s->cursor==15);
    s->cursor=s->length; config("beta","","B",0); check(run(&job,0)==SEARCH_DONE); check(s->cursor==31);
    config("alpha","","GW",0); check(run(&job,0)==SEARCH_DONE); check(s->cursor==5);
    s->cursor=s->length; config("beta","","G",0); check(run(&job,0)==SEARCH_DONE); check(s->cursor==10);
    check(run(&job,0)==SEARCH_DONE); check(s->cursor==10); /* Repeat reuses G. */
    config("b\1ta","","G",0); check(run(&job,0)==SEARCH_DONE); check(s->cursor==10);
    config("missing","","G",0); saved=*s; check(run(&job,0)==SEARCH_MISSING);
    check(s->cursor==saved.cursor&&s->revision==saved.revision&&!s->dirty);
    for(i=0;i<sizeof(options)/sizeof(*options);++i) {
        config("beta","",options[i],0); check(editor.search.count==counts[i]);
    }
    config("beta","","gguuw4294967295",0); check(editor.search.count==0xffffu);
    check(editor.search.flags==(SEARCH_GLOBAL|SEARCH_UPPER|SEARCH_WORD));
    check(!setup((const uint8_t*)"",0,0,0,0,0,0));
    config("a\r\nb\rc","\r\n", "",1);
    check(editor.search.length==5&&editor.search.replacement_length==1);
    check(!memcmp(editor.search.pattern,"a\nb\nc",5)&&editor.search.replacement[0]=='\n');
    check(!setup((const uint8_t*)"a",1,(const uint8_t*)"\32",1,0,0,1));
    {
        uint8_t long_pattern[31],zero=0,literal=1;
        memset(long_pattern,'a',sizeof(long_pattern));
        check(!setup(long_pattern,31,0,0,0,0,0));
        load(s,"a#b"); check(source_replace(s,1,2,&zero,1));
        check(setup(&zero,1,&literal,1,(const uint8_t*)"GN",2,1));
        check(run(&job,0)==SEARCH_DONE);
        check(s->length==3&&buffer_get(s->page,0)=='a'&&buffer_get(s->page,1)==1&&buffer_get(s->page,2)=='b');
    }

    load(s,sample); config("beta","Z","GNU",1); check(run(&job,0)==SEARCH_DONE);
    text(s,"alpha Z\nZ alpha\nalphabet Z\n"); check(s->dirty&&job.visited==3);
    load(s,"alpha beta\nalpha\n"); config("alpha","Z","",1);
    check(run(&job,"n")==SEARCH_DONE); text(s,"alpha beta\nalpha\n"); check(!s->dirty);
    s->cursor=0; check(run(&job,"y")==SEARCH_DONE); text(s,"Z beta\nalpha\n");
    load(s,"a a a a"); config("a","b","N2",1); check(run(&job,"ny")==SEARCH_DONE);
    text(s,"a b a a"); check(job.visited==2);
    load(s,"a a a"); config("a","z","GN1",1);
    check(run(&job,0)==SEARCH_DONE); text(s,"z z z"); check(job.visited==3);
    load(s,"aaaa"); config("a","aa","GN",1); check(run(&job,0)==SEARCH_DONE);
    text(s,"aaaaaaaa"); check(job.visited==4);
    load(s,"aaaa"); config("a","","GN",1); check(run(&job,0)==SEARCH_DONE); text(s,""); check(job.visited==4);
    load(s,"a a a"); config("a","","BGN",1); check(run(&job,0)==SEARCH_DONE); text(s,"  ");
    load(s,"aaa"); config("aa","b","BGN",1); check(run(&job,0)==SEARCH_DONE); text(s,"ab");
    load(s,"abc\ndef\n"); config("c\r\nd","X","GN",1); check(run(&job,0)==SEARCH_DONE); text(s,"abXef\n");
    load(s,"ab\nx"); config("b\1x","Y","GN",1); check(run(&job,0)==SEARCH_DONE); text(s,"aY");
    load(s,"one only_one one2 one\tONE"); config("one","X","GNUW",1);
    check(run(&job,0)==SEARCH_DONE); text(s,"X only_one one2 one\tONE");
    load(s,"aaaa"); config("aa","","G2",0); check(run(&job,0)==SEARCH_DONE); check(s->cursor==4);
    config("aa","","B",0); check(run(&job,0)==SEARCH_DONE); check(s->cursor==2);
    check(run(&job,0)==SEARCH_DONE); check(s->cursor==1); check(run(&job,0)==SEARCH_DONE); check(s->cursor==0);
    check(run(&job,0)==SEARCH_MISSING);

    /* A later rejected replacement retains previous accepted edits. */
    load(s,"a\naaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    config("a","aaa","GN",1); check(run(&job,0)==SEARCH_REJECT);
    check(s->dirty&&job.visited>0&&job.pending); check(s->length<=SOURCE_LIMIT);
    for(i=0;i<s->length;++i) if(buffer_get(s->page,i)=='\n') break;
    check(i==3); /* First line's edit survived the later127-byte line limit. */
    load(s,"a a"); config("a","z","G",1); check(editor_search_begin(&job));
    check(editor_search_step(&job)==SEARCH_MATCH); saved=*s;
    check(source_replace(s,s->length,s->length,(const uint8_t*)"!",1));
    check(editor_search_accept(&job,1)==SEARCH_REJECT); check(s->length==saved.length+1);
    load(s,"a a"); config("a","z","G",1); check(editor_search_begin(&job));
    check(editor_search_step(&job)==SEARCH_MATCH);
    config("q","r","",1); /* Job owns its configuration snapshot. */
    check(editor_search_accept(&job,1)==SEARCH_MORE); text(s,"z a");
    check(editor_search_step(&job)==SEARCH_MATCH); check(editor_search_accept(&job,0)==SEARCH_MORE);
    check(editor_search_step(&job)==SEARCH_DONE);

    editor_field_begin(&field); field_key(&field,'a'); field_key(&field,'b'); field_key(&field,'c');
    field_key(&field,19); field_key(&field,'X'); check(field.length==4&&!memcmp(field.text,"abXc",4));
    field_key(&field,127); check(field.length==3&&!memcmp(field.text,"abc",3));
    field_key(&field,7); check(field.length==2&&!memcmp(field.text,"ab",2));
    field_key(&field,0x80); field_key(&field,22); field_key(&field,'Z');
    check(field.length==2&&!memcmp(field.text,"Zb",2));
    field_key(&field,0x81); memcpy(recall,"old search",10);
    check(editor_field_key(&field,6,recall,10)==FIELD_MORE);
    check(field.length==10&&field.cursor==10&&!memcmp(field.text,"old search",10));
    field_key(&field,1); check(field.cursor==4);
    check(editor_field_key(&field,6,recall,10)==FIELD_MORE); check(field.cursor==10);
    field_key(&field,1); field_key(&field,8); check(field.cursor==3);
    editor_field_begin(&field);
    for(i=0;i<30;++i) field_key(&field,'x');
    saved_field=field; check(editor_field_key(&field,'y',0,0)==FIELD_REJECT);
    check(!memcmp(&field,&saved_field,sizeof(field)));
    editor_field_begin(&field); field_key(&field,16); field_key(&field,1);
    field_key(&field,16); field_key(&field,13); field_key(&field,16); field_key(&field,0);
    check(field.length==3&&field.text[0]==1&&field.text[1]==13&&field.text[2]==0);
    check(editor_field_key(&field,21,0,0)==FIELD_ABORT);
    check(editor_field_key(&field,27,0,0)==FIELD_CANCEL);
    check(editor_field_key(&field,10,0,0)==FIELD_DONE);
    result=editor_key(17); check(result==EDITOR_OK); check(editor_key('f')==EDITOR_FIND);
    check(editor_key(17)==EDITOR_OK); check(editor_key('a')==EDITOR_REPLACE);
    check(editor_key(12)==EDITOR_REPEAT);
}
