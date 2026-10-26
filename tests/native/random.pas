program RandomNumbers;
type bits=array[0..5]of byte;
const expected:array[0..15]of bits=(
      (127,0,62,142,93,1),
      (128,0,25,12,224,17),
      (128,0,40,225,15,28),
      (127,0,58,237,225,74),
      (124,0,192,44,224,124),
      (126,0,156,23,173,117),
      (128,0,140,110,38,111),
      (127,0,46,2,16,120),
      (128,0,66,63,113,121),
      (124,0,224,28,177,91),
      (128,0,239,39,222,31),
      (126,0,248,150,122,125),
      (128,0,10,25,203,22),
      (127,0,62,184,92,17),
      (128,0,169,76,146,102),
      (127,0,212,188,192,31));
      bounds:array[0..8]of integer=(1,2,3,7,255,256,257,16385,32767);
      values:array[0..8]of integer=(0,0,1,2,192,114,122,2040,9597);
      sumexpected:bits=(128,0,44,139,20,85);
var x,last:real;raw:bits;i,j,n,saved,checks,changes,calls:integer;
function next:real;
begin next:=random end;
function bounded(n:integer):integer;
begin bounded:=random(n)end;
function bound:integer;
begin calls:=calls+1;bound:=16385 end;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('RANDOM-FAIL-',checks);halt end end;
{$A-}
function recursive(n:integer):integer;
var k:integer;
begin k:=random(1);if n=0 then recursive:=k else recursive:=k+recursive(n-1)end;
begin
 saved:=stackptr;
 { SizeOf must not evaluate an index or create private generator state. }
 check(sizeof(bounds[random(9)])=2);
 for i:=0 to 15 do begin
  x:=next;move(x,raw,6);
  for j:=0 to 5 do check(raw[j]=expected[i,j]);
  check((x>=0)and(x<1))
 end;
 for i:=0 to 8 do check(bounded(bounds[i])=values[i]);
 x:=random+random;move(x,raw,6);
 for j:=0 to 5 do check(raw[j]=sumexpected[j]);
 n:=random(bound);check((calls=1)and(n>=0)and(n<16385));
 for i:=1 to 1024 do begin
  n:=random(32767);check((n>=0)and(n<32767));
  n:=random(16385);check((n>=0)and(n<16385));
  x:=random;check((x>=0)and(x<1))
 end;
 check(recursive(20)=0);
 last:=random;changes:=0;
 for i:=1 to 32 do begin
  randomize;x:=random;check((x>0)and(x<1));
  if x<>last then changes:=changes+1;last:=x;
  n:=random(7);check((n>=0)and(n<7))
 end;
 check(changes>0);check(stackptr=saved);
 writeln('RANDOM-ALL-OK')
end.
