program SetArguments;
type full=set of byte;small=set of 3..5;word=string[3];
var s:full;seen,n:integer;
function change:integer;
begin s:=[7];change:=123 end;
procedure accept(v:small;last:integer);
begin if (v=[3]) and (last=123) then seen:=seen+1;v:=[] end;
procedure alias(var v:full);
begin v:=v+[255] end;
procedure mixed(first:byte;v:small;text:word;last:integer);
begin if (first=171) and (v=[3]) and (text='abc') and (last=-123) then seen:=seen+1 end;
{$A-}
procedure recursive(level:integer;v:full);
var own:full;
begin
 own:=v+[level];
 if level>0 then recursive(level-1,own);
 if (level in own) and (own<=v+[level]) then seen:=seen+1
end;
begin
 seen:=0;s:=[3];accept(s,change);
 if (seen=1) and (s=[7]) then writeln('SET-VALUE-SNAPSHOT-ORDER-OK');
 alias(s);if (7 in s) and (255 in s) then writeln('SET-VAR-PARAMETER-OK');
 seen:=0;recursive(5,[]);
 if seen=6 then writeln('SET-RECURSIVE-VALUES-OK');
 seen:=0;s:=[3];for n:=1 to 1000 do mixed(171,s,'abcdef',-123);
 if (seen=1000) and (s=[3]) then writeln('SET-MIXED-ABI-CLEANUP-OK');
 seen:=0;for n:=1 to 1000 do begin s:=(s+[5])*[3..5];if 3 in s then seen:=seen+1 end;
 if (seen=1000) and (s=[3,5]) then writeln('SET-EXPRESSION-CLEANUP-OK');
end.
