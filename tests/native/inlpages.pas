program InlinePages;
var pad:array[1..1023]of byte;code:integer;
begin
{$I source-parts/inlnops.inc}
{$I source-parts/inlnops.inc}
{$I source-parts/inlnops.inc}
 inline($21/*+8/$22/code/$C3/*+7/*/<*/0-*);
 if(Mem[code]<>Lo(code))or(Mem[code+1]<>Hi(code))then halt;
 if Mem[code+2]<>Lo(code+2)then halt;
 if(Mem[code+3]<>Lo(-code-3))or(Mem[code+4]<>Hi(-code-3))then halt;
 writeln('INLINE-PAGES-OK')
end.
