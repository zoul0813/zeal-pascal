program KeyOnly;
var saved:integer;
begin
  saved:=stackptr;
  repeat until keypressed;
  if not keypressed then halt;
  if stackptr<>saved then halt;
  writeln('KEY-ONLY-OK');
end.
