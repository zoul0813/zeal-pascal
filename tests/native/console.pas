program ConsoleControls;
type guarded=record before:byte;text:string[127];after:byte end;
var g:guarded;i,saved,checks:integer;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('CONSOLE-FAIL-',checks);halt end end;
procedure line;
begin readln(g.text);check((g.before=73)and(g.after=149))end;
begin
 g.before:=73;g.after:=149;saved:=stackptr;
 check((buflen=127)and cbreak);
 buflen:=3;line;check((g.text='abX')and(buflen=127));
 buflen:=0;line;check((g.text='')and(buflen=127));
 buflen:=127;line;
 check((length(g.text)=127)and(g.text[1]='a')and(g.text[126]='a')and(g.text[127]='X'));
 line;check(g.text='clear');
 line;check(g.text='escape');
 line;check(g.text='delete');
 line;check(g.text='delete');
 line;check(g.text='delete');
 line;check(g.text='delete');
 line;check(g.text='history');
 line;check(g.text='Xistory');
 cbreak:=false;line;check(g.text='prefix');cbreak:=true;
 for i:=1 to 20 do begin line;check(g.text='again')end;
 check((stackptr=saved)and cbreak and(buflen=127));
 writeln('CONSOLE-OK');
end.
