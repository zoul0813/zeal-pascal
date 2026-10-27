program ConsoleOutputRecovery;
var c:char;code:integer;p:boolean;
begin
 p:=eoln(Con);
 {$I-}write(Con,'X');code:=ioresult;{$I+}
 read(Con,c);readln(Con);
 if(code<>$F0)or(c<>'Z')then halt;
 writeln('CON-OUT-RECOVERY-OK');
end.
