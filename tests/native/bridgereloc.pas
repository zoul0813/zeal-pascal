program BridgeRelocation;
type bytes=array[0..2]of byte;
var machine:bytes;
const checks:integer=0;saved:integer=0;target:integer=0;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('BRIDGE-RELOC-FAIL-',checks);halt end end;
procedure install(address:integer);
begin target:=address;machine[0]:=195;machine[1]:=lo(address);machine[2]:=hi(address)end;
function wire(value:integer):integer;external $8000;
{$A+}
procedure maker;
{$A-}
function callback(value:integer):integer;
begin callback:=value+7 end;
begin install(addr(callback));check(mem[target-11]=0)end;
procedure outer;
var local,address:integer;
function callback(value:integer):integer;
begin callback:=value+local end;
begin
 local:=13;address:=addr(callback);install(address);
 check(mem[address-11]=1);check(address=addr(callback));check(wire(9)=22);
end;
begin
 saved:=stackptr;maker;check(wire(3)=10);outer;check(stackptr=saved);
 writeln('BRIDGE-RELOCATION-OK');
end.
