program AbsoluteViews;
type pair=record lo,hi:byte end;
const typed:integer=0;
var n:integer;bytes:array[0..1]of byte absolute n;
 view:pair absolute n;low:byte absolute bytes;chain:byte absolute low;
 next:byte;typedlow:byte absolute typed;s:string[4];len:byte absolute s;
 fixed:byte absolute $1234;
begin
 n:=$1234;
 if(bytes[0]=$34)and(bytes[1]=$12)and(sizeof(bytes)=2)then writeln('ABSOLUTE-LITTLE-ENDIAN-OK');
 view.lo:=$AB;view.hi:=$12;
 if n=$12AB then writeln('ABSOLUTE-RECORD-VIEW-OK');
 chain:=77;if(bytes[0]=77)and(addr(chain)=addr(n))then writeln('ABSOLUTE-ALIAS-CHAIN-OK');
 if(addr(next)=addr(n)+2)or(addr(next)=addr(n)-1)then writeln('ABSOLUTE-NO-EXTRA-STORAGE-OK');
 typedlow:=91;if typed=91 then writeln('ABSOLUTE-TYPED-CONSTANT-OK');
 s:='abcd';len:=2;if(s='ab')and(sizeof(len)=1)then writeln('ABSOLUTE-STRING-LENGTH-OK');
 if addr(fixed)=$1234 then writeln('ABSOLUTE-LOGICAL-ADDRESS-OK');
end.
