program AbsoluteFrames;
var n,seen:integer;
procedure untyped(var v);var b:byte absolute v;begin b:=77 end;
{$A-}
function recur(depth:integer):integer;
var own,r:integer;b:byte absolute own;
begin
 own:=depth;if depth>0 then r:=recur(depth-1)else r:=0;
 if(b=depth)and(addr(b)=addr(own))then recur:=r+1 else recur:=0;
end;
procedure outer(var target:integer);
var local:integer;b:byte absolute local;
 procedure inner;
 var parent:byte absolute local;param:byte absolute target;
 begin parent:=31;param:=47 end;
begin local:=0;inner;
 if(local=31)and(target=47)and(b=31)then writeln('ABSOLUTE-LEXICAL-FRAMES-OK')end;
procedure value(v:integer);var b:byte absolute v;
begin b:=63;if v=63 then writeln('ABSOLUTE-VALUE-PARAMETER-OK')end;
procedure pass(var v:integer);var b:byte absolute v;
begin b:=81 end;
begin
 n:=0;untyped(n);if n=77 then writeln('ABSOLUTE-UNTYPED-VAR-OK');
 if recur(5)=6 then writeln('ABSOLUTE-RECURSIVE-FRAMES-OK');
 n:=0;outer(n);value(0);pass(n);
 if n=81 then writeln('ABSOLUTE-VAR-PARAMETER-OK');
 seen:=0;while seen<100 do begin untyped(n);pass(n);seen:=seen+1 end;
 if n=81 then writeln('ABSOLUTE-REPEATED-FRAMES-OK');
end.
