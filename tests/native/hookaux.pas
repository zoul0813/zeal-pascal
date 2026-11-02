program HookAux;
type bytes=array[0..2]of byte;
 numbers=file of integer;holder=record f:numbers end;pointer=^holder;
var machine:bytes;
{$A-}
procedure native(c:char);external $8000;
procedure install(address:integer);
begin machine[0]:=195;machine[1]:=lo(address);machine[2]:=hi(address)end;
procedure outer;
var olda,oldl,saved,code,hits,number:integer;buffer:string[20];
 probe,keep:numbers;heap:pointer;
procedure serial(c:char);
begin buffer:=buffer+c;install(olda);native('N')end;
procedure printer(c:char);
var f:numbers;n:integer;
begin
 assign(f,'H:/HOOKUNW.DAT');rewrite(f);n:=77;write(f,n);close(f);
 hits:=hits+1;install(oldl);native(c)
end;
begin
 olda:=auxoutptr;oldl:=lstoutptr;saved:=stackptr;hits:=0;buffer:='';
 auxoutptr:=addr(serial);lstoutptr:=addr(printer);
 assign(keep,'H:/HOOKKEEP.DAT');rewrite(keep);number:=11;write(keep,number);
 new(heap);assign(heap^.f,'H:/HOOKHEAP.DAT');rewrite(heap^.f);number:=33;write(heap^.f,number);
 install(olda);native('S');write(Aux,'C');
 {$I-}write(Lst,'X');write(Aux,'suppressed');code:=ioresult;{$I+}
 if(code<>32)or(hits<>1)or(buffer<>'C')then halt;
 assign(probe,'H:/HOOKUNW.DAT');reset(probe);
 if filesize(probe)<>1 then halt;
 read(probe,number);close(probe);if number<>77 then halt;
 number:=22;write(keep,number);close(keep);
 number:=44;write(heap^.f,number);close(heap^.f);dispose(heap);
 if stackptr<>saved then halt;
 auxoutptr:=olda;lstoutptr:=oldl;
end;
begin recurptr:=stackptr-2048; outer;writeln('HOOK-AUX-OK')end.
