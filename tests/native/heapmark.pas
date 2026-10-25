program HeapMarks;
type ip=^integer;link=^node;node=record n:integer;next:link end;
 empty=record end;ep=^empty;
var p,q,base:ip;head,tail:link;z:ep;
 before,after,markaddr,sp,i,seen:integer;
procedure selected;
var a:array[1..2]of ip;
 function tick:integer;begin seen:=seen+1;tick:=2 end;
begin seen:=0;new(a[tick]);a[2]^:=37;
 if(seen=1)and(a[2]^=37)then writeln('HEAP-SELECTED-TARGET-ONCE-OK')end;
begin
 mark(base);markaddr:=ord(base);
 before:=memavail;new(p);p^:=1234;after:=memavail;
 if(p^=1234)and(before-after=4)then writeln('HEAP-NEW-INTEGER-OK');
 before:=memavail;getmem(q,10);after:=memavail;
 if before-after=12 then writeln('HEAP-GETMEM-SIZE-OK');
 new(head);new(tail);head^.n:=17;tail^.n:=29;head^.next:=tail;tail^.next:=nil;
 if(head^.next^.n=29)and(head^.next^.next=nil)then writeln('HEAP-LINKED-RECORD-OK');
 selected;
 release(base);mark(q);
 if ord(q)=markaddr then writeln('HEAP-RELEASE-FRONTIER-OK');
 before:=memavail;
 for i:=1 to 100 do begin mark(base);new(p);p^:=i;release(base)end;
 after:=memavail;if before=after then writeln('HEAP-MARK-RELEASE-CLEANUP-OK');
 new(z);if z<>nil then writeln('HEAP-ZERO-WIDTH-NONNIL-OK');release(q);
 sp:=stackptr;stackptr:=sp;
 if stackptr=sp then writeln('HEAP-STACKPTR-RESTORE-OK');
 if((heapptr xor $8000)<(recurptr xor $8000))and
   ((recurptr xor $8000)<(stackptr xor $8000))then writeln('HEAP-OWNED-BOUNDS-OK');
end.
