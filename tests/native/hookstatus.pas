program HookStatus;
const checks:integer=0;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('HOOK-STATUS-FAIL-',checks);halt end end;
procedure outer;
var old,saved,calls:integer;value:boolean;c:char;
function status:boolean;
begin calls:=calls+1;status:=calls<3 end;
begin
 old:=constptr;saved:=stackptr;calls:=0;constptr:=addr(status);
 check(keypressed);check(keypressed);check(not keypressed);check(calls=3);
 constptr:=old;check(stackptr=saved);
 while not keypressed do;check(keypressed);read(Kbd,c);check(c='G');
end;
begin recurptr:=stackptr-2048; outer;writeln('HOOK-STATUS-OK')end.
