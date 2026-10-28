program UFrame;
{$A-,U+}
type pair=record a,b:integer end;
var p:pair;saved:integer;
function total(n:integer):integer;
var local:pair;
 function child(k:integer):integer;
 begin child:=local.a+k end;
begin
 local.a:=n;local.b:=n+1;
 with local do begin
  if n=0 then total:=a+b else total:=child(b)+total(n-1);
  if(a<>n)or(b<>n+1)then halt;
 end;
end;
begin
 saved:=stackptr;p.a:=10;p.b:=20;
 with p do if(total(6)<>49)or(a<>10)or(b<>20)then halt;
 if stackptr<>saved then halt;
 writeln('U-FRAME-OK');
end.
