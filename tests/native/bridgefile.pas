program BridgeFile;
type code=string[3];ints=file of integer;
const machine:code=#195#0#0;checks:integer=0;saved:integer=0;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('BRIDGE-FILE-FAIL-',checks);halt end end;
procedure install(address:integer);
begin machine[2]:=chr(lo(address));machine[3]:=chr(hi(address))end;
procedure writecall(var f:ints;value:integer);external $8177;
procedure readcall(var f:ints;var value:integer);external $8177;
procedure outer;
var f:ints;value,total,index:integer;
procedure writer(var g:ints;n:integer);begin write(g,n)end;
procedure reader(var g:ints;var n:integer);begin read(g,n);total:=total+n end;
begin
 assign(f,'H:/BRIDGE.DAT');rewrite(f);total:=0;
 install(addr(writer));writecall(f,11);writecall(f,22);writecall(f,33);
 seek(f,0);install(addr(reader));for index:=1 to 3 do readcall(f,value);
 check((value=33)and(total=66));close(f);
end;
procedure verify;
var f:ints;value:integer;
begin
 assign(f,'H:/BRIDGE.DAT');reset(f);check(filesize(f)=3);
 read(f,value);check(value=11);read(f,value);check(value=22);read(f,value);check(value=33);close(f);
end;
begin recurptr:=stackptr-2048; saved:=stackptr;outer;verify;check(stackptr=saved);writeln('BRIDGE-FILE-OK')end.
