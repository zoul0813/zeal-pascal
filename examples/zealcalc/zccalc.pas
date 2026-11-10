{ Read-only formula/render overlay. It returns to the preserved Pascal UI. }
program SheetCalc;
uses ZOS;
{$R+}{$I-}{$C-}{$A+}
{$I include/common.pas}
{$I include/backend.pas}
{$I include/types.pas}
{$I include/calc-data.pas}
var wire: array[0..3] of byte; complete: word;
  r, c, i, status: integer; offset: longword;
{$I include/checksum.pas}
{$I include/calc-store.pas}
{$I include/cells.pas}
{$I include/formula.pas}
begin
  { Reserve CPU space for guarded ZOS calls before any recursive frames exist. }
  RecurPtr := StackPtr - 2496;
  device := -1; viewdevice := -1; exitstatus := 0;
  device := OpenRead(RequestName);
  if device < 0 then Fatal(32);
  Transfer(device, wire, 4, False);
  if (wire[0] <> 1) or (wire[1] <> 0) or
     (wire[2] + wire[3] * 256 <> SizeOf(job)) then Fatal(34);
  Transfer(device, job, SizeOf(job), False);
  CloseDevice(device);
  if (job.tag <> WireTag) or (Length(job.path) < 1) or
     (Length(job.path) > 127) or (job.records < 1) or
     (job.records > MaxEdits + 1) or (Length(job.notice) > 75) or
     (job.top < 1) or (job.top > MaxRows - ViewRows + 1) or
     (job.left < 1) or (job.left > MaxCols - ViewCols + 1) then Fatal(34);
  records := job.records;
  for i := 1 to MaxRows do
    if job.heads[i] >= records then Fatal(34);
  device := OpenRead(job.path);
  if device < 0 then Fatal(2);
  Transfer(device, wire, 4, False);
  if (wire[0] + wire[1] * 256 <> records) or (wire[2] <> 70) or (wire[3] <> 0) then Fatal(34);
  viewdevice := ZOS(ZOS_OPEN, ViewName, ZOS_O_WRONLY);
  if viewdevice < 0 then Fatal(35);
  job.reply.header.tag := 0; job.reply.header.generation := job.generation;
  job.reply.header.top := job.top; job.reply.header.left := job.left;
  job.reply.header.records := records; job.reply.header.notice := job.notice;
  Transfer(viewdevice, job.reply.header, SizeOf(job.reply.header), True);
  for r := job.top to job.top + ViewRows - 1 do
    for c := job.left to job.left + ViewCols - 1 do
    begin
      RenderCell; Transfer(viewdevice, job.reply.visible, SizeOf(job.reply.visible), True)
    end;
  CloseDevice(device);
  offset := 0; SeekDevice(viewdevice, offset);
  complete := WireTag; Transfer(viewdevice, complete, SizeOf(complete), True);
  CloseDevice(viewdevice);
  Stop
end.
