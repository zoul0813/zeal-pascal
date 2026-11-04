{$V-}
type small=string[5];blob=record s:small;pad:array[1..250]of char end;
var r:blob;big:string[200];sp:integer;
procedure check(var v:small);
var guard:integer;
begin guard:=1234;
 if length(copy(v,1,255))<>200 then halt;
 if pos('X',v)<>1 then halt;
 if length(v+'!')<>201 then halt;
 if guard<>1234 then halt;
end;
begin
 sp:=stackptr;fillchar(r,sizeof(r),'X');r.s[0]:=#200;
 if length(copy(r.s,1,255))<>200 then halt;
 if pos('X',r.s)<>1 then halt;
 if length(r.s+'!')<>201 then halt;
 writeln(r.s);big:=copy(r.s,1,200);check(big);
 fillchar(r,sizeof(r),' ');
 if length(copy(r.s,1,255))<>32 then halt;
 if stackptr<>sp then halt;
 writeln('STRING-LENGTH-SAFE')
end.
