program PointerArguments;
type pint=^integer; pp=^pint; other=^integer;
var x,y:integer;p,q:pint;compatible:other;table:array[1..2]of pint;count,i,seen:integer;
procedure value(v:pint);begin v^:=41;v:=nil end;
procedure reference(var v:pint);begin v:=ptr(addr(y));v^:=83 end;
procedure untyped(var v);var local:pp;begin local:=ptr(addr(v));local^^:=89 end;
function result:pint;begin result:=p end;
function tick:integer;begin count:=count+1;tick:=2 end;
procedure nested;
type pint=^target;target=record n:integer end;
var local:target;own:pint;
begin own:=ptr(addr(local));own^.n:=67;
 if own^.n=67 then writeln('POINTER-LOCAL-FORWARD-OK')end;
{$A-}
procedure recurse(depth:integer;v:pint);
var local:integer;own:pint;
begin
 local:=depth;own:=ptr(addr(local));
 if depth>0 then recurse(depth-1,v);
 own^:=own^+10;
 if local=depth+10 then seen:=seen+1;
end;
begin
 x:=1;y:=2;p:=ptr(addr(x));value(p);
 if(p<>nil)and(x=41)then writeln('POINTER-VALUE-ARGUMENT-OK');
 compatible:=p;reference(compatible);p:=compatible;if(ord(p)=addr(y))and(y=83)then writeln('POINTER-VAR-ARGUMENT-OK');
 untyped(p);if y=89 then writeln('POINTER-UNTYPED-ARGUMENT-OK');
 q:=result;if q=p then writeln('POINTER-FUNCTION-RESULT-OK');
 table[2]:=p;count:=0;table[tick]^:=97;
 if(count=1)and(y=97)then writeln('POINTER-SELECTED-LHS-ONCE-OK');
 count:=0;i:=sizeof(table[tick]^);
 if(count=0)and(i=2)then writeln('POINTER-SIZEOF-NO-EVAL-OK');
 nested;seen:=0;recurse(3,p);
 if seen=4 then writeln('POINTER-RECURSIVE-FRAME-OK');
 for i:=1 to 200 do begin value(p);reference(p);q:=result end;
 if(q=p)and(y=83)then writeln('POINTER-ARGUMENT-CLEANUP-OK');
end.
