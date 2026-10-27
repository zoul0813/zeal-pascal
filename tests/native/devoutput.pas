program NativeDevices;
type holder=record f:text end; ptr=^holder;
var f:text;p:ptr;c:char;code,n,saved:integer;
procedure check(ok:boolean);
begin n:=n+1;if not ok then begin writeln('DEVICE-FAIL-',n);halt end end;
procedure send(var g:text);
begin writeln(g,'AUX=',12:4,':',true:6,':',1.5:6:2)end;
function side:integer;
begin n:=n+1;side:=7 end;
begin
 saved:=stackptr;
 send(Aux);assign(f,'aUx:');reset(f);rewrite(f);
 writeln(f,'ASSIGNED');close(f);write(f,'CLOSE-IS-NOOP');writeln(f);
 new(p);assign(p^.f,'AUX:');writeln(p^.f,'HEAP');dispose(p);
 {$I-}
 writeln(Lst,'MISSING');code:=ioresult;check(code=32);
 c:='!';read(Lst,c);code:=ioresult;check((code=2)and(c='!'));
 assign(Lst,'H:/DEVINVALID');code:=ioresult;check(code=34);
 reset(Lst);code:=ioresult;check(code=34);
 rewrite(Lst);code:=ioresult;check(code=34);
 close(Lst);code:=ioresult;check(code=34);
 reset(Aux);code:=ioresult;check(code=34);
 rewrite(Aux);code:=ioresult;check(code=34);
 close(Aux);code:=ioresult;check(code=34);
 assign(Aux,'H:/DEVINVALID');code:=ioresult;check(code=34);
 assign(f,'lSt:');reset(f);rewrite(f);close(f);
 writeln(f,'MISSING');code:=ioresult;check(code=32);
 read(f,c);code:=ioresult;check((code=2)and(c='!'));
 erase(f);code:=ioresult;check(code=32);
 rename(f,'H:/DEVINVALID');code:=ioresult;check(code=32);
 writeln(Lst,'MISSING');write(Aux,'SUPPRESSED',side);
 code:=ioresult;check(code=32);check(n=16);
 {$I+}
 check(stackptr=saved);writeln('DEVICES-OK');
end.
