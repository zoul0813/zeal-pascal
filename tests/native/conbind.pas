program ConsoleBindings;
type item=record f:text;tag:integer end;ptritem=^item;
var f,g,clone:text;a,b,c:char;p,q,r:boolean;code,saved,checks:integer;x:ptritem;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('CON-BIND-FAIL-',checks);halt end end;
procedure take(var source:text;var target:char);
begin read(source,target)end;
begin
 saved:=stackptr;assign(f,'cOn:');assign(g,'CON:');rewrite(g);
 p:=eoln(f);q:=eoln(g);r:=eoln(Con);take(f,a);take(g,b);take(Con,c);
 check((not p)and(not q)and(not r)and(a='A')and(b='B')and(c='C'));readln(Con);
 p:=eoln(f);reset(f);take(f,a);p:=eoln(f);close(f);take(f,b);
 check((a='Y')and(b='Z'));readln(g);
 new(x);x^.tag:=123;assign(x^.f,'CON:');with x^ do begin take(f,c);check((tag=123)and(c='H'))end;
 dispose(x);readln(Con);
 move(f,clone,sizeof(f));{$I-}read(clone,c);code:=ioresult;{$I+}check(code=2);
 assign(clone,'CON:');take(clone,c);check(c='N');readln(Con);
 {$I-}close(Con);code:=ioresult;{$I+}check(code=$22);
 {$I-}reset(Con);code:=ioresult;{$I+}check(code=$22);
 {$I-}rewrite(Con);code:=ioresult;{$I+}check(code=$22);
 {$I-}assign(Con,'CON:');code:=ioresult;{$I+}check(code=$22);
 {$I-}erase(f);code:=ioresult;{$I+}check(code=$20);
 {$I-}rename(f,'H:/CONINVALID');code:=ioresult;{$I+}check(code=$20);
 close(f);close(g);close(clone);check(stackptr=saved);writeln('CON-BIND-OK');
end.
