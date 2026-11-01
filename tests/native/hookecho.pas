program HookEcho;
const checks:integer=0;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('HOOK-ECHO-FAIL-',checks);halt end end;
procedure outer;
var log:string[80];line:string[10];old:integer;c:char;
procedure capture(value:char);begin log:=log+value end;
begin
 old:=conoutptr;conoutptr:=addr(capture);log:='';
 readln(line);check(line='abc');check(log='abc'#13#10);
 log:='';read(Trm,c);check(c='X');check(log='X');
 log:='';read(Kbd,c);check(c='Y');check(log='');
 conoutptr:=old;
end;
begin recurptr:=stackptr-2048; outer;writeln('HOOK-ECHO-OK')end.
