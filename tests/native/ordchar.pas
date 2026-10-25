program OrdinalCharacters;
type short=string[3];fixed=array[1..1]of char;
var s:short;a:fixed;i,total:integer;
begin
 s:='A';
 if(ord(s)=65)and(succ(s)='B')and(pred(s)='@')then writeln('ORDINAL-STRING-CHAR-OK');
 a[1]:='Q';
 if(ord(a)=81)and(succ(a)='R')and(pred(a)='P')then writeln('ORDINAL-CHARARRAY-OK');
 total:=0;
 for i:=1 to 100 do
  if(ord(copy('ZA',2,1))=65)and(succ(concat('','B'))='C')and
    (pred(copy('ZB',2,1))='A')then total:=total+1;
 if total=100 then writeln('ORDINAL-CHAR-TEMPORARY-CLEANUP-OK');
end.
