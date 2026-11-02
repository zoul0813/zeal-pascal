program ExecCall;
var target:file;data:file of integer;n:integer;
begin
  Assign(data,'H:/CLOSED.DAT');Rewrite(data);n:=1234;Write(data,n);Close(data);
  Assign(target,'H:/EXENEXT.COM');Execute(target);
  Writeln('EXECUTE-RETURNED')
end.
