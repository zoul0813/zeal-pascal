{
  PARTS BIN - A small stock ledger in a typed random-access file.

  Each record holds a part code, description and quantity. Relative
  record numbers begin at zero. Back up parts.dat before experimenting
  with updates; this example does not maintain a transaction journal.
}

program PartsBin;

{$R+}
{$I-}

type
  line = string[40];
  part = record
    code: string[6];
    name: string[20];
    qty: integer
  end;

var
  stock: file of part;
  p: part;
  s, cmd: line;
  wanted: string[6];
  count, slot, i, amount, error: integer;
  found, quit, opened: boolean;

{ Disk operations run with I/O checking off and inspect IOResult here. }
procedure Checked;
var
  status: integer;

begin
  status := IOResult;
  if status <> 0 then
  begin
    WriteLn('I/O ', status, '. Inspect parts.dat.');
    Halt
  end
end;

{ Reset can fail before stock is open; do not Close an unopened file. }
procedure Finish;
begin
  if opened then
  begin
    Close(stock);
    opened := False;
    Checked
  end
end;

{ Ctrl-Z follows the same file-finalization path as a normal quit. }
procedure InputLine;
begin
  {$I+}
  if Eof(Input) then
  begin
    {$I-}
    Finish;
    Halt
  end;
  {$I+}
  ReadLn(Input, s);
  {$I-}
end;

procedure Fetch;
begin
  Seek(stock, slot);
  Checked;
  Read(stock, p);
  Checked;
  if (p.code = '') or (p.qty < 0) or (p.qty > 30000) then
  begin
    WriteLn('Bad record. Restore backup.');
    Halt
  end
end;

{ Flush an accepted update before returning to the command prompt. }
procedure Put;
begin
  Seek(stock, slot);
  Checked;
  Write(stock, p);
  Checked;
  Flush(stock);
  Checked
end;

{ Linear search returns either the matching slot or count for an append. }
procedure Locate;
begin
  Write('Code> ');
  InputLine;
  wanted := s;
  for i := 1 to Length(wanted) do
    wanted[i] := UpCase(wanted[i]);
  slot := 0;
  found := False;
  while (slot < count) and not found do
  begin
    Fetch;
    if p.code = wanted then
      found := True
    else
      slot := slot + 1
  end
end;

procedure Row;
begin
  WriteLn(p.code:6, ' ', p.name:20, ' ', p.qty:5)
end;

begin
  opened := False;
  WriteLn('Parts Bin - A add, L list, U update, Q quit.');
  Assign(stock, 'parts.dat');
  Checked;
  Reset(stock);
  error := IOResult;
  if error = 1 then
  begin
    Write('Create parts.dat? Y/N> ');
    InputLine;
    if (s <> 'Y') and (s <> 'y') then
      Halt;
    Rewrite(stock);
    Checked
  end
  else if error <> 0 then
  begin
    WriteLn('Open error ', error);
    Halt
  end;
  opened := True;
  count := FileSize(stock);
  Checked;
  if (count < 0) or (count > 100) then
  begin
    WriteLn('Record limit: 100.');
    Halt
  end;
  quit := False;
  repeat
    Write('> ');
    InputLine;
    cmd := s;
    for i := 1 to Length(cmd) do
      cmd[i] := UpCase(cmd[i]);
    if cmd = 'L' then
    begin
      WriteLn('  Code                 Name   Qty');
      for slot := 0 to count - 1 do
      begin
        Fetch;
        Row
      end
    end
    else if cmd = 'A' then
    begin
      if count = 100 then
        WriteLn('Full (100 records).')
      else
      begin
        Locate;
        if found then
          WriteLn('Duplicate code.')
        else if wanted = '' then
          WriteLn('Code required.')
        else
        begin
          p.code := wanted;
          Write('Name> ');
          InputLine;
          p.name := s;
          p.qty := 0;
          slot := count;
          Put;
          count := count + 1;
          WriteLn('Added ', p.code, '.')
        end
      end
    end
    else if cmd = 'U' then
    begin
      Locate;
      if not found then
        WriteLn('Code not found.')
      else
      begin
        Write('Change (-30000..30000)> ');
        InputLine;
        amount := 0;
        Val(s, amount, error);

        { Validate the delta and final range before adding; Integer
          overflow must not be used to detect a quantity error. }
        if (s = '') or (error <> 0) or
           (amount < -30000) or (amount > 30000) then
          WriteLn('Invalid change.')
        else if (amount < 0) and (p.qty < -amount) then
          WriteLn('Not enough stock.')
        else if (amount > 0) and (p.qty > 30000 - amount) then
          WriteLn('Limit: 30000.')
        else
        begin
          p.qty := p.qty + amount;
          Put;
          Row
        end
      end
    end
    else if cmd = 'Q' then
      quit := True
    else
      WriteLn('A add, L list, U update, Q quit.')
  until quit;
  Finish
end.
