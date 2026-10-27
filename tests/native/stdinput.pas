program StandardInput;
type box=record tag,n,m:integer;b:byte;r:real;space,c:char;s:string[20]end;
var x:box;a:array[1..2]of integer;code:integer;
procedure take(var source:text;var n:integer);
begin read(source,n)end;
function choose:integer;
var v:integer;
begin read(Input,v);if v<>55 then halt;choose:=1 end;
begin
  with x do begin
    tag:=100;n:=-1;m:=-1;
    read(Input,n);take(Input,m);readln(Input,s);
    if(n<>11)or(m<>22)or(s<>' tail')or(tag<>100)then halt;
    read(Input);readln(Input,s);if s<>'prime 9' then halt;
    read(Input,n);{$B-}read(m);{$B+}readln(Input);
    if(n<>33)or(m<>44)then halt;
    n:=777;{$I-}read(Input,n);code:=ioresult;tag:=tag+1;
    if(code<>$10)or(n<>777)or(tag<>101)then halt;
    read(Input,m);code:=ioresult;if(code<>0)or(m<>77)then halt;
    readln(Input);{$I+}
    readln(Input,b,r,space,c,s);
    if(b<>44)or(r<>1.5)or(space<>' ')or(c<>'Z')or(s<>'abc')then halt
  end;
  read(Input,a[choose]);if a[1]<>66 then halt;readln(Input);
  readln(Input,x.s);if x.s<>'' then halt;
  x.n:=999;read(Input,x.n);if x.n<>999 then halt;
  read(Input,x.c);if x.c<>#26 then halt;
  writeln(Output,'STD-INPUT-OK');
end.
