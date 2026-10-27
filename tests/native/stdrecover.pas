program ConsoleRecovery;
var c,d,e:char;code:integer;
begin
  buflen:=3;
  {$I-}read(Input,c);code:=IOResult;{$I+}
  read(Input,c,d,e);
  if(buflen<>127)or(code<>$20)or(c<>'A')or(d<>#13)or(e<>#10)then halt;
  writeln('CONSOLE-RECOVERY-OK');
end.
