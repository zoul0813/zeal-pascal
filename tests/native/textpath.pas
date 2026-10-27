program NativePaths;
var f:file of integer;t:text;i,n:integer;s:string[20];
begin
  assign(f,'./../MixedName');rewrite(f);i:=314;write(f,i);close(f);
  assign(f,'/./MixedName');reset(f);read(f,i);close(f);
  if i<>314 then halt;
  {$I-}assign(f,'H:relative');n:=ioresult;
  if n<>$20 then halt;
  assign(f,'H:/bad'#0'name');n:=ioresult;
  if n<>$20 then halt;
  {$I+}reset(f);read(f,i);close(f);if i<>314 then halt;
  assign(t,'h://./NoExtension');rewrite(t);writeln(t,'native');close(t);
  assign(t,'../NoExtension');reset(t);readln(t,s);close(t);
  if s<>'native' then halt;
  writeln('NATIVE-PATHS-OK');
end.
