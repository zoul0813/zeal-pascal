program Sources;
const rootvalue=2;
{$I source-parts/decl.inc}
{$I source-parts/code.inc}
begin
 fromOther;
 if(n<>3)or(greeting<>'included')or(number<>-1.25)or(piAlias<>Pi)then halt;
 writeln('SOURCE-INCLUDES-OK')
end.
