program KbdLife;
{$A-}
type node=record tag:integer;f:Text end;ptr=^node;
var p,q:ptr;c:char;saved,checks:integer;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('KBD-LIFE-FAIL-',checks);halt end end;
procedure recurse(n:integer);
var f:Text;c:char;
begin
  assign(f,'KBD:');reset(f);check(not eoln(f));
  if n>0 then recurse(n-1);
  read(f,c);check(ord(c)=ord('V')-n)
end;
begin
  saved:=stackptr;new(p);p^.tag:=123;assign(p^.f,'KBD:');reset(p^.f);
  check(not eoln(p^.f));read(p^.f,c);check(c='H');check(p^.tag=123);dispose(p);
  new(q);q^.tag:=456;assign(q^.f,'KBD:');rewrite(q^.f);
  check(not eoln(q^.f));read(q^.f,c);check(c='J');check(q^.tag=456);dispose(q);
  recurse(2);check(stackptr=saved);writeln('KBD-LIFE-OK');
end.
