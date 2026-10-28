program TerminalRead;
var f,g:text;c,d:char;n,code,saved,checks:integer;r:real;s:string[3];p,q:boolean;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('TRM-READ-FAIL-',checks);halt end end;
begin
 saved:=stackptr;read(Trm,c);check(c='A');check(not keypressed);
 read(Trm,n);read(Trm,c);check((n=12)and(c=' '));
 read(Trm,r);read(Trm,c);check((r=1.25)and(c=' '));
 read(Trm,s);read(Trm,c);check((s='abc')and(c='d'));
 read(Trm,c);check(c=#13);readln(Trm);check(not keypressed);
 read(Trm,c);check(c=#10);read(Trm,c);check(c=#27);
 read(Trm,c);check(c=#127);CBreak:=false;
 read(Trm,c);check(c='X');CBreak:=true;
 p:=eof(Trm);read(Trm,c);check(p and(c=#26));
 read(Trm,c);check(c='X');
 assign(f,'tRm:');assign(g,'TRM:');rewrite(g);
 p:=eoln(f);q:=eoln(g);read(f,c);read(g,d);check((not p)and q and(c='K')and(d=#13));
 p:=eoln(f);close(f);read(f,c);check(c='M');
 p:=eoln(f);reset(f);read(f,c);check(c='O');
 {$I-}close(Trm);code:=ioresult;{$I+}check(code=$22);
 {$I-}assign(Trm,'TRM:');code:=ioresult;{$I+}check(code=$22);
 close(f);close(g);check(stackptr=saved);writeln('TRM-READ-OK');
end.
