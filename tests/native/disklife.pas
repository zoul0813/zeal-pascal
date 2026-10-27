program DiskLife;
type ints=file of integer;item=record tag:integer;f:ints end;pointer=^item;
var p:pointer;markptr:pointer;i:integer;
{$A-}
procedure nested(depth:integer);
var f:ints;s:string[24];v:integer;
begin
  str(depth,s);s:=concat('H:/REC',s,'.DAT');
  assign(f,s);rewrite(f);v:=depth;write(f,v);
  if depth>0 then nested(depth-1);
  exit;
end;
procedure heapfiles;
begin
  new(p);p^.tag:=123;
  with p^ do begin
    assign(f,'H:/HEAPIO.DAT');rewrite(f);
    i:=42;write(f,i);
    {$I-}seek(f,9);if ioresult<>$91 then halt;{$I+}
    if tag<>123 then halt;
  end;
  dispose(p);
  mark(markptr);new(p);
  assign(p^.f,'H:/RELEASE.DAT');rewrite(p^.f);
  i:=55;write(p^.f,i);release(markptr);
end;
begin
  nested(3);heapfiles;
  writeln('DISK-LIFETIME-OK');
end.
