program ConsoleOutput;
var f:text;code:integer;
begin
 assign(f,'CON:');write(Con,'');write(Trm,'');write(Con,'CON=',12:4,':',true:6,':',1.5:6:2);writeln(Con);
 write(Trm,'TRM=',7:3);writeln(f,'ASSIGNED');
 {$I-}close(Con);write(Con,'SHOULD-NOT-WRITE');code:=ioresult;{$I+}
 if code<>$22 then halt;
 close(f);writeln('CON-OUTPUT-OK');
end.
