program BridgeGuard;
type code=string[3];
const machine:code=#195#0#0;actual:integer=0;
{$A-}
function wire(n:integer):integer;external $8004;
procedure outer;
var allocation:^byte;target,local:integer;
function callback(n:integer):integer;
begin callback:=local+n end;
begin
 local:=40;target:=addr(callback);machine[2]:=chr(lo(target));machine[3]:=chr(hi(target));
 getmem(allocation,$C900-heapptr);stackptr:=heapptr+90;
 actual:=wire(17);
end;
begin outer end.
