{ ZEALCALC - VisiCalc-inspired native Pascal UI and disk-backed sheet worker. }
program ZealCalc;
uses Crt, ZOS;
{$R+}{$I-}{$C-}{$A+}
{$I include/common.pas}
var
  row, col, top, left, records, shownrow, showncol, showntop, shownleft: integer;
  viewdevice, fetchedrow, fetchedcol: integer;
  path: string[127]; notice: string[75]; finishkey: char;
  active, quitting, painted, ownrequest, ownview, labelcell: boolean;
  request: SheetRequest; header: SheetView; visible: ViewCell;
{$I include/front.pas}
{$I include/screen.pas}
begin
  if Port[$9C] <> 0 then
  begin WriteLn('ZealCalc requires ZVB 80x40 text mode.'); Halt end;
  active := False; viewdevice := -1; fetchedrow := 0;
  ownrequest := False; ownview := False; painted := False;
  FillChar(request, SizeOf(request), 0);
  path := ParamStr(1); if path = '' then path := 'sheet.zvc';
  notice := ''; ReserveScratch;
  CrtInit; active := True; ClrScr;
  row := 1; col := 1; top := 1; left := 1; quitting := False;
  repeat Paint; Command until quitting;
  Stop
end.
