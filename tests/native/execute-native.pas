program ExecNative;
var target:file;
begin
  Assign(target,'H:/RAWEXIT.BIN');Execute(target);
  Writeln('EXECUTE-RETURNED')
end.
