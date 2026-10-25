program Arrays;
type numbers=array[-2..2] of integer;
     row=packed array[1..3] of byte;
     matrix=array[false..true] of row;
     color=(red,green,blue);
     flags=array[color,boolean] of char;
     words=array[boolean] of string[5];
const initial:numbers=(11,22,33,44,55);
      rows:matrix=((1,2,3),(4,5,6));
      labels:words=('abc','def');
var a,b:numbers; m:matrix; f:flags; w:words; i,seen,order:integer;
procedure put(var n:integer);
begin n:=77 end;
function index:integer;
begin seen:=seen+1; order:=order*10+1; index:=-1 end;
function value:integer;
begin order:=order*10+2; value:=99 end;
begin
 {$R+}
 a:=initial; b:=a; a[0]:=9;
 if (b[-2]=11) and (b[0]=33) and (b[2]=55) then writeln('ARRAY-WHOLE-COPY-OK');
 put(a[-2]); seen:=0; order:=0; a[index]:=value;
 if (a[-2]=77) and (a[-1]=99) and (seen=1) and (order=12) then writeln('ARRAY-SELECT-ORDER-VAR-OK');
 m:=rows; m[true,2]:=9; m[false]:=m[true];
 if (m[false][2]=9) and (m[true][3]=6) and (sizeof(m)=6) and
    (addr(m[true])-addr(m)=3) then writeln('ARRAY-MULTIDIM-LAYOUT-OK');
 f[red,false]:='a'; f[blue,true]:='z';
 if (f[red][false]='a') and (f[blue][true]='z') and (sizeof(f)=6) then writeln('ARRAY-ORDINAL-INDEX-OK');
 w:=labels; w[false]:='xy'; w[true][2]:='Z';
 if (w[false]='xy') and (w[true]='dZf') and (sizeof(w[false])=6) then writeln('ARRAY-STRING-SELECTION-OK');
 initial[0]:=88;
 if (initial[0]=88) and (b[0]=33) then writeln('ARRAY-MUTABLE-INITIALIZER-OK');
 seen:=0;
 if (sizeof(a[index])=2) and (seen=0) then writeln('ARRAY-SIZEOF-NO-EVAL-OK');
 for i:=1 to 1000 do begin a[index]:=value; b:=a end;
 if (seen=1000) and (b[-1]=99) then writeln('ARRAY-SELECT-CLEANUP-OK');
end.
