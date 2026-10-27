program DiskFiles;
type ints=file of integer;
var f:ints;i,n:integer;r:record a,b:integer end;
begin
  assign(f,'H:/TYPED.DAT');
  rewrite(f);
  i:=11;write(f,i);i:=22;write(f,i);i:=33;write(f,i);
  if(filesize(f)<>3)or(filepos(f)<>3)then halt;
  seek(f,1);i:=99;write(f,i);flush(f);close(f);
  reset(f);
  read(f,r.a,r.b,i);
  if(r.a<>11)or(r.b<>99)or(i<>33)or not eof(f)then halt;
  seek(f,0);read(f,i);if i<>11 then halt;
  {$I-}seek(f,4);n:=ioresult;
  if(n<>$91)or(ioresult<>0)then halt;
  close(f);
  assign(f,'H:/MISSING.DAT');reset(f);
  if ioresult<>1 then halt;
  {$I+}assign(f,'H:/TYPED.DAT');reset(f);
  if filesize(f)<>3 then halt;
  close(f);
  writeln('DISK-TYPED-OK');
end.
