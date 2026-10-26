program BufferedEndChar;
var i,checks:integer;c,d:char;s:string[8];
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('END-CHAR-FAIL-',checks);halt end end;
begin
 readln(c);check(ord(c)=26);
 read(i,c);check((i=11)and(ord(c)=26));
 readln(s);check(s='tail');
 read(c,d);check((ord(c)=26)and(d='X'));
 read(c,s);check((ord(c)=26)and(s='next'));
 writeln('END-CHAR-OK');
end.
