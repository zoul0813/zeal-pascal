program RealValues;
type number=real; item=record value:real;code:integer end;
     values=array[-1..1]of real;rp=^real;bytes=array[0..5]of byte;
const half=0.5;negative=-half;circle=pi;
      typed:number=1.25;arr:array[1..2]of real=(1,2.5);
      rec:item=(value:0.125;code:7);
var x,y:real;i,n:integer;a:values;r:item;p:rp;bits:bytes;
{$A-}
function mutate:real;
begin x:=20;mutate:=2.5 end;
function factorial(n:integer):real;
begin if n=0 then factorial:=1 else factorial:=n*factorial(n-1)end;
procedure parameters(value:real;var target:real;tail:integer);
begin value:=value+tail;target:=value end;
procedure nested;
var outer:real;
 function add(value:real):real;
 begin add:=outer+value end;
begin outer:=0.25;if add(0.5)=0.75 then writeln('REAL-LEXICAL-FRAME-OK')end;
begin
 if(sizeof(real)=6)and(addr(y)-addr(x)=6)then writeln('REAL-LAYOUT-OK');
 if(half=0.5)and(negative=-0.5)and(circle>3)and(circle<4)and(typed=1.25)then
  writeln('REAL-CONSTANTS-OK');
 if(arr[1]=1.0)and(arr[2]=2.5)and(rec.value=0.125)and(rec.code=7)then
  writeln('REAL-INITIALIZERS-OK');
 x:=1/3;move(x,bits,6);
 if(bits[0]=127)and(bits[1]=170)and(bits[2]=170)and(bits[3]=170)and
   (bits[4]=170)and(bits[5]=42)then writeln('REAL-THIRD-BITS-OK');
 y:=1.0;mem[addr(y)]:=128;for i:=1 to 4 do mem[addr(y)+i]:=255;
 mem[addr(y)+5]:=127;x:=1-y;move(x,bits,6);
 if(bits[0]=90)and(bits[1]=0)and(bits[2]=0)and(bits[3]=0)and
   (bits[4]=0)and(bits[5]=0)then writeln('REAL-CANCELLATION-BITS-OK');
 y:=1.0;for i:=1 to 128 do y:=y/2;
 if mem[addr(y)]=1 then writeln('REAL-MINIMUM-OK');
 y:=y/2;move(y,bits,6);
 if(bits[0]=0)and(bits[1]=0)and(bits[2]=0)and(bits[3]=0)and
   (bits[4]=0)and(bits[5]=0)then writeln('REAL-CANONICAL-UNDERFLOW-OK');
 x:=3;y:=(x+1.5)*2-1;
 if(y=8)and(7/2=3.5)and(2<2.5)and(2.5>=2)and(2.5<>2)then
  writeln('REAL-MIXED-ARITHMETIC-OK');
 if(round(-2.5)=-3)and(trunc(-2.75)=-2)and(frac(-2.75)=-0.75)and
   (int(-2.75)=-2)and(abs(-1.25)=1.25)and(sqr(1.5)=2.25)then
  writeln('REAL-PRIMITIVE-FUNCTIONS-OK');
 x:=1.25;y:=x+mutate;
 if(x=20)and(y=3.75)then writeln('REAL-LEFT-SNAPSHOT-OK');
 x:=1.25;parameters(x,y,2);
 if(x=1.25)and(y=3.25)then writeln('REAL-VALUE-VAR-ARGUMENTS-OK');
 parameters(2,y,3);
 if y=5 then writeln('REAL-INTEGER-ARGUMENT-PROMOTION-OK');
 a[-1]:=0.25;a[0]:=a[-1]+0.5;r.value:=a[0];p:=ptr(addr(r.value));
 p^:=p^+0.125;if r.value=0.875 then writeln('REAL-SELECTED-STORAGE-OK');
 with r do begin value:=value+0.125;
  if value=1 then writeln('REAL-WITH-PRESERVED-OK')end;
 if factorial(5)=120 then writeln('REAL-RECURSIVE-RESULT-OK');
 nested;
end.
