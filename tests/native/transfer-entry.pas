program TransferEntry;
{$C-}
var shared:string[20];number:integer;f:file of char;c:char;
const counter:integer=17;
begin
  if shared='' then
  begin
    shared:='retained';number:=1234;
    assign(f,'H:/SHARED.DAT');reset(f);read(f,c);
    if c<>'A' then halt;
    inline($CD/$00/$88);
    writeln('PARENT-AFTER');halt
  end;
  if shared<>'retained' then halt;
  if number<>1234 then halt;
  if counter<>17 then halt;
  counter:=counter+1;
  read(f,c);
  if c<>'B' then halt;
  if filepos(f)<>2 then halt;
  writeln('CHAIN-TRANSFER-OK:',shared,':',number,':',c,':',counter)
end.
