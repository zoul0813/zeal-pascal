program InputEndOfFile;
var s:string[127];c:char;i:integer;
begin
 readln(s);if s<>''then begin writeln('EOF-FAIL-1');halt end;
 readln(s);if s<>''then begin writeln('EOF-FAIL-2');halt end;
 readln(c);if ord(c)<>26 then begin writeln('EOF-FAIL-3');halt end;
 i:=73;readln(i);if i<>73 then begin writeln('EOF-FAIL-4');halt end;
 writeln('INPUT-EOF-OK');
end.
