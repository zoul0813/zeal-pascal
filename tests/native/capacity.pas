program CodeCapacity;
label finish;
type big=array[1..109]of string[255];
const padding:big=(
'','','','','','','','','','',
'','','','','','','','','','',
'','','','','','','','','','',
'','','','','','','','','','',
'','','','','','','','','','',
'','','','','','','','','','',
'','','','','','','','','','',
'','','','','','','','','','',
'','','','','','','','','','',
'','','','','','','','','','',
'','','','','','','','','');
function late:integer;
begin late:=7 end;
begin
 if length(padding[109])<>0 then halt;
 if late<>7 then halt;
 goto finish;halt;
 finish:writeln('CAPACITY-OK');
end.
