program QueuedConsoleControl;
var ready:boolean;i:integer;c:char;saved:integer;
begin
 saved:=stackptr;ready:=false;
 {$U+}while not ready do begin end;{$U-}
 write(Trm,'T');
 for i:=1 to 254 do begin
  read(Kbd,c);if c<>chr(97+(i-1)mod 26)then halt;
 end;
 read(Kbd,c);if c<>'x' then halt;
 if keypressed then halt;
 if stackptr<>saved then halt;
 writeln('C-QUEUED-OK');
end.
