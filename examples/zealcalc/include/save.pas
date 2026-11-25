overlay procedure SaveSheet;
var i: integer; wire: array[0..1] of word;
{$R-}
function Checksum: word;
var i: integer; total: word;
begin
  total := entry.row + entry.col + entry.previous + Length(entry.text);
  for i := 1 to Length(entry.text) do total := total + Ord(entry.text[i]);
  Checksum := total
end;

{$R+}

procedure Fatal(code: integer);
begin BackendFailure(code) end;
procedure Transfer(var buffer; size: word; writing: boolean);
var amount: word; status: integer;
begin
  amount := size;
  if writing then status := ZOS(ZOS_WRITE, device, buffer, amount)
  else status := ZOS(ZOS_READ, device, buffer, amount);
  if (status <> 0) or (amount <> size) then Fatal(31)
end;
procedure SeekRecord(index: integer);
var offset: longword;
begin
  offset := index; offset := offset * 70 + 4;
  if ZOS(ZOS_SEEK, device, offset, ZOS_SEEK_SET) <> 0 then Fatal(3)
end;
procedure ReadRecord(index: integer);
var i: integer;
begin
  SeekRecord(index); Transfer(entry, SizeOf(entry), False);
  if Length(entry.text) > 63 then Fatal(5);
  if entry.check <> Checksum then Fatal(6);
  for i := 1 to Length(entry.text) do
    if (entry.text[i] < ' ') or (entry.text[i] > '~') then Fatal(7)
end;
procedure CloseSheet;
var status: integer;
begin
  status := ZOS(ZOS_CLOSE, device); device := -1;
  if status <> 0 then Fatal(36)
end;
procedure AppendRecord;
var offset: longword;
begin
  entry.check := Checksum; SeekRecord(records);
  Transfer(entry, SizeOf(entry), True);
  { Commit count only after writing the complete record, as with typed files. }
  offset := 0;
  if ZOS(ZOS_SEEK, device, offset, ZOS_SEEK_SET) <> 0 then Fatal(3);
  wire[0] := records + 1; Transfer(wire, 2, True);
  CloseSheet;
  device := ZOS(ZOS_OPEN, request.path, ZOS_O_RDWR);
  if device < 0 then Fatal(2);
  Transfer(wire, 4, False);
  if (wire[0] <> records + 1) or (wire[1] <> 70) then Fatal(9)
end;
procedure OpenSheet;
var i: integer; offset: longword;
begin
  FillChar(job.heads, SizeOf(job.heads), 0);
  device := ZOS(ZOS_OPEN, request.path, ZOS_O_RDWR);
  if device = -ZOS_ERR_NO_SUCH_ENTRY then
  begin
    device := ZOS(ZOS_OPEN, request.path, ZOS_O_RDWR + ZOS_O_CREAT);
    if device < 0 then Fatal(2);
    records := 0; wire[0] := 0; wire[1] := 70;
    Transfer(wire, 4, True);
    FillChar(entry, SizeOf(entry), 0); entry.text := 'ZEALCALC 1'; AppendRecord
  end
  else if device < 0 then Fatal(31);
  offset := 0;
  if ZOS(ZOS_SEEK, device, offset, ZOS_SEEK_SET) <> 0 then Fatal(3);
  Transfer(wire, 4, False); records := wire[0];
  if (wire[1] <> 70) or (records < 1) or (records > MaxEdits + 1) then Fatal(14);
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
{$R-}
{ Walk one row's validated disk history, retaining just the requested source. }
procedure LoadCell(r, c: integer);
var at: integer;
begin
  if (r < 1) or (r > MaxRows) or (c < 1) or (c > MaxCols) then Fatal(19);
  at := job.heads[r];
  while at <> 0 do
  begin
    ReadRecord(at);
    if (entry.row <> r) or (entry.previous >= at) then Fatal(20);
    if entry.col = c then Exit;
    at := entry.previous
  end;
  entry.text := ''
end;
{$R+}
{$R-}
procedure PutCell;
begin
  LoadCell(request.editrow, request.editcol);
  if request.source = entry.text then begin notice := 'Unchanged.'; Exit end;
  if records > MaxEdits then notice := '4096 edits reached. Back up this file; start another sheet.'
  else
  begin
    FillChar(entry, SizeOf(entry), 0);
    entry.row := request.editrow; entry.col := request.editcol;
    entry.previous := job.heads[request.editrow]; entry.text := request.source;
    AppendRecord; job.heads[request.editrow] := records; records := records + 1;
    notice := 'Saved.'
  end
end;
{$R+}
begin
  if (request.tag <> WireTag) or (Length(request.path) < 1) or
     (Length(request.path) > 127) or (Length(request.source) > 63) or
     (request.top < 1) or (request.top > MaxRows - ViewRows + 1) or
     (request.left < 1) or (request.left > MaxCols - ViewCols + 1) or
     (request.edit > 1) then Fatal(34);
  if request.edit = 1 then
  begin
    if (request.editrow < 1) or (request.editrow > MaxRows) or
       (request.editcol < 1) or (request.editcol > MaxCols) then Fatal(34);
    for i := 1 to Length(request.source) do
      if (request.source[i] < ' ') or (request.source[i] > '~') then Fatal(34)
  end;
  OpenSheet;
  if request.edit = 1 then
    PutCell;
  CloseSheet;
  job.tag := request.tag; job.generation := request.generation;
  job.path := request.path; job.top := request.top; job.left := request.left;
  job.records := records; 
end;
