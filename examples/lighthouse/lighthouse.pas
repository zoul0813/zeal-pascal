{
  LIGHTHOUSE - A four-room adventure on the Zeal text console.

  The inventory is a set; collect supplies and light the lamp to win.
  The 80 by 40 display uses TP3 cursor and video routines. Q or Ctrl-Z
  leaves normally; the winning screen waits for Enter or end of input.
}

program Lighthouse;
uses Crt;

{$R+}

type
  screenline = string[76];
  room = (shore, cottage, cellar, lamp);
  item = (key, oil, wick, matches);
  belongings = set of item;
  roomlines = array[room] of string[48];
  caption = record
    x, y: integer;
    bright: boolean;
    text: string[76]
  end;

{ Fixed layout and room prose are data, leaving one shared drawing loop. }
const
  captions: array[1..15] of caption = (
    (x: 3; y: 2; bright: True; text: 'LIGHTHOUSE'),
    (x: 56; y: 2; bright: False; text: 'Bring the ship home.'),
    (x: 56; y: 6; bright: True; text: 'INVENTORY'),
    (x: 56; y: 14; bright: True; text: 'CELLAR DOOR'),
    (x: 56; y: 18; bright: True; text: 'THE LAMP'),
    (x: 3; y: 28; bright: False;
     text: 'TAKE KEY / MATCHES in cottage; TAKE OIL / WICK in cellar.'),
    (x: 3; y: 29; bright: False;
     text: 'OPEN unlocks the cellar. LIGHT works in the lamp room.'),
    (x: 3; y: 37; bright: False;
     text:
       'N S U D: move   LOOK: examine   INV: inventory   ?: help   Q: quit'),
    (x: 23; y: 14; bright: False; text: '/\'),
    (x: 22; y: 15; bright: False; text: '/==\'),
    (x: 22; y: 16; bright: False; text: '|  |'),
    (x: 22; y: 17; bright: False; text: '|  |'),
    (x: 21; y: 18; bright: False; text: '/|  |\'),
    (x: 20; y: 19; bright: False; text: '/ |__| \'),
    (x: 5; y: 20; bright: False;
     text: '~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~')
  );
  titles: roomlines = (
    'THE SHORE',
    'THE COTTAGE',
    'THE CELLAR',
    'THE LAMP ROOM'
  );
  first: roomlines = (
    'Rain lashes the rocks. Beyond the breakers,',
    'Salt-stained windows rattle in their frames.',
    'Under the stairs: oil and spare wicks.',
    'The great lens waits above the black water.'
  );
  second: roomlines = (
    'a ship searches for the harbour light.',
    'Search here for a key and matches.',
    'The storm is a distant rumble overhead.',
    'Bring oil, a wick and matches. Try LIGHT.'
  );
  exits: roomlines = (
    'N: cottage    U: lamp room',
    'S: shore      D: cellar',
    'U: cottage',
    'D: shore'
  );
  itemnames: array[item] of string[8] = ('Key', 'Oil', 'Wick', 'Matches');
  itemrows: array[item] of integer = (8, 10, 11, 9);

var
  place: room;
  bag: belongings;
  opened, won, quit: boolean;
  command: string[40];
  message: string[76];
  i: integer;

{ Keep display text on the physical console, alongside GotoXY.
  Output may be redirected; only the closing epilogue uses that stream. }
procedure Say(text: screenline);
begin
  Write(Con, text)
end;

procedure Rule(y: integer);
var
  x: integer;

begin
  LowVideo;
  GotoXY(1, y);
  Say('+');
  for x := 2 to 78 do
    Say('-');
  Say('+')
end;

procedure EmptyRow(y: integer);
begin
  GotoXY(1, y);
  ClrEol;
  LowVideo;
  Say('|');
  GotoXY(79, y);
  Say('|');
  if (y >= 5) and (y <= 22) then
  begin
    GotoXY(53, y);
    Say('|')
  end
end;

{ Refresh the dynamic rows and restore the frame after command input.
  Rows 32 through 35 allow long input to wrap without scrolling. }
procedure Draw;
var
  y, n: integer;
  thing: item;

begin
  for y := 5 to 22 do
    EmptyRow(y);
  for y := 25 to 35 do
    EmptyRow(y);
  for n := 1 to 15 do
    with captions[n] do
    begin
      if bright then
        HighVideo
      else
        LowVideo;
      GotoXY(x, y);
      Say(text)
    end;
  HighVideo;
  GotoXY(3, 6);
  Say(titles[place]);
  LowVideo;
  GotoXY(3, 8);
  Say(first[place]);
  GotoXY(3, 9);
  Say(second[place]);
  GotoXY(3, 11);
  Say(exits[place]);
  for thing := key to matches do
  begin
    GotoXY(56, itemrows[thing]);
    if thing in bag then
    begin
      HighVideo;
      Say('[x] ')
    end
    else
    begin
      LowVideo;
      Say('[ ] ')
    end;
    Say(itemnames[thing])
  end;
  LowVideo;
  GotoXY(56, 16);
  if opened then
    Say('Open')
  else
    Say('Locked');
  GotoXY(56, 20);
  if won then
    Say('SHINING')
  else if [oil, wick, matches] <= bag then
    Say('Ready to light')
  else
    Say('Supplies needed');
  HighVideo;
  if won then
  begin
    GotoXY(22, 15);
    Say('/**\');
    GotoXY(3, 9);
    Say('The lens turns. Light crosses the water. ')
  end;
  GotoXY(3, 25);
  Say(message);
  GotoXY(3, 32);
  Say('Command> ')
end;

procedure Look;
begin
  case place of
    shore:
      message := 'A bell sounds offshore. The ship is still out there.';
    cottage:
      message := 'Water pools beneath a coat hanging by the door.';
    cellar:
      message := 'The air smells of lamp oil and damp timber.';
    lamp:
      message := 'Wind howls at the glass. The great lens stands dark.'
  end
end;

{ Set membership makes taking an item twice harmless. }
procedure Take(thing: item; where: room);
begin
  if place <> where then
    message := 'You cannot find that here.'
  else if thing in bag then
    message := 'You already have it.'
  else
  begin
    bag := bag + [thing];
    message := 'Taken.'
  end
end;

{ Grow the beam away from the lantern without overwriting the tower. }
procedure Beam;
var
  step: integer;

begin
  HighVideo;
  for step := 0 to 16 do
  begin
    GotoXY(21 - step, 15);
    Say('=');
    GotoXY(26 + step, 15);
    Say('=');
    Delay(30)
  end;
  Delay(1000)
end;

begin
  place := shore;
  bag := [];
  opened := False;
  won := False;
  quit := False;

  { Save Zeal terminal state before clearing or changing rendition. }
  CrtInit;
  ClrScr;
  Rule(1);
  Rule(4);
  Rule(23);
  Rule(36);
  Rule(39);
  EmptyRow(2);
  EmptyRow(3);
  EmptyRow(24);
  EmptyRow(37);
  EmptyRow(38);
  GotoXY(53, 4);
  Say('+');
  GotoXY(53, 23);
  Say('+');
  Look;
  repeat
    Draw;
    if Eof(Input) then
      command := 'Q'
    else
      ReadLn(Input, command);
    for i := 1 to Length(command) do
      command[i] := UpCase(command[i]);
    if (command = '?') or (command = 'HELP') then
      message :=
        'Find supplies, unlock the cellar, then LIGHT the lamp. See below.'
    else if (command = 'LOOK') or (command = '') then
      Look
    else if command = 'INV' then
      message := 'Inventory is listed in the panel at right.'
    else if command = 'TAKE KEY' then
      Take(key, cottage)
    else if command = 'TAKE MATCHES' then
      Take(matches, cottage)
    else if command = 'TAKE OIL' then
      Take(oil, cellar)
    else if command = 'TAKE WICK' then
      Take(wick, cellar)
    else if command = 'OPEN' then
    begin
      if place <> cottage then
        message := 'No locked door here.'
      else if key in bag then
      begin
        opened := True;
        message := 'The cellar door opens.'
      end
      else
        message := 'The cellar door needs a key.'
    end
    else if command = 'LIGHT' then
    begin
      if place <> lamp then
        message := 'You must reach the lamp first.'
      else if [oil, wick, matches] <= bag then
      begin
        won := True;
        message := 'The beam sweeps the water. A ship turns toward harbour.'
      end
      else
        message := 'You need oil, a wick and matches.'
    end
    else if command = 'N' then
    begin
      if place = shore then
      begin
        place := cottage;
        Look
      end
      else
        message := 'No way north.'
    end
    else if command = 'S' then
    begin
      if place = cottage then
      begin
        place := shore;
        Look
      end
      else
        message := 'No way south.'
    end
    else if command = 'D' then
    begin
      if place = lamp then
      begin
        place := shore;
        Look
      end
      else if place = cottage then
      begin
        if opened then
        begin
          place := cellar;
          Look
        end
        else
          message := 'The cellar door is locked.'
      end
      else
        message := 'No way down.'
    end
    else if command = 'U' then
    begin
      if place = shore then
      begin
        place := lamp;
        Look
      end
      else if place = cellar then
      begin
        place := cottage;
        Look
      end
      else
        message := 'No way up.'
    end
    else if (command = 'Q') or (command = 'QUIT') then
      quit := True
    else
      message := 'Unknown command. ? for help.'
  until won or quit;
  if won then
  begin
    Draw;
    EmptyRow(28);
    EmptyRow(29);
    Beam;
    EmptyRow(37);
    GotoXY(3, 37);
    Say('LIGHTHOUSE - the harbour has its light again.');
    GotoXY(1, 32);
    ClrEol;
    LowVideo;
    Say('|');
    GotoXY(79, 32);
    Say('|');
    HighVideo;
    GotoXY(3, 32);
    Say('The ship is safe. Press Enter to leave.');
    if not Eof(Input) then
      ReadLn(Input, command)
  end;

  { Normal exits share this cleanup. CrtExit restores terminal state,
    not the screen contents that the adventure has overwritten. }
  NormVideo;
  ClrScr;
  CrtExit;
  if won then
    WriteLn('The beam sweeps the water. A ship turns toward harbour.')
  else
    WriteLn('You leave the lighthouse. The lamp awaits another keeper.')
end.
