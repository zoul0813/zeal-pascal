program Options;
var pad:array[1..513]of byte;n,initial:integer;
const typed:integer=17;
function factorial(n:integer):integer;
begin
 if n=0 then factorial:=1 else factorial:=n*factorial(n-1)
end;
procedure setpad(var value:byte);
begin value:=7 end;
begin
 initial:=stackptr;
 if initial<>mem[$401e]+256*mem[$401f]+1 then halt;
 if addr(pad)<>$8000 then halt;
 if typed<>17 then halt;
 setpad(pad[513]);n:=factorial(6);
 if(pad[513]<>7)or(n<>720)then halt;
 if heapptr<mem[$401c]+256*mem[$401d]then halt;
 writeln('OPTIONS-OK')
end.
