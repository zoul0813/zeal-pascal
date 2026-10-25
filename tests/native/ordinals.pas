program Ordinals;
const Limit=255; Minus=-32768; Quoted='can''t'; Empty=''; Letter='A'; Alias=Quoted;
type Octet=Byte; Tiny=-2..2; Color=(Red,Green,Blue); Warm=Green..Blue;
     Name=string[8]; LetterRange='a'..'z'; Truth=True..True;
const Counter:Integer=3; Initial:Name='ready'; Mark:Char=#90;
var b:Octet; n:Integer; c:Color; w:Warm; ch:Char; text:Name;
    small:Tiny; flag:Truth; low:LetterRange;
begin
  b:=-1; n:=b+1;
  if (b=Limit) and (n=256) and (SizeOf(b)=1) then WriteLn('BYTE-OK');
  if (Minus=$8000) and (MaxInt=32767) then WriteLn('CONST-LIMITS-OK');
  text:=Alias;
  if (text=Quoted) and (Empty='') then WriteLn('CONST-STRINGS-OK');
  ch:=Letter; text:=ch;
  if (text='A') and (ch=text) and (ch<'AA') and ('AA'>ch) then WriteLn('CHAR-STRING-OK');
  ch:=#128;
  if (ch>#127) and (Ord(ch)=128) and ('AB'='A'#66) and (^G=#7) then WriteLn('CHAR-LITERALS-OK');
  c:=Color(1); w:=Blue;
  if (c=Green) and (Integer(c)=1) and (Ord(w)=2) then WriteLn('ENUM-CAST-OK');
  n:=0;
  for c:=Red to Blue do n:=n+Ord(c);
  if (n=3) and (c=Blue) then WriteLn('ENUM-FOR-OK');
  n:=0;
  for ch:='a' to 'c' do n:=n+1;
  for b:=254 to 255 do n:=n+1;
  if (n=5) and (ch='c') and (b=255) then WriteLn('BYTE-CHAR-FOR-OK');
  if (Ord(Succ(Red))=1) and (Pred(Blue)=Green) and (Succ(Byte(255))=0)
    and (Chr(65)='A') and (Ord(False)=0) then WriteLn('ORDINAL-FUNCTIONS-OK');
  if (Abs(-7)=7) and (Abs(-32768)=-32768) and (Sqr(13)=169) and Odd(-3)
    and (Hi($1234)=18) and (Lo($1234)=52) and (Swap($1234)=$3412)
    and (UpCase('q')='Q') and (UpCase('Z')='Z') and (UpCase(#255)=#255) then WriteLn('INTEGER-FUNCTIONS-OK');
  if (SizeOf(Octet)=1) and (SizeOf(Tiny)=2) and (SizeOf(Color)=1)
    and (SizeOf(Name)=9) and (Addr(n)=Addr(b)+1) then WriteLn('TYPE-LAYOUT-OK');
  Counter:=Counter+1;
  if (Counter=4) and (Initial='ready') and (Mark='Z') and (Addr(Counter)>Addr(low)) then WriteLn('TYPED-CONST-OK');
  {$R+}
  small:=-2; flag:=True; low:='z'; b:=255; w:=Green;
  if (small=-2) and flag and (low='z') and (w=Green) then WriteLn('CHECKED-BOUNDS-OK');
end.
