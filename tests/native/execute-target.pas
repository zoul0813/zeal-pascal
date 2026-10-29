program ExecNext;
var data:file of integer;n:integer;
begin
  if ParamCount<>0 then Halt;
  Assign(data,'H:/CLOSED.DAT');Reset(data);Read(data,n);Close(data);
  if n<>1234 then Halt;
  Writeln('EXECUTE-CALL-OK')
end.
