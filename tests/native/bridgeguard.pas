program BridgeGuard;
type code=string[3];
var allocation:^byte;
const machine:code=#195#0#0;actual:integer=0;
{$A-}
function wire(n:integer):integer;external $800E;
procedure outer;
var target,local:integer;
function callback(n:integer):integer;
begin callback:=local+n end;
begin
 local:=40;target:=addr(callback);machine[2]:=chr(lo(target));machine[3]:=chr(hi(target));
 stackptr:=heapptr+90;
 actual:=wire(17);
end;
begin getmem(allocation,$C900-heapptr);recurptr:=heapptr+16;outer end.
