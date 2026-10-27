program TextNesting;
var f,g:text;
function side:integer;
begin write(g,'X');side:=7 end;
begin
  assign(f,'H:/NESTOUT.TXT');assign(g,'H:/NESTSIDE.TXT');
  rewrite(f);rewrite(g);
  write(f,'L',side,'R');
  close(f);close(g);writeln('TEXT-NESTING-OK');
end.
