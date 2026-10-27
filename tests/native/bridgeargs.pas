program BridgeArguments;
type code=string[3];text=string[255];small=set of 3..5;
 vector=array[1..3]of integer;item=record number:integer;name:string[7]end;
 pointer=^integer;
const machine:code=#195#0#0;checks:integer=0;target:integer=0;
 big:text='';result:text='';actual:real=0;index:integer=0;saved:integer=0;
 values:vector=(11,22,33);row:item=(number:17;name:'old');
 amount:integer=1;opaque:integer=0;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('BRIDGE-ARG-FAIL-',checks);halt end end;
procedure install(address:integer);
begin target:=address;machine[2]:=chr(lo(address));machine[3]:=chr(hi(address))end;
function stringcall(a:integer;s:text;b:char;t:text;last:integer):text;external $8004;
function realcall(a:real;n:integer;b:real):real;external $8004;
procedure valuecall(s:small;v:vector;r:item;var n:integer;var raw);external $8004;
function boolcall(n:byte):boolean;external $8004;
function charcall(n:char):char;external $8004;
function pointercall:pointer;external $8004;
procedure outer;
var local:integer;prefix:string[3];bias:real;own:pointer;
function strings(a:integer;s:text;b:char;t:text;last:integer):text;
begin
 if length(s)>200 then begin s[1]:=b;strings:=s+chr(lo(last))end
 else strings:=prefix+chr(lo(a))+copy(s,1,2)+b+t+chr(lo(last));
 if length(t)>0 then t[1]:='?';
end;
function reals(a:real;n:integer;b:real):real;
begin reals:=a+b+bias+n end;
procedure copies(s:small;v:vector;r:item;var n:integer;var raw);
begin
 check(s=[3]);check((v[1]=11)and(v[3]=33));check((r.number=17)and(r.name='old'));
 v[1]:=99;r.number:=77;r.name:='new';s:=[];
 n:=n+local;move(n,raw,2);
end;
function booleanvalue(n:byte):boolean;
begin booleanvalue:=(n=171)and(local=40)end;
function charvalue(n:char):char;
begin charvalue:=upcase(n)end;
function pointervalue:pointer;
begin pointervalue:=own end;
begin
 local:=40;prefix:='P';bias:=1.25;own:=ptr(addr(local));
 install(addr(strings));result:=stringcall(65,'abc','Q','YZ',90);
 check(result='PAabQYZZ');
 result:=stringcall(65,'abc','Q','YZ',90)+stringcall(66,'def','R','UV',91);
 check(result='PAabQYZZPBdeRUV[');
 install(addr(reals));actual:=realcall(1.5,7,2.25);check(actual=12);
 install(addr(copies));with row do begin
  valuecall([3],values,row,amount,opaque);check((number=17)and(name='old'))
 end;
 check((amount=41)and(opaque=41)and(values[1]=11)and(row.number=17));
 install(addr(booleanvalue));check(boolcall(171));check(not boolcall(170));
 install(addr(charvalue));check(charcall('q')='Q');
 install(addr(pointervalue));own:=pointercall;check(own^=40);
 for index:=1 to 254 do big:=big+'x';install(addr(strings));
 result:=stringcall(0,big,'Q','',90);
 check((length(result)=255)and(result[1]='Q')and(result[255]='Z')and(big[1]='x'));
end;
begin
 saved:=stackptr;outer;check(stackptr=saved);
 writeln('BRIDGE-ARGUMENTS-OK');
end.
