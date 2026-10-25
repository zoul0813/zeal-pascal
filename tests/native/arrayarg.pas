program ArrayArguments;
type vector=array[1..3] of integer;
     short=string[3]; long=string[5];
var a:vector; seen,n:integer;
function change:integer;
begin a[1]:=9; change:=123 end;
procedure accept(v:vector; x:integer);
begin
 if (v[1]=9) and (x=123) then seen:=seen+1;
 v[1]:=77
end;
procedure alias(var v:vector);
 procedure inner(var x:vector);
 begin x[2]:=88 end;
begin inner(v) end;
{$A-}
procedure recursive(level:integer; v:vector; var dest:vector);
var own:vector;
begin
 own:=v; own[3]:=level;
 if level>0 then recursive(level-1,own,dest) else dest:=own;
 if own[3]=level then seen:=seen+1
end;
procedure values(v:vector; left:short; last:byte; right:long);
begin
 if (v[1]=9) and (left='abc') and (last=255) and (right='xy') then seen:=seen+1
end;
begin
 a[1]:=1; a[2]:=2; a[3]:=3; seen:=0;
 accept(a,change);
 if (seen=1) and (a[1]=9) then writeln('ARRAY-VALUE-COPY-TIMING-OK');
 alias(a); if a[2]=88 then writeln('ARRAY-VAR-LEXICAL-OK');
 seen:=0; recursive(5,a,a);
 if (seen=6) and (a[3]=0) and (a[2]=88) then writeln('ARRAY-RECURSIVE-COPY-OK');
 seen:=0;
 for n:=1 to 1000 do values(a,'abcdef',255,'xy');
 if (seen=1000) and (a[1]=9) then writeln('ARRAY-STRING-ABI-CLEANUP-OK');
end.
