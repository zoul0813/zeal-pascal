{$C-}
var chars,actual,colors:array[0..3199] of byte;
    i,j,k,x,y,sx,sy,n,failures,pass:integer;
    saved:array[1..8] of byte;
    savedmap,bx,by,duration,iff,expected:byte;
    sub:1..80;evals:integer;
procedure check(b:boolean);
begin if not b then begin failures:=failures+1;writeln('FAIL:',n)end;n:=n+1 end;
function arg(wanted:integer):integer;
begin evals:=evals+1;check(evals=wanted);arg:=wanted end;
procedure paint;
begin
 for i:=0 to 3199 do begin chars[i]:=33+(i div 80)mod 90;colors[i]:=(i div 80)mod 16 end;
 inline($3E/$40/$DB/$F1/$F5/$3E/$40/$D3/$F1/
        $21/chars/$11/$4000/$01/3200/$ED/$B0/
        $21/colors/$11/$5000/$01/3200/$ED/$B0/
        $F1/$D3/$F1)
end;
procedure sample;
begin
 inline($3E/$40/$DB/$F1/$F5/$3E/$40/$D3/$F1/
        $21/$4000/$11/actual/$01/3200/$ED/$B0/
        $21/$5000/$11/colors/$01/3200/$ED/$B0/
        $F1/$D3/$F1)
end;
function at(col,row:integer):integer;
begin at:=((row+sy)mod 40)*80+(col+sx)mod 80 end;
procedure unchanged;
begin
 sample;
 for j:=0 to 3199 do begin
  expected:=7;
  if(chars[j]>=33)and(chars[j]<73)then expected:=(chars[j]-33)mod 16;
  check((actual[j]=chars[j])and(colors[j]=expected))
 end
end;
procedure blankrow(row:integer);
begin for x:=0 to 79 do begin k:=at(x,row);chars[k]:=0 end end;
procedure insertat(row:integer);
begin
 paint;gotoxy(13,row+1);lowvideo;insline;
 for y:=39 downto row+1 do
  for x:=0 to 79 do begin k:=at(x,y);j:=at(x,y-1);chars[k]:=chars[j]end;
 blankrow(row);unchanged;check((port[$A2]=12)and(port[$A1]=row))
end;
procedure deleteat(row:integer);
begin
 paint;gotoxy(13,row+1);lowvideo;delline;
 for y:=row to 38 do
  for x:=0 to 79 do begin k:=at(x,y);j:=at(x,y+1);chars[k]:=chars[j]end;
 blankrow(39);unchanged;check((port[$A2]=12)and(port[$A1]=row))
end;
begin
 failures:=0;n:=0;port[$8E]:=0;
 crtexit; { no earlier CrtInit is a no-op }
 bx:=80;by:=40;duration:=0;sub:=1;
 gotoxy(bx,by);gotoxy(sub,sub);
 evals:=0;gotoxy(arg(1),arg(2));check(evals=2);
 port[$8E]:=1;clrscr;check(port[$8E]=1);port[$8E]:=0;
 port[$8E]:=1;gotoxy(10,5);check(port[$8E]=1);port[$8E]:=0;
 port[$8E]:=1;lowvideo;check(port[$8E]=1);port[$8E]:=0;
 port[$A0]:=ord('X');sample;check((actual[329]=ord('X'))and(colors[329]=7));
 check((port[$A2]=10)and(port[$A1]=4));
 inline($F3);clrscr;gotoxy(1,1);insline;delline;clreol;
 inline($ED/$57/$F5/$E1/$7D/$32/iff/$FB);
 check((iff and 4)=0);
 { The pinned board rejects this row. Helpers also guard a bad readback. }
 clrscr;port[$A1]:=50;insline;delline;clreol;sample;
 for i:=0 to 3199 do check((actual[i]=0)and(colors[i]=7));
 gotoxy(1,1);
 for pass:=0 to 1 do begin
  if pass=1 then begin port[$A4]:=73;port[$A3]:=37 end
  else begin port[$A4]:=0;port[$A3]:=0 end;
  sy:=port[$A3];sx:=port[$A4];
  insertat(0);insertat(17);insertat(39);
  deleteat(0);deleteat(17);deleteat(39);
  paint;gotoxy(77,40);lowvideo;clreol;
  for x:=76 to 79 do begin k:=at(x,39);chars[k]:=0 end;
  unchanged;check((port[$A2]=76)and(port[$A1]=39));
 end;
 { reset scroll first so this row has an unambiguous physical oracle }
 port[$A4]:=0;port[$A3]:=0;paint;gotoxy(1,1);lowvideo;clreol;
 for x:=0 to 79 do begin chars[x]:=0 end;unchanged;
 gotoxy(80,40);check((port[$A2]=79)and(port[$A1]=39));
 normvideo;check((port[$A5]=15)and(port[$A8]=$F0));
 highvideo;check((port[$A5]=15)and(port[$A8]=$F0));
 lowvideo;check((port[$A5]=7)and(port[$A8]=$70));
 for i:=1 to 8 do saved[i]:=port[$A0+i];
 savedmap:=port[$8E];crtinit;
 check((port[$A5]=15)and(port[$A8]=$F0));
 gotoxy(2,3);lowvideo;crtexit;
 for i:=1 to 8 do check(port[$A0+i]=saved[i]);
 check(port[$8E]=savedmap);
 crtexit;for i:=1 to 8 do check(port[$A0+i]=saved[i]);
 clrscr;sample;
 for i:=0 to 3199 do check((actual[i]=0)and(colors[i]=7));
 check((port[$A1]=0)and(port[$A2]=0)and(port[$A3]=0)and(port[$A4]=0));
 { Init/Exit must retain the board's pending wrap at the right edge. }
 gotoxy(80,1);port[$A0]:=ord('Z');check(port[$A2]=80);
 sample;for i:=0 to 3199 do begin chars[i]:=actual[i]end;
 clreol;unchanged;check(port[$A2]=80);
 crtinit;gotoxy(1,3);port[$A9]:=$98;gotoxy(4,4);crtexit;check((port[$A2]=80)and(port[$A1]=0));
 port[$A0]:=ord('Y');sample;check(actual[80]=ord('Y'));
 crtinit;gotoxy(4,5);lowvideo;crtinit;gotoxy(8,9);normvideo;crtexit;
 check((port[$A2]=3)and(port[$A1]=4)and(port[$A5]=7));
 delay(duration);delay(25);
 if failures=0 then writeln('CRT-OK:',n)else writeln('CRT-FAIL:',failures,':',n)
end.
