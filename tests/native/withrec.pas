program WithRecords;
type cell=record n:integer;text:string[3] end;
     cells=array[1..3] of cell;
     holder=record c:cell;v:integer end;
var a,b:cell;table:cells;h:holder;n,seen,i:integer;
function index:integer;
begin seen:=seen+1;index:=i end;
procedure early(var target:cell);
var k:integer;
begin
 with target do for k:=1 to 3 do begin n:=n+1;exit end
end;
{$A-}
procedure recursive(level:integer;var target:cell);
var own:cell;
begin
 own.n:=level;
 with own do begin
   if level>0 then recursive(level-1,target) else early(target);
   if n=level then seen:=seen+1
 end
end;
{$W9}
procedure many;
begin with a,a,a,a,a,a,a,a,a do n:=12 end;
{$W4}
begin
 a.n:=1;b.n:=2;n:=9;
 with a,b do begin n:=3;text:='abc' end;
 if (a.n=1) and (b.n=3) and (n=9) then writeln('WITH-LAST-PRECEDENCE-OK');
 seen:=0;i:=1;table[1].n:=10;table[2].n:=20;
 with table[index] do begin i:=2;n:=n+1;text:='xy' end;
 if (seen=1) and (table[1].n=11) and (table[2].n=20) then writeln('WITH-BASE-EVALUATED-ONCE-OK');
 with h,c do begin n:=7;v:=8 end;
 if (h.c.n=7) and (h.v=8) then writeln('WITH-LIST-LEXICAL-LOOKUP-OK');
 seen:=0;
 with a do begin recursive(5,b);n:=n+10 end;
 if (seen=6) and (a.n=11) and (b.n=4) then writeln('WITH-RECURSIVE-EXIT-RESTORE-OK');
 many;
 if a.n=12 then writeln('WITH-W9-OK');
 for i:=1 to 1000 do with a do begin n:=n+1;early(b) end;
 if (a.n=1012) and (b.n=1004) then writeln('WITH-STACK-CLEANUP-OK');
end.
