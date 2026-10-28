program KbdUpper;
var pad:array[1..10480]of byte;c:char;saved:integer;
begin
  saved:=stackptr;pad[1]:=73;pad[10480]:=149;
  read(Kbd,c);
  if(c<>'G')or(pad[1]<>73)or(pad[10480]<>149)then halt;
  if stackptr<>saved then halt;
  writeln('KBD-UPPER-OK');
end.
