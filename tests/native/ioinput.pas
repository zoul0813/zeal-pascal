program IORecovery;
type pair=array[1..2]of integer;word=string[8];
var a:pair;n,m,code,calls,i,saved,checks:integer;r:real;b:byte;s:string[20];
{$A-,B-,I-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('IO-FAIL-',checks);halt end end;
function index:integer;
begin calls:=calls+1;index:=1 end;
function side:integer;
begin calls:=calls+1;side:=7 end;
function recover:word;
begin check(ioresult=16);recover:='RECOVER' end;
procedure nested(depth:integer);
var x,y,prior:integer;
begin
 prior:=stackptr;
 if depth>0 then nested(depth-1)
 else begin
  x:=111;y:=222;readln(x,y);code:=ioresult;
  check((code=16)and(x=111)and(y=222));readln(n);check(n=77)
 end;
 check(stackptr=prior)
end;
begin
 checks:=0;
 check((ioresult=0)and(ioresult=0));
 n:=111;m:=222;readln(n,m);code:=ioresult;
 check((n=7)and(m=222)and(code=16));readln(n);check(n=42);
 n:=111;m:=222;read(n);{$B+}read(m);code:=ioresult;
 {$B-}readln(n);check((n=99)and(m=222)and(code=16));
 a[1]:=111;a[2]:=222;calls:=0;saved:=stackptr;
 readln(a[index],a[index]);check(calls=2);
 for i:=1 to 100 do begin write(side:1);read(a[index])end;
 check((calls=202)and(a[1]=111)and(a[2]=222));check(stackptr=saved);
 code:=ioresult;check((code=16)and(ioresult=0));
 readln(n);check(n=42);check(stackptr=saved);
 readln(n);writeln('PREFIX=',ioresult:1);check(ioresult=0);
 readln(n);writeln(ioresult:1);check(ioresult=0);
 readln(n);calls:=0;writeln(side:1);code:=ioresult;
 check((calls=1)and(code=16));
 readln(n);str(12,s);val('bad',n,m);code:=ioresult;
 check((s='12')and(m=1)and(code=16));
 nested(3);check(stackptr=saved);
 readln(n);writeln('DROP',recover);check(ioresult=0);
 r:=2.5;n:=99;readln(r,n);code:=ioresult;
 check((r=2.5)and(n=99)and(code=16));
 readln(r);check(r=1.25);
 b:=99;readln(b);check((ioresult=16)and(b=99));
 readln(n);write('X':ioresult);writeln;
 check(stackptr=saved);check(ioresult=0);
 writeln('IO-INPUT-OK')
end.
