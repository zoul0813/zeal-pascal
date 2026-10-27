program BridgeFrames;
type code=string[3];
const machine:code=#195#0#0;target:integer=0;checks:integer=0;
 counter:integer=0;saved:integer=0;frontier:integer=0;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('BRIDGE-FRAME-FAIL-',checks);halt end end;
procedure install(address:integer);
begin target:=address;machine[2]:=chr(lo(address));machine[3]:=chr(hi(address))end;
function word(address:integer):integer;
var value:integer;
begin move(mem[address],value,2);word:=value end;
procedure descriptor(address,kind:integer);
begin
 check((mem[address-16]=90)and(mem[address-15]=84)and(mem[address-14]=72)and(mem[address-13]=75));
 check((mem[address-12]=1)and(mem[address-11]=kind)and(word(address-10)=16));
 check((mem[address]=33)and(word(address+1)=address-16)and(mem[address+3]=195));
 check((word(address+4)=word(address-8))and(word(address-2)=0));
 if kind=0 then check((word(address-6)=0)and(word(address-4)=0))
 else check((word(address-6)<>0)and(word(address-6)=word(address-4)));
end;
function wire(value:integer):integer;external $8004;
procedure run;external $8004;
{$A+}
procedure staticmaker;
procedure increment;begin counter:=counter+1 end;
procedure middle;
procedure callback;begin increment end;
begin install(addr(callback));descriptor(target,0)end;
begin middle end;
{$A-}
procedure top;begin counter:=counter+10 end;
procedure recursive(level,parent:integer);
var local,mine,before:integer;markpoint:^byte;
function callback(value:integer):integer;
begin callback:=local+value end;
begin
 local:=100+level;mine:=addr(callback);descriptor(mine,1);
 check(mine=addr(callback));check(mine<>parent);
 mark(markpoint);check(mine=addr(callback));release(markpoint);
 if parent<>0 then begin install(parent);check(wire(7)=108+level)end;
 if level>0 then recursive(level-1,mine);
 check(mine=addr(callback));install(mine);before:=stackptr;
 check(wire(7)=107+level);check(stackptr=before);
end;
procedure forwardcase;
var local:integer;
function callback(value:integer):integer;forward;
procedure setaddress;begin install(addr(callback))end;
function callback;
begin callback:=local+value end;
begin local:=90;setaddress;check(wire(9)=99)end;
procedure mixed;
var local:integer;
{$A+}
procedure fixed;
{$A-}
procedure callback;begin local:=local+5 end;
begin install(addr(callback));descriptor(target,1);run end;
begin local:=40;fixed;check(local=45)end;
begin
 saved:=stackptr;frontier:=heapptr;
 staticmaker;run;run;check(counter=2);descriptor(target,0);
 install(addr(top));run;check(counter=12);
 recursive(4,0);forwardcase;mixed;
 check(stackptr=saved);check(heapptr=frontier);writeln('BRIDGE-FRAMES-OK');
end.
