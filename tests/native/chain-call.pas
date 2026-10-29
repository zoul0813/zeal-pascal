program ChainCaller;
var shared:string[20]; number:integer; data:file of char; ch:char;
    targets:array[1..1]of file; calls:integer;
    context:record marker:integer end;
function choose:integer;
begin calls:=calls+1;choose:=1 end;
begin
  shared:='retained';number:=1234;calls:=0;
  Assign(data,'H:/SHARED.DAT');Reset(data);Read(data,ch);
  if ch<>'A' then Halt;
  with context do begin
    marker:=40;
    Assign(targets[1],'H:/MISSING.CHN');
    {$I-}Chain(targets[choose]);{$I+}
    if IOResult<>1 then Halt;
    marker:=marker+1;
    Assign(targets[1],'H:/INVALID.CHN');
    {$I-}Chain(targets[choose]);{$I+}
    if IOResult<>32 then Halt;
    marker:=marker+1;
    if (marker<>42)or(calls<>2) then Halt
  end;
  Assign(targets[1],'H:/NEXT.CHN');
  Chain(targets[choose]);
  Halt
end.
