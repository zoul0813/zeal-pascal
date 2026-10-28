program InlineAddresses;
var g:array[1..301]of byte;code,saved,count:integer;
const typed:integer=17;
procedure future;forward;
procedure ext;external $2345;
procedure sample;
begin
 inline($CD/future);
 inline($21/*+8/$22/code/$C3/*+28/
 g/g-g/typed/typed-typed/future/future-future/ext/0-*/*+*/0-*-*/*+*-*/<*/<0-*/>1)
end;
procedure future;
begin count:=count+1 end;
function wordAt(offset:integer):integer;
begin wordAt:=Mem[code+offset]+256*Mem[code+offset+1]end;
begin
 count:=0;saved:=StackPtr;sample;
 if count<>1 then halt;
 if StackPtr<>saved then halt;
 if(wordAt(0)<>Addr(g))or(wordAt(2)<>0)then halt;
 if(wordAt(4)<>Addr(typed))or(wordAt(6)<>0)then halt;
 if(wordAt(8)<>Addr(future))or(wordAt(10)<>0)or(wordAt(12)<>$2345)then halt;
 if(wordAt(14)<>-code-14)or(wordAt(16)<>2*(code+16))then halt;
 if(wordAt(18)<>-2*(code+18))or(wordAt(20)<>code+20)then halt;
 if(Mem[code+22]<>Lo(code+22))or(Mem[code+23]<>Lo(-code-23))or(wordAt(24)<>1)then halt;
 writeln('INLINE-ADDRESSES-OK')
end.
