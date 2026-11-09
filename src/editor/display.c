#include "editor/display.h"

static uint8_t digits(uint16_t value) {
    uint8_t count=1;
    while(value>=10) { value/=10; ++count; }
    return count;
}
void main_draw_status(uint16_t line,uint8_t clear_tail) SOURCE_BLOCK_CALL {
    const Source *source=editor.source;
    uint16_t n,used,path,available;
    char text[129];
    output("Line "); output_number(line); output(" Col "); output_number(editor.column);
    output(editor.insert?" Insert ":" Overwrite "); output(editor.indent?"Indent ":" ");
    used=10+digits(line)+digits(editor.column)+(editor.insert?8:11)+(editor.indent?7:1);
    available=screen_width>used+(source->dirty?2:0)?screen_width-used-(source->dirty?2:0):0;
    path=(uint16_t)strlen(source->path); n=0;
    if(path>available&&available>=3) {
        text[n++]='.'; text[n++]='.'; text[n++]='.';
        path-=available-3;
    } else path=0;
    while(source->path[path]&&n<available&&n<128) text[n++]=source->path[path++];
    text[n]=0; output(text); if(source->dirty) output(" *");
    if(clear_tail) {
        /* A partial repaint has no screen clear to erase an older, longer
         * status. Reuse the owned buffer, including on wider consoles. */
        used+=n+(source->dirty?2:0);
        while(used<screen_width) {
            n=screen_width-used; if(n>128)n=128;
            memset(text,' ',n); text[n]=0; output(text); used+=n;
        }
    }
}
