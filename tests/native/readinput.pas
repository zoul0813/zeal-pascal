program NumericInput;
type small=1..5;wide=-2..2;
     item=record r:real;guard:byte end;
     rp=^real;ints=array[1..2]of integer;
var i,j,k,saved,calls,checks:integer;r:real;c:char;s:string[20];short:string[2];
    b:byte;n:small;w:wide;rec:item;p:rp;a:ints;
procedure fresh;external $40C6;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('READ-FAIL-',checks);halt end end;
procedure forwarded(var value:real);
begin readln(value)end;
function index:integer;
begin calls:=calls+1;write('INDEX>');index:=2 end;
begin
 fresh;{$B-}readln(s);{$B+}check(s='legacy');
 readln(i,r,c,s);
 check((i=-32768)and(r=1.25)and(c=' ')and(s='Ztail'));
 {$R+}readln(b,n,w);{$R-}
 check((b=44)and(n=99)and(w=32767));
 rec.guard:=99;forwarded(rec.r);check((rec.r=0.5)and(rec.guard=99));
 with rec do readln(r,i);check((rec.r=-1.25)and(i=7)and(rec.guard=99));
 p:=ptr(addr(rec.r));readln(p^);check((rec.r=0.125)and(rec.guard=99));
 calls:=0;readln(a[index]);check((a[2]=42)and(calls=1));
 {$B-}read(i);read(j);readln(s);
 check((i=11)and(j=22)and(s=' tail'));
 read(short,c,i);readln;
 check((short='ab')and(c='c')and(i=99));
 read(i);read(j);readln(s);check((i=5)and(j=6)and(s=''));
 {$B+}read(i);read(j);readln(s);
 check((i=31)and(j=32)and(s='done'));
 i:=777;r:=777;readln(i,r);check((i=777)and(r=777));
 readln(i,c,s);check((i=0)and(c='1')and(s=' X'));
 saved:=stackptr;
 for k:=1 to 2 do begin readln(i,r);check((i=12)and(r=0.25))end;
 check(stackptr=saved);
 {$B+}read;{$B-}readln(s);check(s='abc');
 {$B+}read(i);read;{$B-}read(j);check((i=11)and(j=33));
 read;read(i);check(i=11);read(c);check(c=#26);read(i);check(i=44);
 read(i);read;read(j);readln(s);
 check((i=55)and(j=66)and(s=''));
 check(stackptr=saved);
 writeln('READ-ALL-OK');
end.
