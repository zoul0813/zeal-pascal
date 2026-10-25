program StringOperationBoundaries;
const a='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-';
      b='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+';
var s,t:string[255]; small:string[1]; i:integer;
begin
 s:=a+a+a+b; t:=copy(s,1,255);
 if (length(t)=255) and (t=s) and (copy(t,255,1)='+') then writeln('COPY-255-OK');
 if (pos(t,s)=1) and (pos('+-a',s)=63) then writeln('POS-255-OK');
 s:=concat(#0,#255,'a',#0,#255);
 if (length(s)=5) and (ord(s[1])=0) and (ord(s[2])=255) and
    (pos(#0#255,s)=1) and (copy(s,4,2)=#0#255) then writeln('STRING-BINARY-BYTES-OK');
 s:=''; insert(#255,s,255); delete(s,1,255);
 small:='a'; insert('XYZ',small,1);
 if (s='') and (small='X') then writeln('STRING-ZERO-ONE-OK');
 s:=a+a+a+b; delete(s,255,32767); insert('+',s,255);
 if (s=t) and (length(s)=255) then writeln('STRING-255-MUTATION-OK');
end.
