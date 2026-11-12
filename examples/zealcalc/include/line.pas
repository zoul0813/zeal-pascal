{$R-}
{ Private indices follow the viewport, frame and string bounds checked at entry. }
overlay procedure Line(y: integer; text: LineText);
var slot, first, offset: integer; oldlength: byte; oldchar: char; same: boolean;
{ The renderer already owns the ZVB ports and validated 80x40 coordinates. }
procedure Position(x, y: integer);
begin
  Port[$8E] := 0; Port[$A1] := y - 1; Port[$A2] := x - 1
end;

procedure Pen(inverse: boolean);
begin
  Port[$8E] := 0;
  if inverse then Port[$A5] := $F0 else Port[$A5] := $0F
end;

{ The documented ZVB text-print port advances the hardware cursor. Rendering
  printable fields directly avoids importing the general file-output engine. }
procedure Say(text: LineText);
var i: integer;
begin
  for i := 1 to Length(text) do Port[$A0] := Ord(text[i])
end;

{ Our renderer fixes scrolling at zero. Clear the tail without moving the
  cursor or triggering the last-column pending wrap. No Pascal while mapped. }
procedure ClearTail;
var cell, count: word; color: byte;
begin
  cell := (y - 1) * 80 + first - 1; count := 81 - first;
  color := $0F;
  Inline(
    $2A/cell/$ED/$4B/count/
    $ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/$3E/$40/$D3/$F0/
    $E5/$C5/$54/$5D/$13/$36/$00/$0B/$78/$B1/$CA/*+4/$ED/$B0/
    $C1/$E1/$11/$00/$10/$19/$54/$5D/$13/$3A/color/$77/
    $0B/$78/$B1/$CA/*+4/$ED/$B0/
    $F1/$D3/$F0/$F1/$E2/*+3/$FB
  )
end;

begin
  if y <= 3 then slot := y else slot := y - 33;
  offset := 2100 + (slot - 1) * 80;
  CacheCopy(offset, oldlength, 1, True);
  first := 1; same := True;
  while (first <= Length(text)) and (first <= oldlength) and same do
  begin
    CacheCopy(offset + first, oldchar, 1, True);
    if text[first] = oldchar then first := first + 1
    else same := False
  end;
  if same and (first > Length(text)) and (Length(text) = oldlength) then Exit;
  Pen(False); Position(first, y); ClearTail;
  Say(Copy(text, first, 79)); CacheCopy(offset, text, 80, False)
end;

