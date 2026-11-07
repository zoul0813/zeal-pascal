program ZVBCRC;
uses ZVB;
const Digits: array[1..9] of Char = '123456789';
var Value: LongWord; Z: Integer;
begin
  Z := 0;
  CRCInitialize(True);
  Value := CRCUpdate(Digits, 0);
  if Value <> 0 then Z := 1 div Z;
  Value := CRCUpdate(Digits, 4);
  SoundInitialize(False);
  Value := CRCUpdate(Digits[5], 5);
  if Value <> $CBF43926 then Z := 1 div Z;
  CRCReset;
  Value := CRCUpdate(Digits, 9);
  if Value <> $CBF43926 then Z := 1 div Z;
  CRCInitialize(False);
  Value := CRCUpdate(Digits, 0);
  if Value <> $CBF43926 then Z := 1 div Z
end.
