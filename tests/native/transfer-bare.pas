program TransferBare;
var shared:string[20];number:integer;
begin
  if shared='' then
  begin
    shared:='retained';number:=1234;
    inline($CD/$00/$88);
    writeln('PARENT-AFTER');halt
  end;
  if shared<>'retained' then halt;
  if number<>1234 then halt;
  halt
end.
