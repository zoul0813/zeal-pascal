program StringOperations;
type text=string[5];
var s:text; result:string[255]; i,seen,order:integer; ch:char;
function value:text;
begin value:=s end;
function first:text;
begin order:=order*10+1; first:='ab' end;
function second:text;
begin order:=order*10+2; second:='cd' end;
function change:integer;
begin s:='XXXXX'; change:=2 end;
function changed:text;
begin s:='zz'; changed:='abcde' end;
begin
 s:='abcde';
 if (copy(s,2,3)='bcd') and (copy(s,4,32767)='de') and
    (copy(s,6,2)='') and (copy(s,255,2)='') and (copy(s,1,0)='') then writeln('COPY-CLIP-EMPTY-OK');
 if (pos('bc',s)=2) and (pos('de',s)=4) and (pos('',s)=0) and
    (pos('abcdef',s)=0) and (pos('z',s)=0) and (pos(s,s)=1) then writeln('POS-BOUNDARIES-OK');
 ch:='x';
 if (copy(ch,1,9)='x') and (pos(ch,'axb')=2) and (concat(ch,'',ch)='xx') and
    (concat('a')='a') then writeln('STRING-CHAR-ARGUMENTS-OK');
 order:=0; result:=concat(first,second,copy('efgh',1,2));
 if (result='abcdef') and (order=12) then writeln('STRING-ARGUMENT-ORDER-OK');
 s:='abcde'; result:=copy(s,change,2);
 if result='bc' then begin
   s:='bc'; if pos(s,changed)=2 then begin
     s:='abcde'; insert(s,s,change);
     if s='Xabcd' then writeln('STRING-ARGUMENT-SNAPSHOT-OK')
   end
 end;
 s:='abcde'; delete(s,2,2);
 if s='ade' then begin delete(s,3,32767); delete(s,255,1); delete(s,1,0);
   if s='ad' then writeln('DELETE-CLIP-ZERO-OK') end;
 s:='abcd'; insert('XYZ',s,2);
 if s='aXYZb' then begin insert('',s,1); insert('q',s,255);
   if s='aXYZb' then writeln('INSERT-CAPACITY-APPEND-OK') end;
 s:='ab'; insert(s,s,2);
 if s='aabb' then begin insert('Z',s,255); if s='aabbZ' then writeln('INSERT-SELF-ALIAS-OK') end;
 seen:=0;
 for i:=1 to 1000 do begin
   s:='abcde'; result:=concat(copy(value,2,2),copy('xy',1,1));
   if (result='bcx') and (pos(copy(value,2,2),value)=2) then seen:=seen+1;
   delete(s,2,length(copy('zz',1,1))); insert(copy(value,2,2),s,2)
 end;
 if (seen=1000) and (s='acdcd') then writeln('STRING-OPERATIONS-CLEANUP-OK');
end.
