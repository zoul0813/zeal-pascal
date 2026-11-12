overlay procedure View(action, r, c: integer);
procedure Transfer(fd: integer; var buffer; size: word; writing: boolean);
var amount: word; status: integer;
begin
  amount := size;
  if writing then status := ZOS(ZOS_WRITE, fd, buffer, amount)
  else status := ZOS(ZOS_READ, fd, buffer, amount);
  if (status <> 0) or (amount <> size) then Fatal('Scratch I/O failed; last edit may already be saved.')
end;

procedure ReserveScratch;
var stat: TZOSStat; status, fd: integer;
begin
  status := ZOS(ZOS_STAT, ViewName, stat);
  if status <> ZOS_ERR_NO_SUCH_ENTRY then Fatal('Scratch file exists or cannot be checked. See README.');
  fd := ZOS(ZOS_OPEN, ViewName, ZOS_O_WRONLY + ZOS_O_CREAT);
  if fd < 0 then Fatal('Cannot reserve scratch file.');
  ownview := True; status := ZOS(ZOS_CLOSE, fd);
  if status <> 0 then Fatal('Cannot close scratch file.')
end;

procedure RefreshView;
var status: integer;
begin
  if viewdevice >= 0 then
  begin
    status := ZOS(ZOS_CLOSE, viewdevice); viewdevice := -1;
    if status <> 0 then Fatal('Cannot close viewport.')
  end;
  if request.generation = 65535 then request.generation := 1
  else request.generation := request.generation + 1;
  request.tag := WireTag;
  request.top := top; request.left := left;
  SaveSheet; Calculate;
  viewdevice := ZOS(ZOS_OPEN, ViewName, ZOS_O_RDONLY);
  if viewdevice < 0 then Fatal('Cannot open viewport.');
  Transfer(viewdevice, job.reply.header, SizeOf(job.reply.header), False);
  if (job.reply.header.tag <> WireTag) or (job.reply.header.generation <> request.generation) or
     (job.reply.header.top <> top) or (job.reply.header.left <> left) or
     (job.reply.header.records < 1) or (job.reply.header.records > MaxEdits + 1) or
     (Length(job.reply.header.notice) > 75) then Fatal('Invalid viewport.');
  records := job.reply.header.records; notice := job.reply.header.notice;
  request.edit := 0; fetchedrow := 0
end;
procedure FetchCell(r, c: integer);
var offset: longword; at: integer;
begin
  if (r = fetchedrow) and (c = fetchedcol) then Exit;
  if (r < top) or (r >= top + ViewRows) or
     (c < left) or (c >= left + ViewCols) then Fatal('Invalid viewport cell.');
  at := (r - top) * ViewCols + c - left;
  offset := SizeOf(job.reply.header) + at * SizeOf(job.reply.visible);
  at := ZOS(ZOS_SEEK, viewdevice, offset, ZOS_SEEK_SET);
  if at <> 0 then Fatal('Viewport seek failed.');
  Transfer(viewdevice, job.reply.visible, SizeOf(job.reply.visible), False);
  if (Length(job.reply.visible.source) > 63) or (Length(job.reply.visible.shown) > 9) then Fatal('Invalid viewport record.');
  fetchedrow := r; fetchedcol := c
end;

begin
  case action of
    0: RefreshView;
    1: FetchCell(r, c);
    2: ReserveScratch
  end
end;
