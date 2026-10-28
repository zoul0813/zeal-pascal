program InlineBytes;
const bytes:array[0..25]of byte=(0,255,0,1,255,255,255,255,0,1,52,68,0,0,0,0,128,0,128,0,2,255,255,1,0,128);
var code,saved,i:integer;
procedure sample;
begin
 inline($21/*+8/$22/code/$C3/*+28/
 0/255/256/-1/$FFFF/1+255/<$1234/>$44/1-1/$FFFF+1/32767+1/-32768/1+-1/1--1/+$FFFF/-$FFFF/-$8000)
end;
begin
 saved:=StackPtr;sample;
 if StackPtr<>saved then halt;
 for i:=0 to 25 do if Mem[code+i]<>bytes[i]then halt;
 writeln('INLINE-BYTES-OK')
end.
