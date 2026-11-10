{ UI owns two private scratch files for one synchronous worker at a time. }
procedure Stop;
var status, i: integer; name: CellText;
begin
  if viewdevice >= 0 then status := ZOS(ZOS_CLOSE, viewdevice);
  for i := 1 to 2 do
    if ((i = 1) and ownrequest) or ((i = 2) and ownview) then
    begin
      if i = 1 then name := RequestName else name := ViewName;
      status := ZOS(ZOS_RM, name)
    end;
  if active then CrtExit;
  if notice <> '' then WriteLn(notice);
  Halt
end;

procedure Fatal(message: CellText);
begin
  notice := message; Stop
end;

procedure Transfer(fd: integer; var buffer; size: word; writing: boolean);
var amount: word; status: integer;
begin
  amount := size;
  if writing then status := ZOS(ZOS_WRITE, fd, buffer, amount)
  else status := ZOS(ZOS_READ, fd, buffer, amount);
  if (status <> 0) or (amount <> size) then Fatal('Scratch I/O failed; last edit may already be saved.')
end;

procedure ReserveScratch;
var stat: TZOSStat; status, fd, i: integer; name: CellText;
begin
  for i := 1 to 2 do
  begin
    if i = 1 then name := RequestName else name := ViewName;
    status := ZOS(ZOS_STAT, name, stat);
    if status <> ZOS_ERR_NO_SUCH_ENTRY then Fatal('Scratch file exists or cannot be checked. See README.');
    fd := ZOS(ZOS_OPEN, name, ZOS_O_WRONLY + ZOS_O_CREAT);
    if fd < 0 then Fatal('Cannot reserve scratch file.');
    if i = 1 then ownrequest := True else ownview := True;
    status := ZOS(ZOS_CLOSE, fd);
    if status <> 0 then Fatal('Cannot close scratch file.')
  end
end;

procedure RefreshView;
var fd, status: integer; code: byte; text: CellText;
begin
  if viewdevice >= 0 then
  begin
    status := ZOS(ZOS_CLOSE, viewdevice); viewdevice := -1;
    if status <> 0 then Fatal('Cannot close viewport.')
  end;
  if request.generation = 65535 then request.generation := 1
  else request.generation := request.generation + 1;
  request.tag := WireTag; request.path := path;
  request.top := top; request.left := left;
  fd := ZOS(ZOS_OPEN, RequestName, ZOS_O_WRONLY);
  if fd < 0 then Fatal('Cannot open request.');
  Transfer(fd, request, SizeOf(request), True);
  status := ZOS(ZOS_CLOSE, fd);
  if status <> 0 then Fatal('Cannot close request.');
  code := 255;
  status := ZOS(ZOS_EXEC, ZOS_EXEC_PRESERVE_PROGRAM, 'zcsheet.bin', '', code);
  if status <> 0 then Fatal('Cannot run zcsheet.bin; use the application directory.');
  if code <> 0 then
  begin
    Str(code, text);
    Fatal('Worker failed (' + text + '); last edit may already be saved.')
  end;
  viewdevice := ZOS(ZOS_OPEN, ViewName, ZOS_O_RDONLY);
  if viewdevice < 0 then Fatal('Cannot open viewport.');
  Transfer(viewdevice, header, SizeOf(header), False);
  if (header.tag <> WireTag) or (header.generation <> request.generation) or (header.top <> top) or (header.left <> left) or
     (header.records < 1) or (header.records > MaxEdits + 1) or
     (Length(header.notice) > 75) then Fatal('Invalid worker response.');
  records := header.records; notice := header.notice;
  request.edit := 0; fetchedrow := 0
end;

procedure FetchCell(r, c: integer);
var offset: longword; at: integer;
begin
  if (r = fetchedrow) and (c = fetchedcol) then Exit;
  if (r < top) or (r >= top + ViewRows) or
     (c < left) or (c >= left + ViewCols) then Fatal('Invalid viewport cell.');
  at := (r - top) * ViewCols + c - left;
  offset := SizeOf(header) + at * SizeOf(visible);
  at := ZOS(ZOS_SEEK, viewdevice, offset, ZOS_SEEK_SET);
  if at <> 0 then Fatal('Viewport seek failed.');
  Transfer(viewdevice, visible, SizeOf(visible), False);
  if (Length(visible.source) > 63) or (Length(visible.shown) > 9) then Fatal('Invalid viewport record.');
  fetchedrow := r; fetchedcol := c
end;

function Cell(r, c: integer): CellText;
begin
  FetchCell(r, c); Cell := visible.source
end;

function DisplayCell(r, c: integer): CellText;
begin
  FetchCell(r, c); labelcell := visible.labelcell; DisplayCell := visible.shown
end;

procedure PutCell(text: CellText);
begin
  request.edit := 1; request.editrow := row; request.editcol := col;
  request.source := text; painted := False
end;
