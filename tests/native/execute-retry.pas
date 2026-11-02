program ExecRetry;
var targets:array[1..1]of file;calls:integer;
    context:record marker:integer end;data:file of integer;n:integer;
function choose:integer;
begin calls:=calls+1;choose:=1 end;
begin
  calls:=0;
  Assign(data,'H:/RETAIN.DAT');Rewrite(data);n:=1234;Write(data,n);
  with context do begin
    marker:=42;Assign(targets[1],'H:/MISSING.COM');
    {$I-}Execute(targets[choose]);{$I+}
    if (IOResult<>1)or(calls<>1)or(marker<>42)then Halt;
    Assign(targets[1],'H:/INVALID.COM');
    {$I-}Execute(targets[choose]);{$I+}
    if (IOResult<>32)or(calls<>2)or(marker<>42)or(FilePos(data)<>1)then Halt;
    Write(data,n);Close(data);
    Writeln('EXECUTE-RETRY-OK')
  end
end.
