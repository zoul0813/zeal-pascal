program Cases;
type colour=(red,green,blue); digit=0..9;
var i,n:integer; c:char; b:boolean; shade:colour; d:digit; octet:byte;
const minus=-7; letter='m';
begin
  n:=0;
  case 2 of 1:n:=99; 2:n:=1; 3:n:=99 end;
  case 7 of 1:n:=99; 2:n:=99 end;
  if n=1 then writeln('CASE-SINGLE-MISS-OK');
  case minus of
    $8000..-8:n:=99;
    -7,-3..0,4,9..11:n:=2;
    1..3:n:=99;
  end;
  if n=2 then writeln('CASE-LISTS-RANGES-OK');
  case 32767 of $8000..32767:n:=3; else n:=99 end;
  case $8000 of $8000:n:=n+1; else n:=99 end;
  if n=4 then writeln('CASE-SIGNED-LIMITS-OK');
  c:='m'; b:=true; shade:=green; octet:=255; d:=9;
  case c of 'a'..'l':n:=99; letter,'z':n:=5 end;
  case b of false:n:=99; true:n:=n+1 end;
  case shade of red,blue:n:=99; green:n:=n+1 end;
  case octet of 0..254:n:=99; 255:n:=n+1 end;
  case d of 0..8:n:=99; 9:n:=n+1 end;
  if n=9 then writeln('CASE-ORDINAL-FAMILIES-OK');
  case 2 of
    1:n:=99;
    3:n:=99;
    else n:=10; n:=n+1;
  end;
  if n=11 then writeln('CASE-ELSE-SEQUENCE-OK');
  case 1 of
    1:case 2 of
      1:n:=99;
      2:if false then n:=99 else n:=12;
      else n:=99;
    end;
    else n:=99;
  end;
  if n=12 then writeln('CASE-NESTED-NEAREST-ELSE-OK');
  n:=0;
  case 1 of 1:n:=1; 1:n:=99 end;
  case 3 of 1..3:n:=n+1; 2..4:n:=99 end;
  case 1 of 2..1:n:=99 else n:=n+1 end;
  octet:=0;
  case octet of 256:n:=99; -1:n:=99; 0:n:=n+1 end;
  d:=1;
  case d of 0:n:=99; 1:n:=n+1 end;
  if n=5 then writeln('CASE-REFERENCE-EDGES-OK');
  n:=0;
  for i:=1 to 4500 do begin
    case i mod 3 of 0:n:=n+1; 1:; 2:case b of true:; false:n:=99 end end;
    case i of $8000..0:n:=99; else n:=n+1 end;
    case i of -1:n:=99 end;
  end;
  if (n=6000) and (i=4500) then writeln('CASE-STACK-CLEANUP-OK');
end.
