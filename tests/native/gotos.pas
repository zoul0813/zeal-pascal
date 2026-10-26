program Labels;
label again,10,done,000,12345678901234567890,
      restart,afterfor,withinfor,outerfor,afterwith,withinwith,caseout,value,afterboth;
type rec=record value:integer end;
var i,j,n,rounds,s,base:integer;r,q:rec;
{$A-}
procedure recursive(k:integer);
label local,again;
var n,s:integer;
begin
 n:=0;s:=stackptr;
 again:n:=n+1;if n<3 then goto again;
 if k>0 then recursive(k-1);
 goto local;n:=99;
 local:if(stackptr=s)and(n=3)then writeln('LABEL-RECURSIVE-OK');
end;
procedure nested;
label finish;
var n:integer;
 procedure child;
 label finish;
 begin goto finish;finish:n:=n+1 end;
begin n:=0;child;goto finish;n:=9;
 finish:if n=1 then writeln('LABEL-LEXICAL-SHADOW-OK')end;
label unused;
begin
 base:=stackptr;n:=0;
 again:n:=n+1;if n<3 then goto again;
 goto 10;n:=99;
 10:if n=3 then writeln('LABEL-FORWARD-BACKWARD-OK');
 goto 000;n:=99;
 000:goto 12345678901234567890;n:=99;
 12345678901234567890:writeln('LABEL-NUMERIC-TEXT-OK');
 rounds:=0;
 restart:s:=stackptr;for i:=1 to 3 do goto afterfor;
 afterfor:if stackptr<>s then halt;
 rounds:=rounds+1;if rounds<30 then goto restart;
 writeln('LABEL-FOR-LEAVE-STACK-OK');
 n:=0;for i:=1 to 3 do begin
 withinfor:n:=n+1;if n=1 then goto withinfor end;
 if n=4 then writeln('LABEL-FOR-STAY-OK');
 n:=0;for i:=1 to 3 do begin
 s:=stackptr;for j:=1 to 2 do goto outerfor;
 outerfor:if stackptr=s then n:=n+1 end;
 if n=3 then writeln('LABEL-NESTED-FOR-STACK-OK');
 r.value:=0;s:=stackptr;
 with r do begin value:=19;goto afterwith;value:=99 end;
 afterwith:if(stackptr=s)and(r.value=19)then writeln('LABEL-WITH-LEAVE-STACK-OK');
 with r do begin
 withinwith:value:=value+1;if value<22 then goto withinwith end;
 if r.value=22 then writeln('LABEL-WITH-STAY-OK');
 with r do begin value:=23;goto value end;
 value:if r.value=23 then writeln('LABEL-FIELD-VALUE-OK');
 s:=stackptr;
 with r do with q do begin value:=29;goto afterboth end;
 afterboth:if(stackptr=s)and(q.value=29)then writeln('LABEL-NESTED-WITH-STACK-OK');
 n:=0;case 1 of 1:begin n:=7;goto caseout end;else n:=9 end;
 caseout:if(stackptr=base)and(n=7)then writeln('LABEL-CASE-LEAVE-STACK-OK');
 nested;recursive(2);
 goto done;n:=99;done:
end.
