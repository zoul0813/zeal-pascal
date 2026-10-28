program PausedBreak;
var ready:boolean;
begin
 ready:=false;
 {$U+}while not ready do begin end;{$U-}
 write(Con,'P');
 writeln('C-STOP-FAILED');
end.
