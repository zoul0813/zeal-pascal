{$C+}
program ConsoleState;
var saved:integer;
procedure late;
{$C-}
begin
 if not CBreak then halt;
end;
begin
 saved:=stackptr;
 if not CBreak then halt;
 late;
 {$C-}if not CBreak then halt;
 CBreak:=false;if CBreak then halt;
 CBreak:=true;if not CBreak then halt;
 if stackptr<>saved then halt;
 writeln('C-STATE-OK');
end.
