program DiskWide;
type block=array[1..300]of byte;wide=file of block;
var f:wide;
{$A-}
procedure transfer;
var b:block;i:integer;
begin
  for i:=1 to 300 do b[i]:=i mod 251;
  assign(f,'H:/WIDE.DAT');rewrite(f);write(f,b);close(f);
  fillchar(b,300,0);reset(f);read(f,b);
  for i:=1 to 300 do if b[i]<>i mod 251 then halt;
  close(f);
end;
begin recurptr:=stackptr-2048; transfer;writeln('DISK-WIDE-OK')end.
