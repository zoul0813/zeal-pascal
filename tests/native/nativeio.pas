program NativeMemoryIO;
var n:integer;fixed:integer absolute $8000;page:byte;
procedure shadow;
var port:array[1..2]of byte;mem:byte;
begin port[1]:=31;mem:=47;
 if(port[1]=31)and(mem=47)then writeln('MEMORY-PREDEFINED-SHADOW-OK')end;
begin
 fixed:=1234;
 if(n=1234)and(addr(fixed)=addr(n))then writeln('NATIVE-ABSOLUTE-GLOBAL-OK');
 page:=port[$80F2];
 if(page<32)or(page>63)then halt;
 {$R+}port[$1F2]:=page+256;
 if(page>=32)and(page<=63)and(port[$80F2]=page)then writeln('NATIVE-PORT-ADDRESS-DATA-IDENTITY-OK');
 shadow;
end.
