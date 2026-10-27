program IndexModes;
type dense=packed record first:byte;pad:array[1..253]of byte;last:byte end;
 blocks=array[-1..1]of dense;
 powers=array[-2..2]of integer;
 matrix=array[-1..1,2..4]of integer;
 empty=record end;
 zeroes=array[-1..1]of empty;
var left:byte;a:blocks;right:byte;cells:matrix;values:powers;
 z:zeroes;
type pointeralias=^blocks;
var p:pointeralias;calls,index,checks:integer;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('INDEX-FAIL-',checks);halt end end;
function next:integer;begin calls:=calls+1;next:=index end;
procedure put(var value:byte;number:byte);begin value:=number end;
{$R+,X+}
procedure speed(var target:blocks);
var k:integer;
begin
 calls:=0;
 for k:=-1 to 1 do begin index:=k;target[next].first:=k+2;put(target[next].last,k+12)end;
 check(calls=6);
 for k:=-1 to 1 do begin check(target[k].first=k+2);check(target[k].last=k+12)end;
 for k:=-2 to 2 do values[k]:=k*7;
 for k:=-2 to 2 do check(values[k]=k*7);
 for k:=-1 to 1 do begin cells[k,2]:=k;cells[k,4]:=k+8 end;
 for k:=-1 to 1 do begin check(cells[k,2]=k);check(cells[k,4]=k+8)end;
 check(addr(z[-1])=addr(z[1]));
end;
(*$x-*)
procedure compact(var target:blocks);
var k:integer;
begin
 calls:=0;
 for k:=-1 to 1 do begin index:=k;target[next].first:=k+22;put(target[next].last,k+32)end;
 check(calls=6);
 for k:=-1 to 1 do begin check(target[k].first=k+22);check(target[k].last=k+32)end;
 for k:=-2 to 2 do values[k]:=k*9;
 for k:=-2 to 2 do check(values[k]=k*9);
 for k:=-1 to 1 do begin cells[k,2]:=k+2;cells[k,4]:=k+18 end;
 for k:=-1 to 1 do begin check(cells[k,2]=k+2);check(cells[k,4]=k+18)end;
 check(addr(z[-1])=addr(z[1]));
end;
begin
 left:=91;right:=173;checks:=0;p:=ptr(addr(a));
 speed(a);compact(p^);check(left=91);check(right=173);
 writeln('INDEX-MODES-OK')
end.
