{$R-}
{ Read-only typed-sheet payload through checked ZOS I/O. The save overlay has
  already validated every record; the calculator still checks each fetched record. }
procedure Stop;
var status: integer;
begin
  if device >= 0 then status := ZOS(ZOS_CLOSE, device);
  if viewdevice >= 0 then status := ZOS(ZOS_CLOSE, viewdevice);
  status := ZOS(ZOS_EXIT, exitstatus);
  Halt
end;
procedure Fatal(code: integer);
begin exitstatus := code; Stop end;

{ Centralize descriptor lifecycle and syscall staging in this overlay. }
procedure CloseDevice(var fd: integer);
var status: integer;
begin
  status := ZOS(ZOS_CLOSE, fd); fd := -1;
  if status <> 0 then Fatal(36)
end;
procedure SeekDevice(fd: integer; offset: longword);
var status: integer;
begin
  status := ZOS(ZOS_SEEK, fd, offset, ZOS_SEEK_SET);
  if status <> 0 then Fatal(3)
end;
function OpenRead(name: PathText): integer;
begin
  OpenRead := ZOS(ZOS_OPEN, name, ZOS_O_RDONLY)
end;

procedure Transfer(fd: integer; var buffer; size: word; writing: boolean);
var amount: word; status: integer;
begin
  amount := size;
  if writing then status := ZOS(ZOS_WRITE, fd, buffer, amount)
  else status := ZOS(ZOS_READ, fd, buffer, amount);
  if (status <> 0) or (amount <> size) then Fatal(31)
end;

procedure ReadRecord(index: integer);
var offset: longword; i: integer;
begin
  if (index < 0) or (index >= records) then Fatal(19);
  offset := index; offset := offset * 70 + 4;
  SeekDevice(device, offset);
  Transfer(device, entry, SizeOf(entry), False);
  if Length(entry.text) > 63 then Fatal(5);
  if entry.check <> Checksum then Fatal(6);
  for i := 1 to Length(entry.text) do
    if (entry.text[i] < ' ') or (entry.text[i] > '~') then Fatal(7)
end;

{$R+}
