program ExecChainNext;
var target:file;data:file of integer;n:integer;
begin
  Assign(target,'H:/MISSING.COM');
  {$I-}Execute(target);{$I+}
  if IOResult<>1 then Halt;
  Assign(target,'H:/EXENEXT.COM');Execute(target)
end.
