program DefaultUser;
const checks:integer=0;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('USR-FAIL-',checks);halt end end;
procedure outer;
var f:text;c:char;saved:integer;
begin
 saved:=stackptr;read(Usr,c);check(c='U');assign(f,'USR:');
 check(not eof(f));read(f,c);check(c='V');close(f);
 check(eof(Usr));read(Usr,c);check(c=#26);read(Usr,c);check(c='W');
 check(stackptr=saved);
end;
begin recurptr:=stackptr-2048; outer;writeln('USR-DEFAULT-OK')end.
