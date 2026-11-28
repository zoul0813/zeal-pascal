program TextPanel;

uses Crt;

var Choice: Integer;
    Key: Char;

procedure DrawPanel;
begin
  TextColor(White);
  GotoXY(6, 4);
  Write(Con, 'BEACON STATUS');
  GotoXY(6, 7);
  if Choice = 1 then TextColor(Yellow) else TextColor(White);
  Write(Con, '1  Workshop');
  GotoXY(6, 8);
  if Choice = 2 then TextColor(Yellow) else TextColor(White);
  Write(Con, '2  Roof    ');
  TextColor(White);
  GotoXY(6, 11);
  Write(Con, 'Press 1 or 2 to select. q returns.')
end;

begin
  CrtInit;
  TextBackground(Blue);
  ClrScr;
  Choice := 1;
  Key := #0;
  DrawPanel;
  repeat
    if KeyPressed then
    begin
      Key := ReadKey;
      if Key = '1' then Choice := 1;
      if Key = '2' then Choice := 2;
      if (Key = '1') or (Key = '2') then DrawPanel
    end;
    Delay(20)
  until Key = 'q';
  CrtExit;
  GotoXY(1, 14);
  WriteLn(Con, 'Panel closed.')
end.
