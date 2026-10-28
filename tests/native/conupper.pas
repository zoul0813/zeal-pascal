program UpperConsole;
var padding:array[1..11000]of byte;f:text;c:char;saved:integer;
begin
 saved:=stackptr;padding[1]:=73;padding[11000]:=149;assign(f,'CON:');read(f,c);
 if(c<>'U')or(padding[1]<>73)or(padding[11000]<>149)then halt;
 if stackptr<>saved then halt;
 close(f);writeln('CON-UPPER-OK');
end.
