{$R-}
{ One typed random-access file and one shared record. Internal array indices
  follow explicit address, length, chain and constant-loop bounds. }
procedure Stop;
var status: integer;
begin
  status := 0;
  if opened then
  begin Close(sheet); status := IOResult; opened := False end;
  if status <> 0 then
    if exitstatus = 0 then exitstatus := 1;
  status := ZOS(ZOS_EXIT, exitstatus);
  Halt
end;

procedure Fatal(code: integer);
begin exitstatus := code; Stop end;

procedure Checked;
var status: integer;
begin
  status := IOResult;
  if status <> 0 then Fatal(31)
end;

procedure ReadRecord(index: integer);
var i: integer;
begin
  Seek(sheet, index); Checked; Read(sheet, entry); Checked;
  if Length(entry.text) > 63 then Fatal(5);
  if entry.check <> Checksum then Fatal(6);
  for i := 1 to Length(entry.text) do
    if (entry.text[i] < ' ') or (entry.text[i] > '~') then Fatal(7)
end;

procedure AppendRecord;
begin
  entry.check := Checksum;
  Seek(sheet, records); Checked; Write(sheet, entry); Checked;
  Close(sheet); opened := False; Checked;
  Reset(sheet); Checked; opened := True;
  if FileSize(sheet) <> records + 1 then Fatal(9);
  Checked
end;

procedure OpenSheet;
var i, status: integer;
begin
  FillChar(job.heads, SizeOf(job.heads), 0);
  Assign(sheet, path); Checked; Reset(sheet); status := IOResult;
  if status = 1 then
  begin
    Rewrite(sheet); Checked; opened := True; records := 0;
    FillChar(entry, SizeOf(entry), 0); entry.text := 'ZEALCALC 1'; AppendRecord
  end
  else if status <> 0 then Fatal(31);
  opened := True; records := FileSize(sheet); Checked;
  if (records < 1) or (records > MaxEdits + 1) then Fatal(14);
  ReadRecord(0);
  if (entry.row <> 0) or (entry.col <> 0) or (entry.previous <> 0) or
     (entry.text <> 'ZEALCALC 1') then Fatal(16);
  for i := 1 to records - 1 do
  begin
    ReadRecord(i);
    if (entry.row < 1) or (entry.row > MaxRows) or
       (entry.col < 1) or (entry.col > MaxCols) then Fatal(17);
    if entry.previous <> job.heads[entry.row] then Fatal(18);
    job.heads[entry.row] := i
  end;
  notice := 'Ready. Edits save immediately. / opens commands.'
end;

{$R+}
