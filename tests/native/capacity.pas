program CodeCapacity;
label finish;
{ Saved startup replaces one256-byte padding slot; late code remains aboveC000. }
type big=array[1..108]of string[255];
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
'','','','','','','','');
function late:integer;
begin late:=7 end;
begin
 if length(padding[108])<>0 then halt;
 if late<>7 then halt;
 goto finish;halt;
 finish:writeln('CAPACITY-OK');
end.
