program KbdUpper;
var pad:array[1..11200]of byte;c:char;saved:integer;
begin
  saved:=stackptr;pad[1]:=73;pad[11200]:=149;
  read(Kbd,c);
  if(c<>'G')or(pad[1]<>73)or(pad[11200]<>149)then halt;
  if stackptr<>saved then halt;
  writeln('KBD-UPPER-OK');
end.
