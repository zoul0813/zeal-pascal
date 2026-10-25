program StringSelections;
type text=string[5];
var s:text; i,seen,order:integer; c:char;
function later:integer; forward;
procedure noeval;
begin
 seen:=0;
 if (sizeof(s[later])=1) and (seen=0) then seen:=10
end;
function later;
begin seen:=seen+1; later:=1 end;
procedure put(var ch:char);
begin ch:='Z' end;
function index:integer;
begin seen:=seen+1; order:=order*10+1; index:=2 end;
function value:text;
begin order:=order*10+2; value:='Q' end;
procedure local(n:integer; var dest:text);
var own:text;
begin
 own:='abc'; put(own[2]);
 if n>0 then local(n-1,dest) else dest:=own;
 if own='aZc' then seen:=seen+1
end;
begin
 {$R+}
 s:='abc'; put(s[2]);
 if (s='aZc') and (ord(s[0])=3) and (s[1]='a') then writeln('SELECT-CHAR-VAR-OK');
 s:='a'; s[4]:='Z'; s[0]:=#4;
 if (length(s)=4) and (ord(s[4])=90) then writeln('SELECT-CAPACITY-ZERO-OK');
 seen:=0; order:=0; s:='abc'; s[index]:=value;
 if (s='aQc') and (seen=1) and (order=12) then writeln('SELECT-EVALUATION-ORDER-OK');
 seen:=0;
 if (sizeof(s[index])=1) and (sizeof(s[sizeof(s[index])])=1) and
    (seen=0) and (addr(s[2])-addr(s)=2) then begin
   noeval; if seen=10 then writeln('SELECT-SIZEOF-ADDR-OK')
 end;
 seen:=0; local(3,s);
 if (s='aZc') and (seen=4) then writeln('SELECT-RECURSIVE-LOCAL-OK');
 seen:=0;
 for i:=1 to 1000 do begin s[2]:=value; c:=s[2]; if c='Q' then seen:=seen+1 end;
 if (seen=1000) and (s='aQc') then writeln('SELECT-TEMPORARY-CLEANUP-OK');
end.
