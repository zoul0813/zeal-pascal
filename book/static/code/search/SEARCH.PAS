program SearchNotes;

{$I-}
const Sample = 'H:/book/search/NOTES.TXT';
      ReportName = 'H:/work/search/REPORT.TXT';
type Query = string[21];
var
  Source, ReportFile: Text;
  InputName: string[128];
  Answer: string[3];
  Term, Window: Query;
  Ch: Char;
  Status, LineNo, Hits: Integer;
  SourceOpen, ReportOpen, Ended, HasText, Matched: Boolean;

procedure Finish;
begin
  if SourceOpen then
  begin
    Close(Source);
    Status := IOResult;
    if Status <> 0 then WriteLn('Input close error ', Status)
  end;
  if ReportOpen then
  begin
    Close(ReportFile);
    Status := IOResult;
    if Status <> 0 then WriteLn('Report close error ', Status)
  end
end;

procedure CheckIO;
begin
  Status := IOResult;
  if Status <> 0 then
  begin
    WriteLn('I/O error ', Status, '. Inspect the files before retrying.');
    Finish;
    Halt
  end
end;

procedure NewReport;
begin
  Assign(ReportFile, ReportName);
  CheckIO;
  Reset(ReportFile);
  Status := IOResult;
  if Status = 0 then
  begin
    ReportOpen := True;
    WriteLn('Report already exists. Move or rename it first.');
    Finish;
    Halt
  end;
  if Status <> 1 then
  begin
    WriteLn('Cannot check report path: ', Status);
    Finish;
    Halt
  end;
  Rewrite(ReportFile);
  CheckIO;
  ReportOpen := True;
  WriteLn(ReportFile, 'Matches for: ', Term);
  CheckIO
end;

procedure EndLine;
begin
  if Matched then
  begin
    Hits := Hits + 1;
    WriteLn('Line ', LineNo);
    if ReportOpen then
    begin
      WriteLn(ReportFile, 'Line ', LineNo);
      CheckIO
    end
  end;
  Matched := False;
  HasText := False;
  Window := ''
end;

begin
  SourceOpen := False;
  ReportOpen := False;
  Write('Input file (Enter for lesson notes)> ');
  ReadLn(InputName);
  CheckIO;
  if InputName = '' then InputName := Sample;
  if Length(InputName) > 127 then
  begin
    WriteLn('Path too long.');
    Halt
  end;
  Write('Search text (1..20 characters)> ');
  ReadLn(Term);
  CheckIO;
  if (Length(Term) < 1) or (Length(Term) > 20) then
  begin
    WriteLn('Enter 1 to 20 characters.');
    Halt
  end;
  Write('Write a new report (y/N)> ');
  ReadLn(Answer);
  CheckIO;
  Assign(Source, InputName);
  CheckIO;
  Reset(Source);
  CheckIO;
  SourceOpen := True;
  if Answer = 'y' then NewReport;
  LineNo := 1;
  Hits := 0;
  Window := '';
  HasText := False;
  Matched := False;
  Ended := Eof(Source);
  CheckIO;
  while not Ended do
  begin
    if LineNo > 30000 then
    begin
      WriteLn('Stopped at 30000 lines. Results are partial.');
      Finish;
      Halt
    end;
    Read(Source, Ch);
    CheckIO;
    if Ch = #10 then
    begin
      EndLine;
      LineNo := LineNo + 1
    end
    else if Ch <> #13 then
    begin
      HasText := True;
      if not Matched then
      begin
        if Length(Window) = Length(Term) then Delete(Window, 1, 1);
        Window := Window + Ch;
        Matched := Window = Term
      end
    end;
    Ended := Eof(Source);
    CheckIO
  end;
  if HasText then EndLine;
  WriteLn('Matching lines: ', Hits);
  if ReportOpen then
  begin
    WriteLn(ReportFile, 'Matching lines: ', Hits);
    CheckIO
  end;
  Finish
end.
