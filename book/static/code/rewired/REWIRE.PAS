program LabRewired;

{$R+}
type
  Room = (NoRoom, Entry, Terminal, Workshop, Store, Roof);
  Direction = (North, South, East, West);
  Item = (Fuse, Key);
  Pack = set of Item;
  RoomInfo = record
    Title: string[32];
    Doors: array[Direction] of Room
  end;

var
  Map: array[Room] of RoomInfo;
  Here, Place: Room;
  Way: Direction;
  Holding: Pack;
  Command: string[24];
  Won: Boolean;

procedure BuildMap;
begin
  for Place := Entry to Roof do
    for Way := North to West do Map[Place].Doors[Way] := NoRoom;
  Map[Entry].Title := 'Entrance';
  Map[Terminal].Title := 'Terminal room';
  Map[Workshop].Title := 'Workshop';
  Map[Store].Title := 'Store room';
  Map[Roof].Title := 'Roof';
  Map[Entry].Doors[North] := Terminal;
  Map[Terminal].Doors[South] := Entry;
  Map[Terminal].Doors[East] := Workshop;
  Map[Workshop].Doors[West] := Terminal;
  Map[Workshop].Doors[South] := Store;
  Map[Store].Doors[North] := Workshop;
  Map[Workshop].Doors[North] := Roof;
  Map[Roof].Doors[South] := Workshop
end;

procedure Move(DirectionToGo: Direction);
var Destination: Room;
begin
  Destination := Map[Here].Doors[DirectionToGo];
  if Destination = NoRoom then WriteLn('There is no door there.')
  else if (Destination = Roof) and not (Key in Holding) then
    WriteLn('The roof hatch is locked.')
  else Here := Destination
end;

procedure Look;
begin
  WriteLn(Map[Here].Title);
  case Here of
    Terminal: if not (Key in Holding) then WriteLn('A brass key lies on a terminal.');
    Workshop: WriteLn('North: roof hatch. South: store room.');
    Store: if not (Fuse in Holding) then WriteLn('A spare fuse is on the shelf.');
    Roof: WriteLn('A dark beacon has an empty fuse holder.')
  end
end;

begin
  BuildMap;
  Holding := [];
  Here := Entry;
  Won := False;
  WriteLn('Find the key and fuse and light the beacon.');
  WriteLn('n, s, e, w, look, take key, take fuse, repair, q');
  Look;
  repeat
    Write('> ');
    ReadLn(Command);
    if Command = 'n' then begin Move(North); Look end
    else if Command = 's' then begin Move(South); Look end
    else if Command = 'e' then begin Move(East); Look end
    else if Command = 'w' then begin Move(West); Look end
    else if Command = 'look' then Look
    else if Command = 'take key' then
      if (Here = Terminal) and not (Key in Holding) then
      begin
        Holding := Holding + [Key];
        WriteLn('Taken.')
      end
      else WriteLn('No key to take here.')
    else if Command = 'take fuse' then
      if (Here = Store) and not (Fuse in Holding) then
      begin
        Holding := Holding + [Fuse];
        WriteLn('Taken.')
      end
      else WriteLn('No fuse to take here.')
    else if Command = 'repair' then
      if (Here = Roof) and (Fuse in Holding) then Won := True
      else WriteLn('You need the fuse at the roof beacon.')
    else if Command <> 'q' then
      WriteLn('Try look or a direction.')
  until (Command = 'q') or Won;
  if Won then WriteLn('The beacon lights. The lab is back on the map.')
end.
