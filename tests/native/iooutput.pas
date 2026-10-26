program OutputRecovery;
var saved,outer,i,calls,code:integer;s:string[8];
{$I-,A-}
function side:integer;
begin calls:=calls+1;side:=7 end;
begin
 if ioresult<>0 then code:=1 div 0;
 calls:=0;
 outer:=stackptr;
 for i:=1 to 100 do begin
  saved:=stackptr;
  writeln('rejected',side:1);code:=ioresult;
  if(code<>240)or(ioresult<>0)then code:=1 div 0;
  if stackptr<>saved then code:=1 div 0
 end;
 if stackptr<>outer then code:=1 div 0;
 if calls<>100 then code:=1 div 0;
 write('rejected');str(12,s);code:=ioresult;
 if(code<>240)or(s<>'12')or(ioresult<>0)then code:=1 div 0;
 write(ioresult:1);if ioresult<>240 then code:=1 div 0
end.
