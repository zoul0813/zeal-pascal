program FullConsoleQueue;
var ready:boolean;i:integer;c:char;saved:integer;
begin
 saved:=stackptr;ready:=false;
 {$U+}while not ready do begin end;{$U-}
 write(Con,'F');
 for i:=1 to 255 do begin
  read(Kbd,c);if c<>chr(97+(i-1)mod 26)then halt;
 end;
 if keypressed then halt;
 if stackptr<>saved then halt;
 writeln('C-FULL-OK');
end.
