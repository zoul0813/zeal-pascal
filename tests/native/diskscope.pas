program DiskScope;
type ints=file of integer;
var f,g:ints;files:array[1..20]of ints;t,other:text;i,v,err:integer;c:char;
const paths:array[0..3]of string[16]=
  ('H:/OUT0.TXT','H:/OUT1.TXT','H:/OUT2.TXT','H:/OUT3.TXT');
{$A-}
procedure walk(depth:integer);
var local:text;c:char;
begin
  Assign(local,'H:/SCOPETXT.TXT');Reset(local);
  if Eof(local)then halt;
  if depth>0 then walk(depth-1);
  Read(local,c);if c<>'A'then halt;
  ReadLn(local);Read(local,c);if c<>'B'then halt;
  Close(local);
end;
procedure emit(depth:integer);
var local:text;c:char;
begin
  Assign(local,paths[depth]);Rewrite(local);Write(local,'A');
  if depth>0 then emit(depth-1);
  Write(local,'B');Close(local);
  Reset(local);Read(local,c);if c<>'A'then halt;
  Read(local,c);if(c<>'B')or not Eof(local)then halt;Close(local);
end;
begin
  for i:=1 to 64 do begin
    Assign(f,'H:/SCOPEIO.DAT');Rewrite(f);v:=i;Write(f,v)
  end;
  Close(f);
  for i:=1 to 20 do begin Assign(files[i],'H:/SCOPEIO.DAT');Reset(files[i])end;
  for i:=1 to 20 do begin Read(files[i],v);if v<>64 then halt;Close(files[i])end;
  Assign(t,'H:/SCOPETXT.TXT');Rewrite(t);WriteLn(t,'A');Write(t,'B');Close(t);
  walk(3);emit(3);
  { Fault injection: temporarily move the path, then retry Close on the same FIB. }
  Assign(f,'H:/CLOSENAT.DAT');Rewrite(f);v:=123;Write(f,v);
  Assign(g,'H:/CLOSENAT.DAT');Rename(g,'H:/CLOSENAT.BAK');
  {$I-}Close(f);err:=IOResult;{$I+}if err<>1 then halt;
  Rename(g,'H:/CLOSENAT.DAT');Close(f);Reset(f);
  if FileSize(f)<>1 then halt;Read(f,v);if v<>123 then halt;Close(f);
  Assign(t,'H:/CLOSENAT.TXT');Rewrite(t);Write(t,'OK');
  Assign(other,'H:/CLOSENAT.TXT');Rename(other,'H:/CLOSETXT.BAK');
  {$I-}Close(t);err:=IOResult;{$I+}if err<>1 then halt;
  Rename(other,'H:/CLOSENAT.TXT');Close(t);Reset(t);
  Read(t,c);if c<>'O'then halt;Read(t,c);if(c<>'K')or not Eof(t)then halt;Close(t);
  WriteLn('DISK-SCOPE-OK');
end.
