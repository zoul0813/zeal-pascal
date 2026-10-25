program ZeroRecords;
type empty=record end;
     data=record head:byte;n:integer;s:string[3] end;
const partial:data=(head:7);
var left:integer;z:empty;e:array[1..2] of empty;right:integer;seen:integer;
procedure value(v:empty;last:integer);
begin if last=123 then seen:=seen+1 end;
begin
 left:=321;right:=456;seen:=0;
 z:=e[1];e[2]:=z;z:=z;value(z,123);
 if (left=321) and (right=456) and (seen=1) and (sizeof(e)=0) and
    (partial.n=0) and (length(partial.s)=0) then writeln('ZERO-RECORD-COPY-OK');
end.
