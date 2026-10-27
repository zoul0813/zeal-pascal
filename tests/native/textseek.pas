program TextSeek;
var f:text;n:integer;a,b:boolean;
begin
  assign(f,'H:/TEXTSPACE.TXT');rewrite(f);
  writeln(f,'  ');writeln(f,'  12  ');writeln(f,'   ');close(f);
  reset(f);a:=eoln(f);b:=seekeoln(f);
  if a or not b then halt;
  readln(f);a:=seekeof(f);read(f,n);b:=seekeoln(f);
  if a or(n<>12)or not b then halt;
  if not seekeof(f)then halt;
  close(f);writeln('TEXT-SEEK-OK');
end.
