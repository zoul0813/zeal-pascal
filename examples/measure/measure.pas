{
  MEASURE LAB - Collect a batch for a separate report program.

  values.dat is a native typed file of Real values. A new batch needs
  confirmation because Rewrite replaces the previous contents.
  Run stats.bin through Execute only after closing the data file.
}

program MeasureLab;

{$R+}
{$I-}

var
  samples: file of real;
  next: file;
  text: string[40];
  cmd: char;
  value: real;
  count, status, code: integer;
  opened, done, quit: boolean;

{ Capture IOResult before cleanup, keeping the original error code.
  On failure, try to close an open data file before halting. }
procedure Checked;
var
  error, cleanup: integer;

begin
  error := IOResult;
  if error <> 0 then
  begin
    if opened then
    begin
      Close(samples);
      cleanup := IOResult;
      opened := False
    end;
    WriteLn('Data I/O error ', error);
    Halt
  end
end;

{ All normal departures finalize the typed output file. }
procedure Finish;
begin
  if opened then
  begin
    Close(samples);
    Checked;
    opened := False
  end
end;

{ Console input uses checked I/O; Ctrl-Z closes any active batch. }
procedure Line;
begin
  {$I+}
  if Eof(Input) then
  begin
    {$I-}
    Finish;
    Halt
  end;
  {$I+}
  ReadLn(Input, text);
  {$I-}
end;

begin
  opened := False;
  quit := False;
  Assign(samples, 'values.dat');
  Checked;
  WriteLn('Measure Lab - N new batch, R report, Q quit.');
  repeat
    Write('> ');
    Line;
    cmd := '?';
    if Length(text) = 1 then
      cmd := UpCase(text[1]);
    if cmd = 'N' then
    begin
      Write('Start a new batch (replaces saved values.dat)? Y/N> ');
      Line;
      if (text = 'Y') or (text = 'y') then
      begin
        Rewrite(samples);
        Checked;
        opened := True;
        count := 0;
        done := False;
        WriteLn('Enter measurements -1000000..1000000; blank finishes.');
        repeat
          Write('Value> ');
          Line;
          if text = '' then
            done := True
          else
          begin
            value := 0.0;
            Val(text, value, code);
            if (code <> 0) or (Abs(value) > 1000000.0) then
              WriteLn('Invalid measurement.')
            else
            begin
              Write(samples, value);
              Checked;
              count := count + 1
            end
          end;
          if count = 100 then
          begin
            WriteLn('Batch limit: 100.');
            done := True
          end
        until done;
        Finish;
        if count > 0 then
          cmd := 'R'
        else
          WriteLn('Empty batch saved.')
      end
    end
    else if cmd = 'Q' then
      quit := True
    else if cmd <> 'R' then
      WriteLn('N new batch, R report, Q quit.');

    { Execute replaces this program on success; only a failed load
      reaches the diagnostic below. values.dat carries the batch
      into the report program. }
    if cmd = 'R' then
    begin
      Finish;
      Assign(next, 'stats.bin');
      Checked;
      Execute(next);
      status := IOResult;
      WriteLn('Cannot start stats.bin: ', status, '. Saved data retained.')
    end
  until quit;
  Finish
end.
