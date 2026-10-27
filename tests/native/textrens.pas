program RenameResources;
var f:text;q:array[1..256]of text;
    i,j,n,count:integer;s:string[20];
begin
  {$I-}assign(f,'H:/RFULL.NEW');erase(f);n:=ioresult;
  if(n<>0)and(n<>1)then halt;
  {$I+}assign(f,'H:/RHOLD.TXT');rewrite(f);writeln(f,'hold');close(f);
  assign(f,'H:/RFULL.OLD');rewrite(f);writeln(f,'FULL');close(f);
  {$I-}i:=1;n:=0;count:=0;
  while(i<=256)and(n=0)do begin
    assign(q[i],'H:/RHOLD.TXT');reset(q[i]);n:=ioresult;
    if n=0 then begin count:=i;i:=i+1 end
  end;
  if(n<>$f3)or(count<1)then halt;
  close(q[count]);n:=ioresult;if n<>0 then halt;
  rename(f,'H:/RFULL.NEW');n:=ioresult;if n<>$f3 then halt;
  reset(q[count]);n:=ioresult;if n<>0 then halt;
  close(q[count]);n:=ioresult;if n<>0 then halt;
  for j:=1 to count-1 do begin close(q[j]);n:=ioresult;if n<>0 then halt end;
  {$I+}reset(f);readln(f,s);close(f);if s<>'FULL' then halt;
  writeln('NATIVE-RENAME-SPACE-OK');
end.
