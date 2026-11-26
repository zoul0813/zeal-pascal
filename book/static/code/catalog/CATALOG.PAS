program Collection;

{$R+}
{$I-}
const DataName = 'H:/work/catalog/ITEMS.DAT';
      MaxItems = 50;
type
  LabelText = string[24];
  Entry = record
    Title: LabelText;
    Active: Boolean
  end;
  Answer = string[25];
var
  Data: file of Entry;
  ReportFile: Text;
  Card: Entry;
  Line, Wanted: Answer;
  Opened, ExportOpen: Boolean;
  Status, Count, Index, Code: Integer;
  Command: string[2];

procedure Finish;
begin
  if ExportOpen then
  begin
    Close(ReportFile);
    Status := IOResult;
    if Status <> 0 then WriteLn('Export close error ', Status)
  end;
  if Opened then
  begin
    Close(Data);
    Status := IOResult;
    if Status <> 0 then WriteLn('Catalog close error ', Status)
  end
end;

procedure CheckIO;
begin
  Status := IOResult;
  if Status <> 0 then
  begin
    WriteLn('I/O error ', Status, '. Inspect your files before retrying.');
    Finish;
    Halt
  end
end;

procedure Fetch;
begin
  Seek(Data, Index);
  CheckIO;
  Read(Data, Card);
  CheckIO
end;

procedure Put;
begin
  Seek(Data, Index);
  CheckIO;
  Write(Data, Card);
  CheckIO;
  Flush(Data);
  CheckIO
end;

function ReadTitle: Boolean;
begin
  Write('Title (1..24 characters)> ');
  ReadLn(Line);
  CheckIO;
  ReadTitle := (Length(Line) > 0) and (Length(Line) <= 24)
end;

procedure ShowCards;
begin
  if Command = 'f' then
  begin
    Write('Search text> ');
    ReadLn(Wanted);
    CheckIO
  end;
  for Index := 0 to Count - 1 do
  begin
    Fetch;
    if Card.Active then
      if Command <> 'f' then WriteLn(Index + 1, ' ', Card.Title)
      else if Pos(Wanted, Card.Title) > 0 then WriteLn(Index + 1, ' ', Card.Title)
  end
end;

procedure ChangeCard;
begin
  Write('Item number> ');
  ReadLn(Line);
  CheckIO;
  if (Length(Line) = 0) or (Length(Line) > 2) then
  begin
    WriteLn('No such item.');
    Exit
  end;
  if (Line[1] < '0') or (Line[1] > '9') then
  begin
    WriteLn('Enter a listed decimal number.');
    Exit
  end;
  Val(Line, Index, Code);
  if Code <> 0 then
  begin
    WriteLn('Enter a listed number.');
    Exit
  end;
  if (Index < 1) or (Index > Count) then
  begin
    WriteLn('No such item.');
    Exit
  end;
  Index := Index - 1;
  Fetch;
  if not Card.Active then WriteLn('That item was removed.')
  else if Command = 'd' then
  begin
    Write('Remove this item (y/N)> ');
    ReadLn(Line);
    CheckIO;
    if Line = 'y' then
    begin
      Card.Active := False;
      Put;
      WriteLn('Removed.')
    end
  end
  else if ReadTitle then
  begin
    Card.Title := Line;
    Put;
    WriteLn('Updated.')
  end
  else WriteLn('Title unchanged.')
end;

procedure ExportCards;
begin
  Assign(ReportFile, 'H:/work/catalog/LIST.TXT');
  CheckIO;
  Reset(ReportFile);
  Status := IOResult;
  if Status = 0 then
  begin
    Close(ReportFile);
    CheckIO;
    WriteLn('LIST.TXT exists. Move or rename it first.');
    Exit
  end;
  if Status <> 1 then
  begin
    WriteLn('Cannot check export path: ', Status);
    Exit
  end;
  Rewrite(ReportFile);
  CheckIO;
  ExportOpen := True;
  for Index := 0 to Count - 1 do
  begin
    Fetch;
    if Card.Active then
    begin
      WriteLn(ReportFile, Index + 1, ' ', Card.Title);
      CheckIO
    end
  end;
  Close(ReportFile);
  ExportOpen := False;
  CheckIO;
  WriteLn('Wrote LIST.TXT.')
end;

begin
  Opened := False;
  ExportOpen := False;
  Assign(Data, DataName);
  CheckIO;
  Reset(Data);
  Status := IOResult;
  if Status = 1 then
  begin
    Write('Create ITEMS.DAT (y/N)> ');
    ReadLn(Line);
    CheckIO;
    if Line <> 'y' then Halt;
    Rewrite(Data);
    CheckIO
  end
  else if Status <> 0 then
  begin
    WriteLn('Cannot open catalog: ', Status);
    Halt
  end;
  Opened := True;
  Count := FileSize(Data);
  CheckIO;
  if (Count < 0) or (Count > MaxItems) then
  begin
    WriteLn('Unexpected catalog size.');
    Finish;
    Halt
  end;
  WriteLn('a add, l list, f find, u update, d remove, e export, q quit');
  repeat
    Write('Catalog> ');
    ReadLn(Command);
    CheckIO;
    if Command = 'a' then
      if Count = MaxItems then WriteLn('All 50 slots have been used.')
      else if ReadTitle then
      begin
        Index := Count;
        Card.Title := Line;
        Card.Active := True;
        Put;
        Count := Count + 1;
        WriteLn('Added as item ', Count)
      end
      else WriteLn('Title must be 1 to 24 characters.')
    else if (Command = 'l') or (Command = 'f') then ShowCards
    else if (Command = 'u') or (Command = 'd') then ChangeCard
    else if Command = 'e' then ExportCards
    else if Command <> 'q' then WriteLn('Choose a, l, f, u, d, e or q.')
  until Command = 'q';
  Finish
end.
