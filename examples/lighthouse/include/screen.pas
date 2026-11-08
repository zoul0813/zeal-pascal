{ Console drawing and cached updates. The scene is painted only once. }

{ This full-screen application runs with Output on the native console. }
procedure Say(text: screenline);
begin
  Write(text)
end;

procedure Pen(foreground, background: integer);
begin
  TextBackground(background);
  TextColor(foreground)
end;

{ Pad only this field. A shorter replacement cannot leave an old suffix. }
procedure Field(x, y, width: integer; text: screenline);
begin
  GotoXY(x, y);
  Say(Copy(text + padding + padding, 1, width))
end;

{ Only input rows are erased on an ordinary turn. Even 127 characters
  at the final Enter prompt fit above the lower border. }
procedure PrepareInput;
var
  y: integer;
begin
  Pen(LightBlue, Black);
  for y := 32 to 35 do
  begin
    GotoXY(1, y);
    ClrEol;
    Say(#179);
    GotoXY(79, y);
    Say(#179)
  end
end;

procedure StartScreen;
var
  x, y, n: integer;
begin
  CrtInit;
  Pen(LightGray, Black);
  ClrScr;
  Pen(LightBlue, Black);
  for y := 2 to 38 do
  begin
    GotoXY(1, y);
    Say(#179);
    GotoXY(79, y);
    Say(#179)
  end;
  for n := 1 to 5 do
    with borders[n] do
    begin
      GotoXY(1, y);
      Say(left);
      for x := 2 to 78 do
        Say(#196);
      Say(right)
    end;
  for y := 5 to 26 do
  begin
    GotoXY(53, y);
    Say(#179)
  end;
  GotoXY(53, 4);
  Say(#194);
  GotoXY(53, 27);
  Say(#193);
  Pen(LightGray, Blue);
  Field(2, 2, 77, '');
  for y := 5 to 26 do
    Field(54, y, 25, '');
  Pen(LightBlue, Blue);
  for y := 18 to 26 do
    Field(2, y, 51, '');
  for n := 1 to ArtCount do
    with scenery[n] do
    begin
      Pen(fg, bg);
      GotoXY(x, y);
      Say(text)
    end;
  for n := 1 to LabelCount do
    with labels[n] do
    begin
      Pen(fg, bg);
      GotoXY(x, y);
      Say(text)
    end
end;

{ The displayed values are a small snapshot, not another game model.
  No room, inventory, status or scene cell is touched unless it changes. }
procedure Refresh;
var
  thing: item;
  state: integer;
begin
  if (not painted) or (place <> shownroom) then
  begin
    Pen(LightCyan, Black);
    Field(3, 6, 48, titles[place]);
    Pen(LightGray, Black);
    Field(3, 8, 48, first[place]);
    Field(3, 9, 48, second[place]);
    Field(3, 11, 48, exits[place]);
    shownroom := place
  end;
  for thing := key to matches do
    if (not painted) or ((thing in bag) <> (thing in shownbag)) then
    begin
      if thing in bag then
        Pen(LightGreen, Blue)
      else
        Pen(LightGray, Blue);
      GotoXY(56, itemrows[thing]);
      if thing in bag then
        Say('[x] ')
      else
        Say('[ ] ');
      Say(itemnames[thing])
    end;
  shownbag := bag;
  if (not painted) or (opened <> showndoor) then
  begin
    if opened then
      Pen(LightGreen, Blue)
    else
      Pen(LightRed, Blue);
    if opened then
      Field(56, 17, 20, 'Open')
    else
      Field(56, 17, 20, 'Locked');
    showndoor := opened
  end;
  state := 0;
  if [oil, wick, matches] <= bag then
    state := 1;
  if won then
    state := 2;
  if state <> shownlamp then
  begin
    if state = 0 then
      Pen(LightGray, Blue)
    else
      Pen(Yellow, Blue);
    case state of
      0: Field(56, 23, 20, 'Supplies needed');
      1: Field(56, 23, 20, 'Ready to light');
      2:
        begin
          Field(56, 23, 20, 'SHINING');
          Pen(LightCyan, Black);
          Field(3, 8, 48, 'The great lens wakes above the black water.');
          Field(3, 9, 48, 'The lens turns. Light crosses the water.');
          Field(3, 11, 48, '')
        end
    end;
    shownlamp := state
  end;
  if (not painted) or (message <> shownmessage) or
     (messagecolor <> showncolor) then
  begin
    Pen(messagecolor, Black);
    Field(3, 28, 76, message);
    shownmessage := message;
    showncolor := messagecolor
  end;
  painted := True
end;

