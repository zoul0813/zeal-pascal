program SilentLab;
{$R+}
var
  Room: Integer;
  Command: string[20];
begin
  Room := 1;
  WriteLn('THE SILENT LAB');
  WriteLn('Commands: n, s, e, w, look, quit');
  repeat
    WriteLn;
    case Room of
      1: WriteLn('Entrance. A door leads north.');
      2: WriteLn('Terminal room. Doors lead south and east.');
      3: WriteLn('Workshop. The terminal room is west.')
    end;
    Write('> ');
    ReadLn(Command);
    if Command = 'n' then
    begin
      if Room = 1 then Room := 2
      else WriteLn('There is no door north.')
    end
    else if Command = 's' then
    begin
      if Room = 2 then Room := 1
      else WriteLn('There is no door south.')
    end
    else if Command = 'e' then
    begin
      if Room = 2 then Room := 3
      else WriteLn('There is no door east.')
    end
    else if Command = 'w' then
    begin
      if Room = 3 then Room := 2
      else WriteLn('There is no door west.')
    end
    else if (Command <> 'look') and (Command <> 'quit') then
      WriteLn('Try n, s, e, w, look, or quit.')
  until Command = 'quit';
  WriteLn('You leave the lab. For now.')
end.
