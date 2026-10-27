program TextEdges;
var f:text;n,i,code:integer;c:char;a:boolean;
begin
 assign(f,'H:/LINES.TXT');rewrite(f);
 writeln(f,'');writeln(f,'11');writeln(f,'22');close(f);
 reset(f);n:=777;read(f,n);
 a:=eoln(f);writeln('FIRST=',n:1,',',a);
 n:=888;read(f,n);a:=eoln(f);writeln('SECOND=',n:1,',',a);
 n:=999;read(f,n);a:=eof(f);writeln('THIRD=',n:1,',',a);close(f);
 assign(f,'H:/LIMIT.TXT');rewrite(f);
 for i:=1 to 30 do write(f,'0');writeln(f,'1 9');close(f);
 reset(f);n:=777;{$I-}read(f,n);code:=ioresult;{$I+}
 read(f,c);writeln('LIMIT=',code:1,',',n:1,',',ord(c):1);
 close(f);assign(f,'H:/EMPTY.TXT');rewrite(f);close(f);reset(f);
 c:='?';{$I-}read(f,c);code:=ioresult;{$I+}
 a:=eof(f);writeln('EMPTYCHAR=',code:1,',',ord(c):1,',',a);close(f);
 writeln('END-NUMERIC')
end.
