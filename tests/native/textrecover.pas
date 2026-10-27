program TextRecover;
var f:text;n:integer;b:byte;r:real;s:string[2];
begin
  assign(f,'H:/BADNUM.TXT');reset(f);n:=777;
  {$I-}readln(f,n);if(ioresult<>$10)or(n<>777)then halt;
  {$I+}readln(f,n);if n<>42 then halt;
  r:=2.5;{$I-}read(f,r);if(ioresult<>$10)or(r<>2.5)then halt;
  {$I+}readln(f);readln(f,b);if b<>0 then halt;
  read(f,s);if s<>'ab' then halt;
  read(f,s);if s<>'c' then halt;
  readln(f);close(f);writeln('TEXT-RECOVERY-OK');
end.
