program StringLifetimes;
type text=string[12];
var s:text; seen:integer;
function empty:text;
begin empty:='' end;
function touch:text;
begin s:='z'; touch:='a' end;
function state:text;
const calls:integer=0;
begin calls:=calls+1; if calls=1 then state:='one' else state:='two' end;
function step(x:integer):text;
begin seen:=seen*10+x; step:='x' end;
{$A-}
function recursive(x:integer):text;
begin if x=0 then recursive:='end' else recursive:=recursive(x-1) end;
function nesting(x:integer):text;
var captured:text;
 function left(k:integer):text; forward;
 function right(k:integer):text;
 begin if k=0 then right:=captured else right:=left(k-1) end;
 function left;
 begin left:=right(k) end;
begin captured:='outer'; nesting:=left(x) end;
begin
 if (state+state='onetwo') then writeln('STATIC-RESULT-COPY-OK');
 if (recursive(8)='end') and (nesting(4)='outer') then writeln('RECURSIVE-STRING-RESULT-OK');
 s:='a';
 if s=touch then writeln('STRING-COMPARE-SNAPSHOT-OK');
 s:='a';
 if s+touch='aa' then writeln('STRING-CONCAT-SNAPSHOT-OK');
 if ('a'+'b'='ab') and (empty+'x'='x') and ('x'+empty='x') then writeln('CONCAT-CHAR-EMPTY-OK');
 seen:=0; s:=step(1)+step(2)+step(3);
 if (seen=123) and (s='xxx') then writeln('CONCAT-ORDER-OK');
end.
