program Records;
type item=record c:char; n:integer; text:string[4] end;
     items=array[-1..1] of item;
     outer=packed record head:byte; list:items; tail:char end;
     empty=record end;
const initial:item=(c:'a';n:123;text:'xy');
      partial:item=(c:'q');
      many:items=((c:'b';n:1;text:'one'),(c:'c';n:2;text:'two'),(c:'d';n:3;text:'tri'));
var a,b:item; group:outer; z:empty; e:array[1..2] of empty; seen,order,i:integer;
procedure put(var n:integer);
begin n:=99 end;
function index:integer;
begin seen:=seen+1;order:=order*10+1;index:=0 end;
function value:integer;
begin order:=order*10+2;value:=77 end;
procedure copy(v:item; var out:item);
begin v.n:=456;out:=v end;
begin
 {$R+}
 a:=initial;b:=a;put(a.n);
 if (a.n=99) and (b.n=123) and (sizeof(item)=8) and
    (addr(a.n)-addr(a)=1) and (addr(a.text)-addr(a)=3) then writeln('RECORD-LAYOUT-COPY-VAR-OK');
 group.list:=many;seen:=0;order:=0;group.list[index].n:=value;
 group.list[1].text[2]:='Z';
 if (group.list[0].n=77) and (seen=1) and (order=12) and
    (group.list[1].text='tZi') and (sizeof(outer)=26) then writeln('RECORD-ARRAY-SELECTION-OK');
 copy(a,b);
 if (a.n=99) and (b.n=456) then writeln('RECORD-VALUE-ISOLATION-OK');
 initial.n:=321;if initial.n=321 then writeln('RECORD-MUTABLE-INITIALIZER-OK');
 if partial.c='q' then writeln('RECORD-PARTIAL-INITIALIZER-OK');
 seen:=0;if (sizeof(group.list[index].n)=2) and (seen=0) then writeln('RECORD-SIZEOF-NO-EVAL-OK');
 if (sizeof(z)=0) and (sizeof(e)=0) then writeln('RECORD-ZERO-WIDTH-OK');
 seen:=0;
 for i:=1 to 1000 do begin group.list[index].n:=value;b:=group.list[0] end;
 if (seen=1000) and (b.n=77) then writeln('RECORD-SELECTION-CLEANUP-OK');
end.
