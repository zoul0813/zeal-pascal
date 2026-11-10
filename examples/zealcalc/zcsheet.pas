{ Validate/save overlay. Tail-replace this process with the formula overlay. }
program SheetWorker;
uses ZOS;
{$R+}{$I-}{$C-}{$A+}
{$I include/common.pas}
{$I include/backend.pas}
{$I include/types.pas}
{$I include/data.pas}
var request: SheetRequest;
  requestfile: file; calcfile: file of CalcRequest;
  i, status: integer; code: byte;
{$I include/checksum.pas}
{$I include/store.pas}
{$I include/cells.pas}
{$I include/put-cell.pas}
begin
  opened := False; exitstatus := 0;
  Assign(requestfile, RequestName); Checked; Reset(requestfile); Checked;
  BlockRead(requestfile, request, 2); Checked; Close(requestfile); Checked;
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
  path := request.path; OpenSheet;
  if request.edit = 1 then
  begin row := request.editrow; col := request.editcol; PutCell(request.source) end;
  Close(sheet); opened := False; Checked;
  job.tag := request.tag; job.generation := request.generation;
  job.path := request.path; job.top := request.top; job.left := request.left;
  job.records := records; job.notice := notice;
  Assign(calcfile, RequestName); Checked; Rewrite(calcfile); Checked;
  Write(calcfile, job); Checked; Close(calcfile); Checked;
  code := 255;
  status := ZOS(ZOS_EXEC, ZOS_EXEC_OVERRIDE_PROGRAM, 'zccalc.bin', '', code);
  { Successful override does not return. The UI rejects the old generation
    if load/exec fails, including when the edit has already committed. }
  exitstatus := 32; Stop
end.
