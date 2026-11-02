program ExecFailure;
var target:file;data:file of integer;n:integer;
    context:record marker:integer end;i,low:integer;
    guard:array[1..1536]of byte absolute $F400;
begin
  Assign(data,'H:/RECOVER.DAT');Rewrite(data);n:=1234;Write(data,n);Close(data);
  FillChar(guard,SizeOf(guard),167);
  with context do begin
    marker:=42;Assign(target,'H:/TOCTOU.COM');
    {$I-}Execute(target);{$I+}
    if (IOResult<>1)or(marker<>42)then Halt;
    low:=0;
    for i:=1 to 1536 do
      if (low=0)and(guard[i]<>167)then low:=$F400+i-1;
    Writeln('EXECUTE-STACK-LOW:',low);
    Reset(data);Read(data,n);Close(data);
    if n<>1234 then Halt;
    Writeln('EXECUTE-COMMIT-RETRY-OK')
  end
end.
