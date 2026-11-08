#include "compiler/messages.h"
#include <assert.h>
#include <stdio.h>

static char text[128];
void output(const char *value) {
    assert(strlen(text)+strlen(value)<sizeof(text));strcat(text,value);
}
int main(void) {
    static const struct {uint8_t code;const char *text;} cases[]={
        {1,": ';' expected"},{41,": Unknown identifier or syntax error"},
        {45,": Constant out of range"},{77,": Overlays unavailable in Memory mode"},
        {90,": Source file unavailable"},{93,": Invalid compiler directive"},
        {98,": Memory overflow"},{99,": Compiler overflow"},
        {100,": Source line exceeds 127 bytes"},
        {0,""},{16,""},{19,""},{38,""},{39,""},{255,""}
    };
    unsigned i;
    for(i=0;i<sizeof(cases)/sizeof(cases[0]);++i) {
        text[0]=0;compiler_message(cases[i].code);assert(!strcmp(text,cases[i].text));
    }
    text[0]=0;compiler_message(255);compiler_message(1);
    assert(!strcmp(text,": ';' expected"));
    puts("PASS: 16 compiler-message and numeric-fallback cases");return 0;
}
