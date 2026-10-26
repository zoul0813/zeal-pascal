program ShadowRandom;
var seed:integer;
function random(n:integer):integer;
begin random:=n+1 end;
procedure randomize;
begin seed:=7 end;
begin
 randomize;
 if(seed=7)and(random(4)=5)then writeln('RANDOM-SHADOW-OK')
end.
