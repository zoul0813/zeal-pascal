program Parameters;
{$R+}
type wordstr=string[127];
var i,calls:integer; first,last:wordstr; small:string[3];
procedure require(ok:boolean);
begin
  if not ok then begin writeln('PARAMS-FAIL');halt end
end;
function next:integer;
begin calls:=calls+1;next:=1 end;
function firstcopy:wordstr;
begin firstcopy:=ParamStr(1) end;
procedure valuecopy(s:wordstr);
begin require(s=first) end;
begin
  writeln('PARAMS:',ParamCount:1);
  require(ParamStr(-32768)='');require(ParamStr(-1)='');
  require(ParamStr(0)='');require(ParamStr(128)='');
  require(ParamStr(256)='');require(ParamStr(32767)='');
  require(ParamStr(ParamCount+1)='');
  first:=ParamStr(1);last:=ParamStr(ParamCount);
  require(firstcopy=first);valuecopy(ParamStr(1));
  calls:=0;require(ParamStr(next)=first);require(calls=1);
  require((ParamStr(1)=first) and (ParamStr(ParamCount)=last));
  require(Pos(ParamStr(1),ParamStr(1))=Ord(Length(first)>0));
  small:=ParamStr(1);require(small=Copy(first,1,3));
  for i:=1 to ParamCount do
    writeln(i:1,':',Length(ParamStr(i)):1,':',ParamStr(i));
  require(IOResult=0);
  writeln('PARAMS-OK')
end.
