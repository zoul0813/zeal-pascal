program ConsolePause;
var ready:boolean;c:char;saved:integer;
begin
 saved:=stackptr;ready:=false;
 {$U+}while not ready do begin end;{$U-}
 write(Con,'P');
 read(Kbd,c);if c<>'x' then halt;
 if stackptr<>saved then halt;
 writeln('C-PAUSE-OK');
end.
