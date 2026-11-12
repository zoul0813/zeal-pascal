{ ZealCalc: demand-loaded code overlays with one bounded RAM data bank. }
program ZealCalc;
uses Crt, ZOS;
{$R+}{$I-}{$C-}{$A+}
{$I include/common.pas}
{$I include/backend.pas}
{$I include/types.pas}
var
  row, col, top, left, shownrow, showncol, showntop, shownleft: byte;
  records, viewdevice, fetchedrow, fetchedcol: integer;
  notice: string[75]; finishkey: char;
  active, quitting, painted, ownview, labelcell: boolean;
  request: SheetRequest; job: CalcRequest; entry: DiskCell;
  device, outputdevice: integer; cachepage: byte;
  screenstate: array[0..8] of byte;
procedure Stop; forward;
procedure Fatal(message: CellText); forward;
procedure BackendFailure(code: integer); forward;
procedure CacheCopy(offset: word; var buffer; size: word; reading: boolean); forward;
{$I include/save.pas}
{$I include/calculate.pas}
procedure BackendFailure;
begin
  Str(code, notice);
  notice := 'Sheet failed (' + notice + '); last edit may already be saved.';
  Stop
end;
{$I include/view.pas}
{$I include/line.pas}
procedure QueueEdit;
begin
  request.edit := 1; request.editrow := row; request.editcol := col
end;
{$I include/screen.pas}
procedure Fatal;
begin notice := message; Stop end;
{$I include/stop.pas}
{$I include/cache.pas}
overlay procedure Start;
var i, status: integer; zero: byte;
begin
  device := -1; outputdevice := -1; cachepage := 255;
  active := False; viewdevice := -1; fetchedrow := 0;
  ownview := False; painted := False; notice := '';
  if request.path = '' then request.path := 'sheet.zvc';
  if Port[$9C] <> 0 then Fatal('ZealCalc requires ZVB 80x40 text mode.');
  status := ZOS(ZOS_PALLOC, cachepage);
  if status <> 0 then BackendFailure(37);
  zero := 0;
  for i := 0 to 209 do CacheCopy(i * 10, zero, 1, False);
  for i := 0 to 5 do CacheCopy(2100 + i * 80, zero, 1, False);
  View(2, 0, 0);
  Port[$8E] := 0;
  for i := 1 to 8 do screenstate[i] := Port[$A0 + i];
  screenstate[0] := Port[$A9] and $38;
  Port[$A5] := $0F; Port[$A8] := $F0; active := True;
  Port[$8E] := 0; Port[$A3] := 0; Port[$A4] := 0;
  Port[$A1] := 0; Port[$A2] := 0;
  for i := 1 to 3200 do Port[$A0] := 32;
  Port[$A1] := 0; Port[$A2] := 0;
  row := 1; col := 1; top := 1; left := 1; quitting := False;
  repeat Paint; Command until quitting;
  Stop
end;
begin
  FillChar(request, SizeOf(request), 0); request.path := ParamStr(1);
  OvrDrive(0); Start
end.
