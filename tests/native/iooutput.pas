program OutputRecovery;
var saved,outer,i,calls,code:integer;s:string[8];
{$I-,A-}
function side:integer;
begin calls:=calls+1;side:=7 end;
begin
 if ioresult<>0 then halt;
 calls:=0;
 outer:=stackptr;
 for i:=1 to 100 do begin
  saved:=stackptr;
  writeln('rejected',side:1);code:=ioresult;
  if(code<>240)or(ioresult<>0)then halt;
  if stackptr<>saved then halt
 end;
 if stackptr<>outer then halt;
 if calls<>100 then halt;
 write('rejected');str(12,s);code:=ioresult;
 if(code<>240)or(s<>'12')or(ioresult<>0)then halt;
 write(ioresult:1);if ioresult<>240 then halt
end.
