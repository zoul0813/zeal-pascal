{
  LIGHTHOUSE - A four-room adventure on the Zeal text console.

  CP437 strokes and foreground/background colors form a night harbour.
  The fixed scene is drawn once. Cached game state updates only changed
  fields; command input has its own small clearing area. Q or Ctrl-Z
  leaves normally; the winning screen waits for Enter or end of input.
}

program Lighthouse;

{$R+}
{$C-}
{$B-}
{ Use Q/Ctrl-Z exits rather than teletype output flow control. }

{ Includes share this program's declarations; they are not separate units. }
{$I include/data.pas}
{$I include/screen.pas}
{$I include/game.pas}
{$I include/beam.pas}

begin
  place := shore;
  shownroom := shore;
  bag := [];
  shownbag := [];
  opened := False;
  showndoor := False;
  won := False;
  quit := False;
  painted := False;
  shownlamp := -1;
  shownmessage := '';
  showncolor := Black;
  StartScreen;
  Look;
  repeat
    Refresh;
    PrepareInput;
    Pen(LightCyan, Black);
    GotoXY(3, 32);
    Say('Command> ');
    Pen(White, Black);
    { TP3 B- keeps a real CR distinct from Ctrl-Z. Read the complete
      physical line before limiting the command to 40 characters. }
    ReadLn(raw, terminator);
    if terminator = #26 then
      command := 'Q'
    else
      command := Copy(raw, 1, 40);
    messagecolor := LightRed;
    for i := 1 to Length(command) do
      command[i] := UpCase(command[i]);
    { Match complete names, including aliases, before dispatching. }
    choice := UnknownCommand;
    for i := 1 to CommandCount do
      if command = commands[i].name then
        choice := commands[i].code;
    case choice of
      HelpCommand:
        begin
          messagecolor := LightCyan;
          message :=
            'Find supplies, unlock the cellar, then LIGHT the lamp. See below.'
        end;
      LookCommand:
        Look;
      InventoryCommand:
        begin
          messagecolor := LightCyan;
          message := 'Inventory is listed in the panel at right.'
        end;
      KeyCommand: Take(key, cottage);
      OilCommand: Take(oil, cellar);
      WickCommand: Take(wick, cellar);
      MatchesCommand: Take(matches, cottage);
      OpenCommand:
        begin
          if place <> cottage then
            message := 'No locked door here.'
          else if key in bag then
          begin
            opened := True;
            messagecolor := LightGreen;
            message := 'The cellar door opens.'
          end
          else
            message := 'The cellar door needs a key.'
        end;
      LightCommand:
        begin
          if place <> lamp then
            message := 'You must reach the lamp first.'
          else if [oil, wick, matches] <= bag then
          begin
            won := True;
            messagecolor := Yellow;
            message := 'The beam sweeps the water. A ship turns toward harbour.'
          end
          else
            message := 'You need oil, a wick and matches.'
        end;
      NorthCommand:
        begin
          if place = shore then
          begin
            place := cottage;
            Look
          end
          else
            message := 'No way north.'
        end;
      SouthCommand:
        begin
          if place = cottage then
          begin
            place := shore;
            Look
          end
          else
            message := 'No way south.'
        end;
      DownCommand:
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
        end;
      UpCommand:
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
        end;
      QuitCommand:
        quit := True;
      UnknownCommand:
        message := 'Unknown command. ? for help.'
    end
  until won or quit;
  if won then
  begin
    PrepareInput;
    Pen(LightGray, Black);
    Field(3, 30, 76, '');
    Field(3, 31, 76, '');
    Beam;
    Refresh;
    Pen(Yellow, Black);
    Field(3, 37, 76, 'LIGHTHOUSE - the harbour has its light again.');
    Pen(Yellow, Black);
    GotoXY(3, 32);
    Say('The ship is safe. Press Enter to leave.');
    Pen(White, Black);
    ReadLn(raw, terminator)
  end;

  { Normal exits restore terminal state, not the overwritten screen. }
  Pen(White, Black);
  ClrScr;
  CrtExit;
  if not won then
    message := 'You leave the lighthouse. The lamp awaits another keeper.';
  WriteLn(message)
end.
