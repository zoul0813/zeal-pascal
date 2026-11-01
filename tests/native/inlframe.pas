program InlineFrames;
var actual,calls,saved,nativeInner,selfEntry:integer;
{$A-}
function resultValue:integer;
begin
 inline($21/resultValue/$22/selfEntry);
 resultValue:=9
end;
procedure outer(depth:integer);
var local,before:integer;
procedure inner(value:integer;var ref:integer);
var temp:integer;aliasRef:integer absolute ref;
begin
 inline($21/temp/$36/42/$23/$36/0);
 inline($21/value/$36/17/$23/$36/0);
 inline($2A/aliasRef/$36/44/$23/$36/0);
 if(temp<>42)or(value<>17)or(ref<>44)or(aliasRef<>44)then halt;
 inline($21/local/$36/61/$23/$36/0);
 if local<>61 then halt;
 inline($21/inner/$22/nativeInner);
 calls:=calls+1
end;
begin
 local:=100+depth;before:=StackPtr;
 inner(3,actual);
 if(actual<>44)or(local<>61)then halt;
 {Native internal entry: lexical parent, then explicit arguments.}
 inline($DD/$E5/$21/>5/$E5/$21/actual/$E5/$CD/inner);
 if(StackPtr<>before)or(actual<>44)or(local<>61)then halt;
 if depth>0 then outer(depth-1);
 if local<>61 then halt
end;
begin
 calls:=0;actual:=0;saved:=StackPtr;outer(2);
 if(calls<>6)or(actual<>44)or(nativeInner=0)then halt;
 if StackPtr<>saved then halt;
 if(resultValue<>9)or(selfEntry<>Addr(resultValue))then halt;
 writeln('INLINE-FRAMES-OK')
end.
