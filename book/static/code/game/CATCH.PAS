program CatchTheBeacon;

uses Crt;

const LeftEdge = 12;
      RightEdge = 40;
      TopEdge = 5;
      CatchRow = 15;

var Basket, DropCol, DropRow, Score: Integer;
    Key: Char;
    Playing, Quit: Boolean;

procedure Put(X, Y: Integer; Mark: Char);
begin
  GotoXY(X, Y);
  Write(Con, Mark)
end;

procedure StartRound;
begin
  ClrScr;
  GotoXY(4, 2);
  Write(Con, 'a/d move, q quits. Catch the beacon before it lands.');
  Basket := 26;
  DropCol := 24;
  DropRow := TopEdge;
  Score := 0;
  Playing := True;
  Put(Basket, CatchRow, '=')
end;

begin
  CrtInit;
  Quit := False;
  StartRound;
  repeat
    if Playing then
    begin
      Put(DropCol, DropRow, '*');
      Delay(130);
      Put(DropCol, DropRow, ' ');
      if KeyPressed then
      begin
        Key := ReadKey;
        Put(Basket, CatchRow, ' ');
        if (Key = 'a') and (Basket > LeftEdge) then Basket := Basket - 1;
        if (Key = 'd') and (Basket < RightEdge) then Basket := Basket + 1;
        if Key = 'q' then Quit := True;
        Put(Basket, CatchRow, '=')
      end;
      if DropRow = CatchRow - 1 then
      begin
        if Basket = DropCol then
        begin
          Score := Score + 1;
          GotoXY(4, 4);
          Write(Con, 'Score ', Score:2);
          if Score = 9 then Playing := False
          else
          begin
            DropCol := DropCol + 2;
            DropRow := TopEdge
          end
        end
        else Playing := False
      end
      else DropRow := DropRow + 1
    end
    else
    begin
      GotoXY(4, 18);
      if Score = 9 then Write(Con, 'You caught all nine. ')
      else Write(Con, 'Missed. Score ', Score, '. ');
      Write(Con, 'r starts over, q quits.');
      repeat Key := ReadKey until (Key = 'q') or (Key = 'r');
      if Key = 'q' then Quit := True
      else StartRound
    end
  until Quit;
  CrtExit;
  GotoXY(1, 21);
  WriteLn(Con, 'Game closed.')
end.
