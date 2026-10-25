program StringBoundaries;
type huge=string[255]; short=string[3];
const a='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-';
      b='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+';
var s:huge; n,seen:integer; tiny:short;
function value:huge;
begin value:=s end;
function identity(v:huge):huge;
begin identity:=v end;
function small(v:short):short;
begin small:=v end;
procedure accept(x:integer; v:short; y:integer);
begin if (x=123) and (y=456) and (v='abc') then seen:=seen+1 end;
begin
 s:=a+a+a+b;
 if (length(s)=255) and (length(value)=255) and (value=s) then writeln('STRING-255-RESULT-OK');
 if identity(identity(value))=s then writeln('STRING-255-NESTED-OK');
 tiny:=value;
 if (tiny='abc') and (small(value)='abc') then writeln('STRING-RESULT-TRUNCATE-OK');
 seen:=0;
 for n:=1 to 1000 do begin accept(123,value,456); tiny:=small(value) end;
 if (seen=1000) and (tiny='abc') then writeln('STRING-255-ARGUMENT-CLEANUP-OK');
 s:='';
 if (length(identity(value))=0) and (value+identity(value)='') then writeln('STRING-ZERO-RESULT-OK');
end.
