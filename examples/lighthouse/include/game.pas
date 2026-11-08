{ Room observations and inventory changes. Navigation stays in the main loop. }

procedure Look;
begin
  messagecolor := LightGray;
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
  begin
    messagecolor := LightGray;
    message := 'You already have it.'
  end
  else
  begin
    bag := bag + [thing];
    messagecolor := LightGreen;
    message := 'Taken.'
  end
end;

