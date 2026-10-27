program DiskBlocks;
var f:file;b:array[0..383]of byte;i,n:integer;
begin
  assign(f,'H:/BLOCKS.DAT');rewrite(f);
  for i:=0 to 255 do b[i]:=i;
  blockwrite(f,b,2,n);
  if(n<>2)or(filesize(f)<>2)or(filepos(f)<>2)then halt;
  close(f);reset(f);fillchar(b,384,0);
  blockread(f,b,3,n);
  if(n<>2)or(filepos(f)<>2)or not eof(f)then halt;
  for i:=0 to 255 do if b[i]<>i then halt;
  blockread(f,b,1,n);if n<>0 then halt;
  {$I-}blockread(f,b,1);if ioresult<>$99 then halt;
  {$I+}close(f);
  writeln('DISK-BLOCKS-OK');
end.
