program KbdBind;
var a,b,clone:Text;x,y,z:char;p,q,r:boolean;saved,checks,code,calls:integer;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('KBD-BIND-FAIL-',checks);halt end end;
procedure take(var f:Text;var c:char);
begin read(f,c) end;
function destination:integer;
begin calls:=calls+1;destination:=1 end;
begin
  saved:=stackptr;assign(a,'kBd:');assign(b,'KBD:');rewrite(b);
  p:=eoln(a);q:=eoln(b);r:=eoln(Kbd);
  take(a,x);take(b,y);take(Kbd,z);
  check((not p)and q and(not r));check((x='X')and(ord(y)=13)and(z='Y'));
  close(a);take(a,x);check(x='Z');
  check(not eoln(a));reset(a);take(a,x);check(x='M');
  {$I-}write(a,'X');code:=ioresult;{$I+}check(code=3);
  {$I-}erase(a);code:=ioresult;{$I+}check(code=$20);
  {$I-}rename(a,'H:/KBDINVALID');code:=ioresult;{$I+}check(code=$20);
  {$I-}close(Kbd);code:=ioresult;{$I+}check(code=$22);
  {$I-}reset(Kbd);code:=ioresult;{$I+}check(code=$22);
  {$I-}rewrite(Kbd);code:=ioresult;{$I+}check(code=$22);
  {$I-}assign(Kbd,'H:/KBDINVALID');code:=ioresult;{$I+}check(code=$22);
  {$I-}write(Kbd,'X');code:=ioresult;{$I+}check(code=3);
  {$I-}close(Kbd);read(a,mem[addr(x)+destination-1]);code:=ioresult;{$I+}
  check((code=$22)and(calls=1));
  check(seekeoln(a));take(a,x);check(ord(x)=13);
  check(seekeof(b));take(b,y);check(ord(y)=26);
  move(a,clone,sizeof(a));{$I-}read(clone,z);code:=ioresult;{$I+}check(code=2);
  close(a);close(b);check(stackptr=saved);writeln('KBD-BIND-OK');
end.
