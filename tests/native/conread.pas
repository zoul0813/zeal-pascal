program ConsoleRead;
var a,b,c,d,e:char;n,m,checks,saved,calls:integer;s,t:string[127];
    p,q:boolean;r:real;small:byte;values:array[1..2]of integer;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('CON-READ-FAIL-',checks);halt end end;
function choose:integer;
begin calls:=calls+1;read(Con,n);choose:=1 end;
begin
 saved:=stackptr;
 p:=eoln(Input);q:=eoln(Con);read(Input,a);read(Con,b);read(Output,c);
 check((not p)and(not q)and(a='A')and(b='B')and(c='C'));readln(Con);
 read(Con,n,m);check((n=12)and(m=34));readln(Con);
 readln(Con,s);readln(Con,t);check((s='A')and(t='B'));
 read(Con,a,b);check((a='X')and(b=#13));readln(Con);
 read(Con,c);check(c='Y');readln(Con);
 p:=eoln(Input);read(Con,c,d);read(Con,e);read(Input,a);
 check((a='A')and(c=#13)and(d=#10)and(e='B'));readln(Con);
 {$B+}read(n);read(Con,c);m:=99;read(Input,m);check(m=99);
 read(Input,d);read(Input,m);check((n=1)and(m=7)and(c='B')and(d=#26));readln(Con);read(Input,d);check(d=#13);
 readln(Con,small,r,a,s);check((small=44)and(r=1.25)and(a=' ')and(s='Zabc'));
 calls:=0;read(Con,values[choose]);check((n=55)and(values[1]=66)and(calls=1));readln(Con);
 readln(Con,s);check((length(s)=127)and(s[1]='L')and(s[127]='L'));
 check(eof(Input));read(Input,d);check(d=#26);
 read(Con,c);n:=99;{$B+}read(n);read(Con,d);
 check((c='A')and(d='B')and(n=99)and eof(Input));readln(Con);
 check(eof(Con));read(Con,c);check(c=#26);
 read(Con,c);check(c='Z');readln(Con);
 check(stackptr=saved);writeln('CON-READ-OK');
end.
