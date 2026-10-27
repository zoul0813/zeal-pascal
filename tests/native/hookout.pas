program HookOutput;
type bytes=array[0..2]of byte;
var machine:bytes;
const checks:integer=0;
{$A-}
procedure check(ok:boolean);
begin checks:=checks+1;if not ok then begin writeln('HOOK-OUT-FAIL-',checks);halt end end;
procedure native(c:char);external $8000;
procedure install(address:integer);
begin machine[0]:=195;machine[1]:=lo(address);machine[2]:=hi(address)end;
procedure outer;
var text:string[255];auxtext:string[20];old,oldaux,saved:integer;code:integer;
procedure auxiliary(c:char);begin auxtext:=auxtext+c end;
procedure capture(c:char);
begin if c='#'then write(Aux,'nested')else text:=text+c end;
begin
 old:=conoutptr;oldaux:=auxoutptr;text:='';auxtext:='';saved:=stackptr;
 conoutptr:=addr(capture);auxoutptr:=addr(auxiliary);
 write('A',12:4,true:6,1.5:6:2,#0#255);write(Con,'C');write(Trm,'T');writeln('#');
 check(text='A  12  TRUE  1.50'#0#255'CT'#13#10);check(auxtext='nested');
 text:='';write('x':255);check((length(text)=255)and(text[1]=' ')and(text[255]='x'));
 text:='';install(old);native('P');check(text='');
 {$I-}close(Output);write('suppressed');code:=ioresult;{$I+}
 check(code=34);check(text='');
 conoutptr:=old;auxoutptr:=oldaux;check(stackptr=saved);
end;
begin outer;writeln('HOOK-OUTPUT-OK')end.
