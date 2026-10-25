program SetLayout;
type small=set of 3..5;shifted=set of 9..17;all=set of byte;
     rec=record tag:char;s:small;t:shifted end;
     collection=array[boolean] of rec;
const initial:small=[3,5];other:shifted=[9..11,17];
      recordvalue:rec=(tag:'a';s:[3];t:[10,17]);
var a:small;b:shifted;wide:all;x:rec;v:collection;
begin
 a:=initial;b:=other;
 if (sizeof(small)=1) and (sizeof(shifted)=2) and (sizeof(all)=32) and
    (sizeof(rec)=4) and (addr(x.s)-addr(x)=1) then writeln('SET-COMPRESSED-LAYOUT-OK');
 if (a=[3,5]) and (9 in b) and (17 in b) then writeln('SET-TYPED-INITIALIZERS-OK');
 a:=[0,3,5,7,8];
 if (0 in a) and (7 in a) and not (8 in a) then writeln('SET-WHOLE-BYTE-EDGES-OK');
 wide:=a;b:=wide;
 if (0 in wide) and (7 in wide) and (b=[]) then writeln('SET-CROSS-RANGE-COPY-OK');
 x:=recordvalue;v[false]:=x;v[true].s:=[5];
 with v[false] do s:=s+[5];
 if (v[false].s=[3,5]) and (v[true].s=[5]) and (17 in v[false].t) then writeln('SET-SELECTED-STORAGE-OK');
 initial:=[5];if initial=[5] then writeln('SET-MUTABLE-CONSTANT-OK');
end.
