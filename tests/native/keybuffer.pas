program KeyBuffer;
type box=record tag:integer;s:string[20];c:char end;
var x:box;a:array[1..2]of char;saved,i,code,checks:integer;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('KEY-FAIL-',checks);halt end end;
function choose:integer;
begin repeat until keypressed;check(keypressed);choose:=1 end;
begin
  saved:=stackptr;x.tag:=123;
  with x do begin
    repeat until keypressed;
    check(keypressed);readln(s);check(s='Ab');check(tag=123);
    read(Input,c);check(c='Q');check(not keypressed);
    readln(Input,s);check(s='tail');check(tag=123);
    for i:=1 to 40 do check(not keypressed);
    {$I-}close(Output);check(not keypressed);code:=ioresult;{$I+}
    check((code=$22)and(tag=123));
    read(Input,a[choose]);check(a[1]='X');readln(Input,s);check(s='rest');
    check(tag=123)
  end;
  check(stackptr=saved);writeln('KEY-BUFFER-OK');
end.
