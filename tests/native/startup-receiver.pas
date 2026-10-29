program StartupReceiver;
var f:File;
procedure NotCalled;
begin Chain(f) end;
begin
  WriteLn('COMPILED-STARTUP-OK:',ParamStr(1))
end.
