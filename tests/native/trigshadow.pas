program ShadowTrigonometry;
function sin(n:integer):integer;
begin sin:=n+1 end;
function cos(n:integer):integer;
begin cos:=n+2 end;
function arctan(n:integer):integer;
begin arctan:=n+3 end;
begin
 if(sin(4)=5)and(cos(4)=6)and(arctan(4)=7)then writeln('TRIG-SHADOW-OK');
end.
