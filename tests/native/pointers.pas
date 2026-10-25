program Pointers;
type
 link=^node;
 node=record value:integer; next:link end;
 pint=^integer; otherint=^integer;
 ppint=^pint;
 text=string[8]; ptext=^text;
 vector=array[1..3]of integer; pvector=^vector;
 flags=set of byte; pflags=^flags;
var first,second:node; p,q:link; n:integer; a:pint; b:otherint;
 aa:ppint; s:text; ps:ptext; v:vector; pv:pvector; f:flags; pf:pflags;
begin
 p:=nil;q:=nil;
 if(p=q)and not(p<>nil)and(sizeof(p)=2)then writeln('POINTER-NIL-WIDTH-OK');
 first.value:=17;second.value:=23;
 p:=ptr(addr(first));q:=ptr(addr(second));p^.next:=q;q^.next:=nil;
 if(p^.value=17)and(p^.next^.value=23)and(p^.next^.next=nil)then
  writeln('POINTER-FORWARD-RECORD-OK');
 with p^ do begin value:=31;next^.value:=47 end;
 if(first.value=31)and(second.value=47)then writeln('POINTER-WITH-OK');
 n:=7;a:=ptr(addr(n));b:=a;b^:=123;
 if(a=b)and(ord(a)=addr(n))and(n=123)then writeln('POINTER-TARGET-COMPATIBILITY-OK');
 aa:=ptr(addr(a));aa^^:=-321;
 if n=-321 then writeln('POINTER-REPEATED-DEREFERENCE-OK');
 s:='hello';ps:=ptr(addr(s));ps^[2]:='A';
 if(ps^='hAllo')and(addr(ps^[2])=addr(s[2]))then writeln('POINTER-STRING-SELECTION-OK');
 pv:=ptr(addr(v));pv^[2]:=432;
 if(pv^[2]=432)and(sizeof(pv^[2])=2)then writeln('POINTER-ARRAY-SELECTION-OK');
 pf:=ptr(addr(f));pf^:=[3,255];
 if(3 in pf^)and(255 in f)then writeln('POINTER-SET-SELECTION-OK');
 if(ord(^M)=13)and(ord(^J)=10)and(length(^M^J)=2)then writeln('POINTER-CARET-LITERALS-OK');
end.
