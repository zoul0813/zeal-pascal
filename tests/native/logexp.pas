program LogarithmsAndExponentials;
var x,y:real;i,j,saved,checks:integer;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('LOGEXP-FAIL-',checks);halt end end;
function nested(n:integer):real;
var a:real;
 function inner:real;
 begin inner:=exp(ln(a))end;
begin a:=n;nested:=inner end;
function recursive(n:integer):real;
begin if n=0 then recursive:=exp(0)else recursive:=exp(0)+recursive(n-1)end;
begin
 saved:=stackptr;
 check((ln(1)=0)and(exp(0)=1));
 x:=ln(2);check((x>0.69314718055)and(x<0.69314718057));
 y:=exp(1);check((y>2.71828182844)and(y<2.71828182848));
 check((abs(nested(25)-25)<0.000000001)and(recursive(3)=4));
 x:=1.0;mem[addr(x)+1]:=1;x:=ln(x);
 check((x>0)and(x<0.000000000002));
 x:=1.0;mem[addr(x)]:=128;for j:=1 to 4 do mem[addr(x)+j]:=255;
 mem[addr(x)+5]:=127;x:=ln(x);
 check((x<0)and(x> -0.000000000002));
 x:=exp(-88);check((x>0)and(x<1.0e-38));
 x:=exp(-89);check(x=0);
 for i:=-80 to 80 do begin
  x:=exp(i);y:=ln(x);check(abs(y-i)<0.000000001)
 end;
 check(stackptr=saved);
 writeln('LOGEXP-ALL-OK');
end.
