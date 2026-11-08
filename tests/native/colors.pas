{$C-}
const
 first=Black;last=White;
 named:array[0..15]of integer=(Black,Blue,Green,Cyan,Red,Magenta,Brown,LightGray,
  DarkGray,LightBlue,LightGreen,LightCyan,LightRed,LightMagenta,Yellow,White);
type color=first..last;
 directcolor=Black..White;
 negativecolor=-Red..White;
 colorset=set of Black..White;
var chars,attrs:array[0..319]of byte;
 bg,fg,i,j,x,y,n,failures,code,constant_value,calls:integer;
 chosen:color;small:byte;sub:directcolor;negative:negativecolor;
 direct:array[Red..White]of integer;selected:colorset;
 rowaddr,pen,cursorpen,fcode,bcode:integer;
procedure check(ok:boolean);
begin
 if not ok then begin failures:=failures+1;writeln('COLOR-FAIL:',n)end;
 n:=n+1
end;
{ Page 0 is borrowed only inside bounded Inline copies, with IFF preserved.
  Program code and dynamically placed globals remain mapped above $4000. }
procedure sample;
begin
 inline($ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/$3E/$40/$D3/$F0/
  $21/>$0000/$11/chars/$01/320/$ED/$B0/
  $21/$1000/$11/attrs/$01/320/$ED/$B0/$F1/$D3/$F0/$D1/$CB/$53/$28/$01/$FB)
end;
procedure bottom;
begin
 rowaddr:=((port[$A3]+39)mod 40)*80;
 inline($ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/$3E/$40/$D3/$F0/
  $2A/rowaddr/$11/chars/$01/>80/$ED/$B0/
  $2A/rowaddr/$01/$1000/$09/$11/attrs/$01/>80/$ED/$B0/$F1/$D3/$F0/$D1/$CB/$53/$28/$01/$FB)
end;
function pick:integer;
begin calls:=calls+1;TextBackground(LightBlue);pick:=Yellow end;
{$A-}
procedure recursive(depth:integer);
var a,b:byte;
begin
 a:=WhereX;b:=WhereY;
 if depth>0 then recursive(depth-1);
 check((a=7)and(b=9))
end;
{$A+}
begin
 failures:=0;n:=0;
 for i:=0 to 15 do check(named[i]=i);
 chosen:=Yellow;check(chosen=14);
 sub:=White;negative:=-Red;direct[Red]:=Green;selected:=[Black,Red..White];
 check(sub=15);check((negative=-4)and(sizeof(negative)=2));
 check(direct[Red]=2);check((Black in selected)and(White in selected)and not(Blue in selected));
 case chosen of Yellow:check(true)else check(false)end;
 inline($21/>White/$22/constant_value);check(constant_value=15);
 normvideo;clrscr;
 for bg:=0 to 15 do
  for fg:=0 to 15 do begin
   TextBackground(bg);TextColor(fg);i:=bg*16+fg;
   check(port[$A5]=i);check(port[$A8]=fg*16+bg);
   x:=i mod 64+1;y:=i div 64+1;GotoXY(x,y);
   check((WhereX=x)and(WhereY=y));write(Con,'X')
  end;
 sample;
 for i:=0 to 255 do begin
  j:=(i div 64)*80+i mod 64;
  check(chars[j]=ord('X'));check(attrs[j]=i)
 end;
 { Changes through raw Port must be preserved by the other color component. }
 port[$A5]:=$B2;TextColor(LightRed);check(port[$A5]=$BC);
 port[$A5]:=$43;TextBackground(White);check(port[$A5]=$F3);
 calls:=0;TextBackground(Black);TextColor(pick);
 check((calls=1)and(port[$A5]=$9E));
 TextColor(Yellow);GotoXY(7,9);
 port[$8E]:=1;x:=WhereX;check(port[$8E]=1);
 y:=WhereY;check(port[$8E]=1);TextBackground(Blue);check(port[$8E]=1);
 TextColor(Yellow);check(port[$8E]=1);port[$8E]:=0;
 check((x=7)and(y=9));check(WhereX*100+WhereY=709);
 small:=WhereX;check(small=7);recursive(5);
 writeln('COLOR-POS:',WhereX:3,':',WhereY:3);
 inline($F3);x:=WhereX;y:=WhereY;TextColor(LightCyan);TextBackground(Red);
 inline($ED/$57/$F5/$E1/$7D/$32/small/$FB);check((small and 4)=0);
 check((x=7)and(y=9));
 port[$A3]:=37;port[$A4]:=73;check((WhereX=7)and(WhereY=9));
 port[$A3]:=0;port[$A4]:=0;
 { Capture/restore mixed colors and the software cursor snapshot. }
 TextBackground(LightBlue);TextColor(Yellow);CrtInit;
 TextBackground(White);TextColor(Black);GotoXY(1,1);CrtExit;
 check((port[$A5]=$9E)and(WhereX=7)and(WhereY=9));
 GotoXY(80,40);write(Con,'Z');
 check(WhereX=81);check(WhereY=40);ClrEol;check(WhereX=81);
 write(Con,'N');check((WhereX=2)and(WhereY=40));bottom;
 check((chars[0]=ord('N'))and(attrs[0]=$9E));
 for i:=1 to 79 do check((chars[i]=32)and(attrs[i]=$9E));
 { Blank operations must use both nibbles and line shifts retain attributes. }
 TextBackground(White);TextColor(LightRed);ClrScr;sample;
 for i:=0 to 319 do check((chars[i]=0)and(attrs[i]=$FC));
 GotoXY(1,1);write(Con,'A');GotoXY(1,2);write(Con,'B');
 TextBackground(LightBlue);TextColor(Yellow);GotoXY(1,1);InsLine;sample;
 check((chars[0]=0)and(attrs[0]=$9E));
 check((chars[80]=ord('A'))and(attrs[80]=$FC));
 check((chars[160]=ord('B'))and(attrs[160]=$FC));
 DelLine;sample;check((chars[0]=ord('A'))and(attrs[0]=$FC));
 check((chars[80]=ord('B'))and(attrs[80]=$FC));
 ClrEol;sample;for i:=0 to 79 do check((chars[i]=0)and(attrs[i]=$9E));
 {$I-}Reset(Input);x:=WhereX;y:=WhereY;
 TextColor(Black);TextBackground(Black);code:=IOResult;{$I+}
 check((x=0)and(y=0)and(code=$22)and(port[$A5]=$9E));
 { Unsupported text mode: query returns zero with the original I/O error. }
 pen:=port[$A5];cursorpen:=port[$A8];port[$9C]:=1;Delay(25);
 {$I-}x:=WhereX;y:=WhereY;code:=IOResult;
 TextColor(Red);fcode:=IOResult;TextBackground(White);bcode:=IOResult;{$I+}
 port[$9C]:=0;Delay(25);check((x=0)and(y=0)and(code=$20));
 check((fcode=$20)and(bcode=$20)and(port[$A5]=pen)and(port[$A8]=cursorpen));
 if failures=0 then writeln('COLOR-OK:',n)else writeln('COLOR-ERRORS:',failures)
end.
