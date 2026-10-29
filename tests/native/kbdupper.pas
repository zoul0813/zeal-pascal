program KbdUpper;
{ Leave295 bytes for saved-program startup while keeping raw dispatch aboveC000. }
const padsize=10185;
var pad:array[1..padsize]of byte;c:char;saved:integer;
begin
  saved:=stackptr;pad[1]:=73;pad[padsize]:=149;
  read(Kbd,c);
  if(c<>'G')or(pad[1]<>73)or(pad[padsize]<>149)then halt;
  if stackptr<>saved then halt;
  writeln('KBD-UPPER-OK');
end.
