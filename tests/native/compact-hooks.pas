var OldOut, OldStatus, Total, Z: Integer;
procedure Capture(C: Char);
begin Total := Total + Ord(C) end;
function Ready: Boolean;
begin Ready := True end;
begin
  Z := 0; Total := 0;
  OldOut := ConOutPtr; OldStatus := ConStPtr;
  ConOutPtr := Addr(Capture); ConStPtr := Addr(Ready);
  if not KeyPressed then Total := 1 div Z;
  Write('A');
  if Total <> 65 then Total := 1 div Z;
  ConOutPtr := OldOut; ConStPtr := OldStatus
end.
