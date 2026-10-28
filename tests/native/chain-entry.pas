program ChainEntry;
{$C-}
var shared:string[20];number:integer;
const counter:integer=17;
begin
  if shared<>'retained' then halt;
  if number<>1234 then halt;
  if CBreak then halt;
  if ConOutPtr=0 then halt;
  if ConStPtr=0 then halt;
  if LstOutPtr=0 then halt;
  if AuxOutPtr=0 then halt;
  if UsrOutPtr=0 then halt;
  if counter<>17 then halt;
  counter:=counter+1;
  writeln('CHAIN-ENTRY-OK:',shared,':',number,':',counter)
end.
