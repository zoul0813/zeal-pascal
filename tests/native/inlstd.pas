program InlineStandard;
var code,saved:integer;
procedure sample;
begin
 inline($21/*+8/$22/code/$C3/*+16/
 Input/Output/HeapPtr/RecurPtr/BufLen/CBreak/ConOutPtr)
end;
function wordAt(offset:integer):integer;
begin wordAt:=Mem[code+offset]+256*Mem[code+offset+1]end;
begin
 saved:=StackPtr;sample;
 if StackPtr<>saved then halt;
 if(wordAt(0)<>Addr(Input))or(wordAt(2)<>Addr(Output))then halt;
 if(wordAt(4)<>Addr(HeapPtr))or(wordAt(6)<>Addr(RecurPtr))then halt;
 if(wordAt(8)<>Addr(BufLen))or(wordAt(10)<>Addr(CBreak))then halt;
 if wordAt(12)<>Addr(ConOutPtr)then halt;
 if(Mem[wordAt(0)]<>$5A)or(Mem[wordAt(0)+1]<>$46)then halt;
 writeln('INLINE-STANDARD-OK')
end.
