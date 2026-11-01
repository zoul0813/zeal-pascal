program HookDevices;
type r=record f:text end;p=^r;
const checks:integer=0;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('HOOK-DEVICE-FAIL-',checks);halt end end;
procedure outer;
var printer,serial,user:string[80];oldl,olda,oldu,saved,code:integer;f:text;heap:p;
procedure listout(c:char);begin printer:=printer+c end;
procedure auxout(c:char);begin serial:=serial+c end;
procedure usrout(c:char);begin user:=user+c end;
procedure send(var g:text);begin writeln(g,7:3,':',true:5)end;
begin
 printer:='';serial:='';user:='';saved:=stackptr;
 oldl:=lstoutptr;olda:=auxoutptr;oldu:=usroutptr;
 lstoutptr:=addr(listout);auxoutptr:=addr(auxout);usroutptr:=addr(usrout);
 send(Lst);send(Aux);send(Usr);
 assign(f,'lSt:');reset(f);send(f);close(f);
 assign(f,'aUx:');rewrite(f);send(f);close(f);
 new(heap);assign(heap^.f,'uSr:');send(heap^.f);dispose(heap);
 check(printer='  7: TRUE'#13#10'  7: TRUE'#13#10);
 check(serial=printer);check(user=printer);
 {$I-}reset(Usr);code:=ioresult;{$I+}check(code=34);
 {$I-}erase(Usr);code:=ioresult;{$I+}check(code=32);
 lstoutptr:=oldl;auxoutptr:=olda;usroutptr:=oldu;write(Usr,'D');
 {$I-}write(Lst,'missing');code:=ioresult;{$I+}check(code=32);
 check(stackptr=saved);
end;
begin recurptr:=stackptr-2048; outer;writeln('HOOK-DEVICES-OK')end.
