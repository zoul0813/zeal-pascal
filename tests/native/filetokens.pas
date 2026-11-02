program FileBlocks;
type ints=file of integer;box=record left:byte;f:ints;right:byte end;pointer=^box;
var f:ints;a:array[1..20]of box;p:pointer;i,saved:integer;
const checks:integer=0;
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('FILE-BLOCK-FAIL-',checks);halt end end;
function word(address:integer):integer;
begin word:=mem[address]+256*mem[address+1]end;
begin
 check((sizeof(text)=176)and(sizeof(ints)=176)and(sizeof(box)=178));
 for i:=1 to 20 do begin
  a[i].left:=12;a[i].right:=34;
  assign(a[i].f,'H:/FIB.DAT');
  check((a[i].left=12)and(a[i].right=34));
 end;
 check(addr(a[2])-addr(a[1])=178);
 saved:=memavail;new(p);fillchar(p^,sizeof(box),170);
 assign(p^.f,'H:/FIB.DAT');check((p^.left=170)and(p^.right=170));
 dispose(p);check(memavail=saved);
 assign(f,'H:/FIB.DAT');rewrite(f);i:=7;write(f,i);
 check((word(addr(f)+4)=1)and(word(addr(f)+6)=2)and(word(addr(f)+8)=1));
 close(f);reset(f);read(f,i);check((i=7)and(eof(f)));close(f);
 writeln('FILE-TOKENS-OK')
end.
