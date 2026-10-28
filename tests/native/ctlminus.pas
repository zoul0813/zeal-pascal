{$C-}
program ConsoleMinus;
var s:string[8];c:char;saved:integer;
begin
 saved:=stackptr;
 if CBreak then halt;
 readln(Con,s);if s<>'X' then halt;
 read(Trm,c);if c<>'Y' then halt;
 read(Kbd,c);if c<>#3 then halt;
 if stackptr<>saved then halt;
 writeln('C-MINUS-OK');
end.
