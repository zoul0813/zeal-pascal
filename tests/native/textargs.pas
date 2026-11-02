program TextArguments;
type item=record tag:integer;f:text;s:string[3];b:byte;c:char end;pointer=^item;
var p:pointer;
procedure selected(var t:text;var n:byte);
begin read(t,n)end;
{$A-}
procedure go;
var local:text;
begin
  new(p);p^.tag:=1234;
  with p^ do begin
    assign(f,'H:/TEXTARGS.TXT');rewrite(f);writeln(f,'abc 256 Q');close(f);
    reset(f);read(f,s);selected(f,b);read(f,c);read(f,c);
    if(tag<>1234)or(s<>'abc')or(b<>0)or(c<>'Q')then halt;
    close(f);
  end;
  dispose(p);
  assign(local,'H:/LOCALTEXT.TXT');rewrite(local);writeln(local,'LOCAL');close(local);
end;
begin recurptr:=stackptr-2048; go;writeln('TEXT-ARGUMENTS-OK')end.
