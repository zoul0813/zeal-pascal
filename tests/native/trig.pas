program Trigonometry;
type bytes=array[0..5]of byte;
const largest:bytes=(255,255,255,255,255,127);
      minimum:bytes=(1,0,0,0,0,0);
      sinlow:bytes=(128,189,54,173,45,58);
      sinhigh:bytes=(128,193,54,173,45,58);
      coslow:bytes=(128,110,36,118,181,47);
      coshigh:bytes=(128,114,36,118,181,47);
      atanlow:bytes=(129,31,162,218,15,73);
      atanhigh:bytes=(129,35,162,218,15,73);
var x,y,lo,hi:real;i,saved,checks:integer;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('TRIG-FAIL-',checks);halt end end;
function nested(n:integer):real;
var a:real;
 function inner:real;
 begin inner:=sqr(sin(a))+sqr(cos(a))end;
begin a:=n;nested:=inner end;
function recursive(n:integer):real;
begin if n=0 then recursive:=cos(0)else recursive:=cos(0)+recursive(n-1)end;
begin
 saved:=stackptr;
 check((sin(0)=0)and(cos(0)=1)and(arctan(0)=0));
 x:=sin(1);check((x>0.84147098479)and(x<0.84147098482));
 x:=cos(1);check((x>0.54030230585)and(x<0.54030230589));
 x:=arctan(1);check((x>0.78539816338)and(x<0.78539816342));
 check((abs(nested(25)-1)<0.000000001)and(recursive(3)=4));
 move(largest,x,6);move(sinlow,lo,6);move(sinhigh,hi,6);
 y:=sin(x);check((y>=lo)and(y<=hi));
 move(coslow,lo,6);move(coshigh,hi,6);y:=cos(x);check((y>=lo)and(y<=hi));
 move(atanlow,lo,6);move(atanhigh,hi,6);y:=arctan(x);check((y>=lo)and(y<=hi));
 move(minimum,x,6);check((sin(x)=0)and(arctan(x)=0)and(cos(x)=1));
 minimum[1]:=1;move(minimum,x,6);check((sin(x)=x)and(arctan(x)=x));
 for i:=-8 to 8 do begin
  x:=i;check((sin(-x)=-sin(x))and(cos(-x)=cos(x))and(arctan(-x)=-arctan(x)));
  check(abs(sqr(sin(x))+sqr(cos(x))-1)<0.000000001)
 end;
 check(stackptr=saved);writeln('TRIG-ALL-OK');
end.
