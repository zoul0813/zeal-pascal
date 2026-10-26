program SquareRoots;
type bytes=array[0..5]of byte;
     guarded=record before:byte;value:real;after:byte end;
var x,y:real;g:guarded;raw,result:bytes;i,j,saved,checks:integer;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('SQRT-FAIL-',checks);halt end end;
function nested(n:integer):real;
var a:real;
 function inner:real;
 begin inner:=sqrt(a)end;
begin a:=n;nested:=inner end;
function recursive(n:integer):real;
begin if n=0 then recursive:=sqrt(4)else recursive:=sqrt(9)+recursive(n-1)end;
begin
 saved:=stackptr;g.before:=73;g.after:=149;
 check((sqrt(0)=0)and(sqrt(1)=1)and(sqrt(4)=2)and(sqrt(9)=3));
 check((sqrt(0.25)=0.5)and(sqrt(2.25)=1.5));
 x:=sqrt(2);check((x>1.41421356236)and(x<1.41421356238));
 check((nested(25)=5)and(recursive(3)=11));
 check(sqrt(sqrt(256))=4);
 for i:=0 to 181 do check(sqrt(i*i)=i);
 for i:=1 to 255 do begin
  raw[0]:=i;for j:=1 to 5 do raw[j]:=0;move(raw,x,6);
  g.value:=sqrt(x);move(g.value,result,6);
  check((g.before=73)and(g.after=149)and(result[0]=(i+129)div 2));
  if odd(i)then begin for j:=1 to 5 do check(result[j]=0)end
  else begin
   check((result[1]=249)and(result[2]=51)and(result[3]=243)and(result[4]=4)and(result[5]=53))
  end
 end;
 check(stackptr=saved);
 writeln('SQRT-ALL-OK');
end.
