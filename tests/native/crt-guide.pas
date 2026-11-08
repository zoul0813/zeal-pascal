program CrtGuideCheck;
uses Crt;
var Z: Integer; C: Char;
procedure Require(OK: Boolean);
begin if not OK then Z := 1 div Z end;
begin
  Z := 0;
  CrtExit;
  CrtInit;
  TextBackground(Blue); TextColor(Yellow);
  ClrScr;
  GotoXY(5, 7);
  Require((WhereX = 5) and (WhereY = 7));
  ClrEol; InsLine; DelLine;
  Require((WhereX = 5) and (WhereY = 7));
  LowVideo; NormVideo; HighVideo;
  Delay(0); Delay(1);
  if KeyPressed then C := Crt.ReadKey;
  CrtExit;
  WriteLn('CRT-GUIDE-OK')
end.
