program StandardOutput;
type box=record tag:integer end;
var f:text;x:box;code,calls:integer;
procedure emit(var target:text);
begin writeln(target,'ALIAS=',7:3)end;
function effect:integer;
begin calls:=calls+1;effect:=9 end;
begin
  if(sizeof(Input)<>16)or(sizeof(Output)<>16)then halt;
  writeln(Output,'STD=',-123:6,',',true:6,',',1.5:7:2,',','ok':4);
  emit(Output);
  {$I-}assign(Output,'H:/STDINVALID');code:=ioresult;if code<>$22 then halt;
  reset(Output);code:=ioresult;if code<>$22 then halt;
  rewrite(Output);code:=ioresult;if code<>$22 then halt;
  with x do begin
    tag:=123;close(Output);code:=ioresult;tag:=tag+1;
    if(code<>$22)or(tag<>124)then halt
  end;
  calls:=0;close(Output);writeln(Output,'SUPPRESSED',effect);code:=ioresult;
  if(code<>$22)or(calls<>1)then halt;
  {$I+}move(Output,f,16);
  {$I-}write(f,'NO');code:=ioresult;if code<>3 then halt;
  {$I+}assign(f,'H:/STDFILE.TXT');rewrite(f);write(f,'FILE');close(f);
  writeln(Output,'STD-OUTPUT-OK');
end.
