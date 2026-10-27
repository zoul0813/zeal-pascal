program NativeRename;
type path=string[40];ints=file of integer;
var f:text;g:ints;a:array[1..2]of text;
    s:path;i,n,calls,selected:integer;
procedure remove(name:path);
var q:text;code:integer;
begin
  {$I-}assign(q,name);erase(q);code:=ioresult;{$I+}
  if(code<>0)and(code<>1)then halt
end;
procedure move(var q:text;name:path);
begin rename(q,name) end;
function index:integer;
begin calls:=calls+1;index:=selected end;
function destination:path;
begin
  calls:=calls+1;selected:=2;
  assign(a[2],'H:/RNSIDE.TXT');rewrite(a[2]);write(a[2],'SIDE');close(a[2]);
  destination:='H:/RNNEST.NEW'
end;
function ignored:path;
begin calls:=calls+1;ignored:='H:/RNSKIPPED' end;
begin
  remove('H:/RNNEW.TXT');remove('H:/RNINT.NEW');remove('H:/RNNEST.NEW');
  assign(f,'H:/RNOLD.TXT');rewrite(f);writeln(f,'rename');close(f);
  move(f,'H:/RNNEW.TXT');reset(f);readln(f,s);close(f);
  if s<>'rename' then halt;
  {$I-}rename(f,'H:/RNNEW.TXT');n:=ioresult;
  if n<>$20 then halt;
  assign(f,'H:/RNOLD.TXT');reset(f);n:=ioresult;
  if n<>1 then halt;
  {$I+}assign(f,'H:/RNNEW.TXT');reset(f);
  {$I-}rename(f,'H:/RNOPEN.NEW');n:=ioresult;
  if n<>$20 then halt;
  {$I+}readln(f,s);close(f);if s<>'rename' then halt;
  assign(g,'H:/RNINT.OLD');rewrite(g);i:=456;write(g,i);close(g);
  rename(g,'H:/RNINT.NEW');reset(g);read(g,i);close(g);
  if i<>456 then halt;
  selected:=1;calls:=0;assign(a[1],'H:/RNNEST.OLD');rewrite(a[1]);
  write(a[1],'NEST');close(a[1]);rename(a[index],destination);
  if(calls<>2)or(selected<>2)then halt;
  reset(a[1]);readln(a[1],s);close(a[1]);if s<>'NEST' then halt;
  {$I-}assign(f,'H:/RNMISSING');rename(f,'H:/RNNOTMADE');
  calls:=0;rename(a[1],ignored);n:=ioresult;
  if(n<>1)or(calls<>1)then halt;
  {$I+}reset(a[1]);readln(a[1],s);close(a[1]);
  if s<>'NEST' then halt;
  writeln('NATIVE-RENAME-OK');
end.
