program BridgeError;
type code=string[3];
const machine:code=#195#0#0;actual:integer=0;
{$A-}
function wire:integer;external $800A;
procedure outer;
var zero:integer;target:integer;
function callback:integer;
begin callback:=10 div zero end;
begin
 zero:=0;target:=addr(callback);machine[2]:=chr(lo(target));machine[3]:=chr(hi(target));
 actual:=wire;
end;
begin outer end.
