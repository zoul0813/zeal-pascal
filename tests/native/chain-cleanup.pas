{$C-}
program ChainCleanup;
var shared:string[20];number:integer;data:file of char;ch:char;
    target:file;context:record marker:integer end;
begin
  Assign(target,'H:/NEXT.CHN');
  with context do begin
    marker:=40;
    {$I-}Chain(target);{$I+}
    if IOResult<>32 then Halt;
    marker:=marker+1;
    if marker<>41 then Halt
  end;
  WriteLn('CHAIN-CLEANUP-OK')
end.
