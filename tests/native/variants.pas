program Variants;
type tagged=record c:char;case tag:boolean of false:(n:integer);true:(s:string[3]) end;
     free=record case byte of 0,1:(n:integer);2:(s:string[3]) end;
     nested=record case boolean of
       false:(a:char;case byte of 0:(n:integer);1:(s:string[3]));true:(b:char) end;
     repeated=record case boolean of false:(x:integer);false:(y:char) end;
const first:tagged=(c:'a';tag:false;n:123);
      second:tagged=(c:'b';tag:true;s:'xy');
      mismatch:tagged=(c:'c';tag:false;s:'zz');
      one:free=(n:456);two:free=(s:'abc');
      deep:nested=(a:'q';s:'def');
var a,b:tagged;f:free;n:nested;r:repeated;
begin
 if (sizeof(tagged)=6) and (sizeof(free)=4) and (sizeof(nested)=5) then writeln('VARIANT-MAX-LAYOUT-OK');
 a:=first;b:=second;
 if (a.n=123) and (b.s='xy') and (addr(a.n)-addr(a)=2) and
    (addr(a.n)=addr(a.s)) then writeln('VARIANT-TAGGED-COPY-OK');
 if (mismatch.tag=false) and (mismatch.s='zz') then writeln('VARIANT-INITIALIZER-FIELD-CHOICE-OK');
 f:=one;if f.n=456 then begin f:=two;if f.s='abc' then writeln('VARIANT-FREE-INITIALIZERS-OK') end;
 n:=deep;if (n.a='q') and (n.s='def') and (addr(n.n)-addr(n)=1) then writeln('VARIANT-NESTED-INITIALIZER-OK');
 a.tag:=true;a.n:=789;
 if (a.n=789) and (a.tag=true) then writeln('VARIANT-ACCESS-UNSELECTED-OK');
 r.x:=65;if ord(r.y)=65 then writeln('VARIANT-REPEATED-LABEL-OK');
 with b do begin s[2]:='Z';if (s='xZ') and tag then writeln('VARIANT-WITH-FIELD-OK') end;
end.
