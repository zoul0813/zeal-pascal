program KbdRead;
type box=record tag:integer;c:char end;
var x:box;s:string[3];n,code,saved,checks,calls:integer;r:real;b:byte;
    a:array[1..2]of char;p:boolean;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('KBD-READ-FAIL-',checks);halt end end;
function choose:integer;
begin calls:=calls+1;choose:=1 end;
begin
  saved:=stackptr;x.tag:=123;
  repeat until keypressed;check(keypressed);check(keypressed);
  read(Kbd,x.c);check(x.c='A');check(not keypressed);
  read(Kbd,n);read(Kbd,x.c);check((n=12)and(x.c=' '));
  read(Kbd,r);read(Kbd,x.c);check((r=1.25)and(x.c=' '));
  read(Kbd,b);read(Kbd,x.c);check((b=44)and(x.c=' '));
  {$I-}read(Kbd,n);code:=ioresult;{$I+}
  check(code=$10);read(Kbd,x.c);check(x.c=' ');
  read(Kbd,s);read(Kbd,x.c);check((s='abc')and(x.c='d'));
  check(eoln(Kbd));check(eoln(Kbd));check(not eof(Kbd));
  read(Kbd,x.c);check(ord(x.c)=13);
  readln(Kbd);check(not keypressed);
  read(Kbd,x.c);check(ord(x.c)=10);
  read(Kbd,x.c);check(ord(x.c)=27);
  read(Kbd,x.c);check(ord(x.c)=127);
  read(Kbd,x.c);check(ord(x.c)=3);
  check(eof(Kbd));check(eof(Kbd));check(eoln(Kbd));
  read(Kbd,x.c);check(ord(x.c)=26);
  check(not eof(Kbd));read(Kbd,x.c);check(x.c='X');
  with x do begin read(Kbd,a[choose]);check(a[1]='K');check(tag=123) end;
  check(calls=1);
  read(Kbd);check(not eoln(Kbd));read(Kbd,x.c);check(x.c='m');
  check(stackptr=saved);writeln('KBD-READ-OK');
end.
