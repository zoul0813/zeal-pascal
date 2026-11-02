program FileDeclarations;
type ints=file of integer;same=file of integer;raw=file;
     empty=record end;zero=file of empty;fp=^ints;
     bundle=record left:byte;t:text;f:ints;u:raw;right:byte end;
     files=array[1..2]of bundle;
     variant=record case boolean of false:(t:text);true:(f:ints)end;
     component=record f:text end;aggregate=file of component;
var f:ints;g:same;t:text;u:raw;z:zero;a,b:bundle;all:files;v:variant;
    p:fp;checks,saved:integer;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('FILE-TYPE-FAIL-',checks);halt end end;
procedure touch(var item:ints);
begin check(sizeof(item)=176);mem[addr(item)+15]:=173 end;
procedure forwarded(var item:same);
begin touch(item)end;
procedure nested(depth,outer:integer);
var item:ints;local:bundle;frame:integer;
begin
 frame:=stackptr;fillchar(item,16,depth);local.left:=12;local.right:=34;
 check((outer=0)or(addr(item)=outer));
 if depth>0 then nested(depth-1,addr(item));
 check((mem[addr(item)]=depth)and(mem[addr(item)+14]=depth));
 touch(local.f);
 check((local.left=12)and(local.right=34)and(mem[addr(local.f)+15]=173));
 check(stackptr=frame)
end;
procedure copy_value(item:bundle);
begin item.left:=99;check(sizeof(item.f)=176)end;
{$A+}
procedure static_file(first:boolean);
var item:text;
begin
 if first then mem[addr(item)+15]:=77 else check(mem[addr(item)+15]=77)
end;
begin
 checks:=0;
 check((sizeof(text)=176)and(sizeof(ints)=176)and(sizeof(raw)=176)and(sizeof(zero)=176));
 check((sizeof(f)=176)and(sizeof(g)=176)and(sizeof(t)=176)and(sizeof(u)=176)and(sizeof(z)=176));
 check((sizeof(bundle)=530)and(sizeof(files)=1060)and(sizeof(variant)=176)and(sizeof(aggregate)=176));
 fillchar(g,16,0);forwarded(g);check(mem[addr(g)+15]=173);
 a.left:=12;a.right:=34;fillchar(a.f,16,83);b:=a;
 check((b.left=12)and(b.right=34)and(mem[addr(b.f)+14]=83));
 copy_value(a);check(a.left=12);
 all[1]:=a;all[2]:=b;check(addr(all[2])-addr(all[1])=530);
 with all[2] do begin touch(f);check(mem[addr(f)+15]=173)end;
 saved:=stackptr;nested(5,0);check(stackptr=saved);
 static_file(true);static_file(false);
 saved:=memavail;new(p);check(sizeof(p^)=176);touch(p^);
 check(mem[ord(p)+15]=173);dispose(p);check(memavail=saved);
 writeln('FILE-DECLARATIONS-OK')
end.
