program FileTokens;
type ints=file of integer;raw=file;empty=record end;zero=file of empty;
     block=array[1..300]of byte;wide=file of block;wp=^wide;
     box=record left:byte;f:ints;right:byte end;bp=^box;ip=^ints;
     batch=array[0..256]of ints;ap=^batch;
     small=array[1..2]of box;
     fileunion=record case boolean of false:(t:text);true:(i:ints)end;
     mixed=record case boolean of false:(n:integer);true:(f:ints)end;
var t:text;f:ints;u:raw;z:zero;w:wide;b:box;a:small;many:batch;v:fileunion;x:mixed;
    p,q:bp;fp:ip;pa:ap;pw:wp;i,saved,checks:integer;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('FILE-TOKEN-FAIL-',checks);halt end end;
function word(address:integer):integer;
begin word:=mem[address]+256*mem[address+1]end;
function token(address,owner,family,width:integer):boolean;
var j:integer;ok:boolean;
begin
 ok:=(word(address)=$465a)and(word(address+2)=0)and(word(address+4)=0);
 ok:=ok and(word(address+6)=owner)and(mem[address+8]=family);
 ok:=ok and(mem[address+9]=0)and(word(address+10)=width);
 for j:=12 to 15 do ok:=ok and(mem[address+j]=0);
 token:=ok
end;
procedure alias(var item:ints;owner:integer);
begin check(token(addr(item),owner,1,2))end;
procedure nested(depth,outer:integer);
var local:ints;other:text;boxes:small;life,frame:integer;
begin
 life:=word(addr(local)+6);frame:=stackptr;
 check((life<>0)and(life<>outer));
 check(token(addr(local),life,1,2));check(token(addr(other),life,0,0));
 check(token(addr(boxes[1].f),life,1,2));check(token(addr(boxes[2].f),life,1,2));
 alias(local,life);
 if depth>0 then nested(depth-1,life);
 check(token(addr(local),life,1,2));check(stackptr=frame)
end;
procedure value_copy(item:mixed);
begin check(item.n=1234)end;
{$A+}
procedure static_tokens(first:boolean);
var item:text;count:integer;
begin
 check(token(addr(item),0,0,0));
 if first then count:=77 else check(count=77)
end;
begin
 checks:=0;
 check((ioresult=0)and(token(addr(t),0,0,0))and(token(addr(f),0,1,2)));
 check(token(addr(u),0,2,128));check(token(addr(z),0,1,0));
 check(token(addr(w),0,1,300));
 check(token(addr(b.f),0,1,2));
 for i:=1 to 2 do check(token(addr(a[i].f),0,1,2));
 for i:=0 to 256 do check(token(addr(many[i]),0,1,2));
 check(token(addr(v.t),0,0,0));
 alias(f,0);saved:=stackptr;nested(4,0);check(stackptr=saved);
 static_tokens(true);static_tokens(false);
 x.n:=1234;value_copy(x);check(x.n=1234);
 saved:=memavail;new(p);fillchar(p^,sizeof(box),170);i:=ord(p);dispose(p);
 new(q);check(ord(q)=i);check(token(addr(q^.f),ord(q),1,2));
 check((q^.left=170)and(q^.right=170));alias(q^.f,ord(q));dispose(q);
 check(memavail=saved);
 getmem(fp,16);fillchar(fp^,16,119);freemem(fp,16);
 getmem(fp,16);check(mem[ord(fp)]=119);freemem(fp,16);
 new(pw);check(token(ord(pw),ord(pw),1,300));dispose(pw);
 saved:=memavail;new(pa);
 for i:=0 to 256 do check(token(addr(pa^[i]),ord(pa),1,2));
 dispose(pa);check(memavail=saved);check(ioresult=0);
 writeln('FILE-TOKENS-OK')
end.
