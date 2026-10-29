program UpperConsole;
{ Reserve saved startup while retaining an upper-bank CON callback. }
const padsize=9985;
var padding:array[1..padsize]of byte;f:text;c:char;saved:integer;
begin
 saved:=stackptr;padding[1]:=73;padding[padsize]:=149;assign(f,'CON:');read(f,c);
 if(c<>'U')or(padding[1]<>73)or(padding[padsize]<>149)then halt;
 if stackptr<>saved then halt;
 close(f);writeln('CON-UPPER-OK');
end.
