program IndexWrap;
type item=array[1..255]of byte;
var a:array[0..0]of item absolute $9000;i:integer;
procedure check(ok:boolean);begin if not ok then halt end;
begin
 {$R-,X+}
 i:=-1;check(addr(a[i])=$8F01);i:=257;check(addr(a[i])=$8FFF);
 i:=-32768;check(addr(a[i])=$1000);i:=32767;check(addr(a[i])=$0F01);
 {$X-}
 i:=-1;check(addr(a[i])=$8F01);i:=257;check(addr(a[i])=$8FFF);
 i:=-32768;check(addr(a[i])=$1000);i:=32767;check(addr(a[i])=$0F01);
 writeln('INDEX-WRAP-OK')
end.
