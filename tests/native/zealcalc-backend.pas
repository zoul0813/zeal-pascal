{ Acceptance harness: the same save/calculate members used by the UI. }
program CalcTest;
uses ZOS;
{$R+}{$I-}{$C-}{$A+}
{$I include/common.pas}
{$I include/backend.pas}
{$I include/types.pas}
var request: SheetRequest; job: CalcRequest; entry: DiskCell;
  records, row, col, device, outputdevice: integer;
  notice: string[75]; labelcell: boolean;
  fd, status: integer; amount: word; cachepage: byte;
procedure BackendFailure(code: integer); forward;
procedure CacheCopy(offset: word; var buffer; size: word; reading: boolean); forward;
procedure BackendFailure;
begin
  if device >= 0 then status := ZOS(ZOS_CLOSE, device);
  if outputdevice >= 0 then status := ZOS(ZOS_CLOSE, outputdevice);
  if cachepage <> 255 then status := ZOS(ZOS_PFREE, cachepage);
  status := ZOS(ZOS_EXIT, code); Halt
end;
{$I include/cache.pas}
{$I include/save.pas}
{$I include/calculate.pas}
begin
  OvrDrive(0); device := -1; outputdevice := -1; cachepage := 255;
  status := ZOS(ZOS_PALLOC, cachepage);
  if status <> 0 then BackendFailure(37);
  fd := ZOS(ZOS_OPEN, RequestName, ZOS_O_RDONLY);
  if fd < 0 then BackendFailure(32);
  amount := SizeOf(request);
  status := ZOS(ZOS_READ, fd, request, amount);
  if (status <> 0) or (amount <> SizeOf(request)) then BackendFailure(31);
  status := ZOS(ZOS_CLOSE, fd);
  if status <> 0 then BackendFailure(36);
  row := request.editrow; col := request.editcol;
  SaveSheet; Calculate; BackendFailure(0)
end.
