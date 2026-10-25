program Control;
var i,j,n,limit:integer; b:boolean; tail:integer; a,c:string[8];
begin
  tail:=1234; b:=true; b:=false;
  if tail=1234 then writeln('BOOL-LAYOUT-OK');
  b:=not false and true xor false;
  writeln(b,',',not b);
  if b then if false then writeln('BAD') else writeln('NEAREST-ELSE-OK');
  n:=0;
  for i:=1 to 5 do n:=n+i;
  if (n=15) and (i=5) then writeln('FOR-UP-OK');
  n:=0;
  for i:=3 downto 1 do n:=n+i;
  if (n=6) and (i=1) then writeln('FOR-DOWN-OK');
  i:=99;
  for i:=5 to 3 do writeln('BAD');
  if i=99 then writeln('FOR-ZERO-TRIP-OK');
  j:=99;
  for j:=1 downto 3 do writeln('BAD');
  if j=99 then writeln('FOR-ZERO-DOWN-OK');
  limit:=3; n:=0;
  for i:=1 to limit do begin limit:=0; n:=n+i end;
  if (n=6) and (i=3) and (limit=0) then writeln('FOR-BOUND-ONCE-OK');
  n:=0;
  for i:=1 to 3 do for j:=1 to 2 do n:=n+1;
  if (n=6) and (i=3) and (j=2) then writeln('NESTED-FOR-OK');
  n:=0;
  for i:=32766 to 32767 do n:=n+1;
  for j:=-32767 downto -32768 do n:=n+1;
  if (n=4) and (i=32767) and (j=-32768) then writeln('FOR-LIMITS-OK');
  n:=0;
  for i:=-2 to 1 do n:=n+i;
  for j:=1 downto -2 do n:=n+j;
  if n=-4 then writeln('FOR-SIGNED-OK');
  n:=0;
  for b:=false to true do n:=n+1;
  if (n=2) and b and (tail=1234) then writeln('FOR-BOOLEAN-OK');
  n:=0;
  while false do n:=99;
  repeat n:=n+1 until n>=3;
  while n>0 do n:=n-1;
  if n=0 then writeln('WHILE-REPEAT-OK');
  if (-32768<32767) and (32767>-32768) and (-1<0) and (0>=0)
    and (1<>2) and (2<=2) and (false<true) then writeln('COMPARE-OK');
  if ((not 0)=$FFFF) and (($FF00 and $0FF0)=$0F00)
    and (($AA or $55)=$FF) and (($AA xor $FF)=$55) then writeln('BITWISE-OK');
  if (($FFFF shr 1)=$7FFF) and ((1 shl 0)=1) and ((1 shl 16)=0)
    and ((1 shl 17)=0) and (($8000 shr 15)=1) then writeln('SHIFTS-OK');
  a:='abc'; c:='abcd';
  if (a<c) and (c>a) and (a<=a) and (a>=a) and (a<>c)
    and (''<'a') and ('x'='x') and ('b'>'aa') then writeln('STR-COMPARE-OK');
end.
