#include "keys.h"
#include <zos_keyboard.h>
#include <assert.h>
#include <stdio.h>

static unsigned checks;
static void expect(unsigned char raw,unsigned char wanted) {
    unsigned char actual=key_event(raw);
    ++checks;
    if(actual!=wanted) {fprintf(stderr,"key %u: %u != %u\n",raw,actual,wanted);assert(actual==wanted);}
}
static void release(unsigned char raw) {expect(KB_RELEASED,0);expect(raw,0);}
int main(void) {
    const char *plain="`1234567890-=[]\\;',./",*upper="~!@#$%^&*()_+{}|:\"<>?";
    unsigned i;
    expect('a','a');release('a');expect('a','a');
    expect(KB_LEFT_SHIFT,0);expect(KB_RIGHT_SHIFT,0);expect('a','A');
    release(KB_LEFT_SHIFT);expect('a','A');release(KB_RIGHT_SHIFT);expect('a','a');
    expect(KB_CAPS_LOCK,0);release(KB_CAPS_LOCK);expect('z','Z');
    expect(KB_LEFT_SHIFT,0);expect('z','z');release(KB_LEFT_SHIFT);
    expect(KB_CAPS_LOCK,0);release(KB_CAPS_LOCK);expect('z','z');
    expect(KB_RIGHT_SHIFT,0);
    for(i=0;plain[i];++i) expect((unsigned char)plain[i],(unsigned char)upper[i]);
    release(KB_RIGHT_SHIFT);
    expect(KB_LEFT_CTRL,0);expect(KB_RIGHT_CTRL,0);
    for(i=0;i<26;++i) expect('a'+i,1+i);
    release(KB_LEFT_CTRL);expect('k',11);release(KB_RIGHT_CTRL);expect('k','k');
    expect('\b',8);expect(127,127);
#ifdef ZP_PASCAL_KEYS
    expect('\n',13);
#else
    expect('\n',10);
#endif
    expect('\t',9);
#ifdef ZP_PASCAL_KEYS
    expect(KB_DELETE,127);release(KB_DELETE);
    expect(KB_ESC,27);release(KB_ESC);
#else
    expect(KB_DELETE,KB_DELETE);release(KB_DELETE);
    expect(KB_ESC,KB_ESC);release(KB_ESC);
#endif
    expect(KB_LEFT_ARROW,KB_LEFT_ARROW);release(KB_LEFT_ARROW);
    expect(0,0);expect(1,0);
#ifndef ZP_PASCAL_KEYS
    expect(KB_LEFT_CTRL,0);expect(KB_RIGHT_SHIFT,0);expect(KB_CAPS_LOCK,0);
    expect(KB_RELEASED,0);key_reset();expect('a','a');expect('1','1');
#endif
    printf("PASS: %u raw-key modifier/release/character checks\n",checks);
    return 0;
}
