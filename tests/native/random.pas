program RandomNumbers;
type bits=array[0..5]of byte;
const expected:array[0..15]of bits=(
      (126,0,164,139,101,88),
      (127,0,164,116,200,113),
      (126,0,236,24,107,4),
      (128,0,164,70,151,100),
      (127,0,26,247,167,76),
      (128,0,246,36,107,70),
      (126,0,124,11,92,72),
      (127,0,144,170,99,103),
      (128,0,49,220,181,2),
      (125,0,208,188,234,29),
      (126,0,12,30,10,34),
      (128,0,236,171,223,31),
      (128,0,213,4,207,69),
      (128,0,62,210,106,99),
      (128,0,39,84,237,78),
      (127,0,32,149,97,119));
      bounds:array[0..8]of integer=(1,2,3,7,255,256,257,16385,32767);
      values:array[0..8]of integer=(0,1,1,1,88,11,134,5694,20611);
      sumexpected:bits=(129,128,30,46,0,3);
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
