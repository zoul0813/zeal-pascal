program UpperConsole;
var padding:array[1..10600]of byte;f:text;c:char;saved:integer;
begin
 saved:=stackptr;padding[1]:=73;padding[10600]:=149;assign(f,'CON:');read(f,c);
 if(c<>'U')or(padding[1]<>73)or(padding[10600]<>149)then halt;
 if stackptr<>saved then halt;
 close(f);writeln('CON-UPPER-OK');
end.
