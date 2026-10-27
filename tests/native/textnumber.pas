program TextNumber;
var f:text;n:integer;b:byte;r:real;s:string[20];c:char;
begin
  assign(f,'H:/NUMBERS.TXT');reset(f);
  read(f,n,b,r);readln(f,s);
  if(n<>-32768)or(b<>255)or(r<>1.5)or(s<>' xyz')then halt;
  readln(f,c,n);if(c<>'Q')or(n<>42)then halt;
  n:=777;r:=2.5;read(f,n,r);
  if(n<>777)or(r<>2.5)or not eof(f)then halt;
  close(f);writeln('TEXT-NUMBERS-OK');
end.
