program UQueue;
var c:char;i,j,saved:integer;
{$U+}
begin
 saved:=stackptr;
 for j:=1 to 40 do begin end;
 for i:=1 to 270 do begin
  repeat until keypressed;
  read(kbd,c);
  {$U-}
  if c<>chr(97+(i-1)mod 26)then halt;
  {$U+}
 end;
 {$U-}
 if stackptr<>saved then halt;
 writeln('U-QUEUE-OK');
end.
