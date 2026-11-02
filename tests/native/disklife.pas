program DiskLife;
type ints=file of integer;item=record tag:integer;f:ints end;pointer=^item;
var p:pointer;markptr:pointer;i,first,savedRP:integer;
{$A-}
procedure nested(depth:integer);
var f:ints;s:string[24];v,before:integer;
begin
  if first=0 then first:=addr(f) else if addr(f)<>first then halt;
  before:=recurptr;
  str(depth,s);s:=concat('H:/REC',s,'.DAT');
  assign(f,s);rewrite(f);v:=depth;write(f,v);
  if depth>0 then nested(depth-1);
  if recurptr<>before then halt;
  seek(f,0);read(f,v);if(v<>depth)or(filesize(f)<>1)then halt;
  close(f);exit;
end;
procedure heapfiles;
begin
  new(p);p^.tag:=123;
  with p^ do begin
    assign(f,'H:/HEAPIO.DAT');rewrite(f);
    i:=42;write(f,i);
    {$I-}seek(f,9);if ioresult<>$91 then halt;{$I+}
    if tag<>123 then halt;close(f);
  end;
  dispose(p);
  mark(markptr);new(p);
  assign(p^.f,'H:/RELEASE.DAT');rewrite(p^.f);
  i:=55;write(p^.f,i);close(p^.f);release(markptr);
end;
begin recurptr:=stackptr-2048;
  savedRP:=recurptr;first:=0;nested(3);if recurptr<>savedRP then halt;heapfiles;
  writeln('DISK-LIFETIME-OK');
end.
