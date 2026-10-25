program HeapIndividualFree;
type ip=^integer;var a,b,c,d:ip;before,after,freeaddr,i:integer;
begin
 before:=memavail;new(a);a^:=41;freeaddr:=ord(a);dispose(a);after:=memavail;
 if(before=after)and(ord(a)=freeaddr)then writeln('HEAP-DISPOSE-RESTORE-OK');
 before:=memavail;getmem(a,10);freemem(a,10);after:=memavail;
 if before=after then writeln('HEAP-FREEMEM-RESTORE-OK');
 getmem(a,20);getmem(b,20);getmem(c,20);freeaddr:=ord(b);
 freemem(b,20);getmem(d,6);
 if ord(d)=freeaddr then writeln('HEAP-FIRST-FIT-SPLIT-OK');
 freemem(d,6);freemem(a,20);getmem(d,42);
 if(ord(d) xor $8000)<(ord(c) xor $8000)then writeln('HEAP-COALESCE-NEIGHBORS-OK');
 freemem(c,20);freemem(d,42);
 after:=memavail;if after=before then writeln('HEAP-COALESCE-TAIL-RESTORE-OK');
 before:=memavail;
 for i:=1 to 100 do begin getmem(a,1);freemem(a,1)end;
 after:=memavail;if before=after then writeln('HEAP-ODD-SIZE-CLEANUP-OK');
 before:=memavail;after:=maxavail;
 if(before=after)and(before>0)then writeln('HEAP-MAXAVAIL-OK');
end.
