program ConsoleError;
var code,saved:integer;
begin
 saved:=stackptr;
 {$I-}write(Con,'X');code:=ioresult;{$I+}
 if code<>$F0 then halt;
 CBreak:=false;
 if stackptr<>saved then halt;
 writeln('C-ERROR-OK');
end.
