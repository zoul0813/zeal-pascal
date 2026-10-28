program Steps;
var n:integer;
begin
 n:=0;
{$I source-parts/step.inc}
{$I ./source-parts/step.inc}
 if n<>2 then halt;
 writeln('SOURCE-REPEATED-OK')
end.
