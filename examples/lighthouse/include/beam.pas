{ The winning animation changes the lantern, beam and distant ship. }

{ Light the lantern first, then spread light and reveal the ship. }
procedure Beam;
var
  step: integer;
  shade: char;
begin
  Pen(Yellow, Brown);
  GotoXY(25, 16);
  Say(#177#219#219#177);
  Delay(150);
  Pen(Yellow, Black);
  for step := 0 to 20 do
  begin
    case step div 6 of
      0: shade := #219;
      1: shade := #178;
      2: shade := #177;
      3: shade := #176
    end;
    GotoXY(23 - step, 16);
    Say(shade);
    GotoXY(30 + step, 16);
    Say(shade);
    if step >= 10 then
    begin
      GotoXY(23 - step, 15);
      Say(#176);
      GotoXY(30 + step, 15);
      Say(#176);
      GotoXY(23 - step, 17);
      Say(#176);
      GotoXY(30 + step, 17);
      Say(#176)
    end;
    Delay(30)
  end;
  Pen(White, Black);
  GotoXY(42, 17);
  Say(#220#219#220);
  Pen(LightGray, Blue);
  GotoXY(40, 18);
  Say(#223#219#219#219#219#219#223);
  Delay(1000)
end;

