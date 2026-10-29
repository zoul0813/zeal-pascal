program ChainNext;
var shared:string[20]; number:integer; data:file of char; ch:char;
const initialized:integer=17;
begin
  Read(data,ch);initialized:=initialized+1;
  if FilePos(data)<>2 then begin WriteLn('BAD-POSITION');Halt end;
  WriteLn('CHAIN-CALL-OK:',shared,':',number,':',ch,':',initialized)
end.
