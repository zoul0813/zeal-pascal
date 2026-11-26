program DeskCalc;

const Limit = 9999;
      Slots = 5;
type
  Entry = string[11];
  ResultLine = string[20];
var
  History: array[1..Slots] of ResultLine;
  Line: Entry;
  Total, Stored, Operand, Next: Real;
  Used, I: Integer;
  Ready: Boolean;

function ReadOperand(Answer: Entry; var Value: Real): Boolean;
var
  I, Code: Integer;
  Ch: Char;
  Valid, HasDigit, HasDot: Boolean;
begin
  Valid := (Length(Answer) >= 2) and (Length(Answer) <= 10);
  HasDigit := False;
  HasDot := False;
  if Valid then
    for I := 2 to Length(Answer) do
    begin
      Ch := Answer[I];
      if (Ch >= '0') and (Ch <= '9') then HasDigit := True
      else if (Ch = '.') and not HasDot then HasDot := True
      else if not ((I = 2) and (Ch = '-')) then Valid := False
    end;
  ReadOperand := False;
  if Valid and HasDigit then
  begin
    Val(Copy(Answer, 2, Length(Answer) - 1), Value, Code);
    if Code = 0 then ReadOperand := Abs(Value) <= Limit
  end
end;

procedure RememberResult;
begin
  if Used = Slots then
    for I := 1 to Slots - 1 do History[I] := History[I + 1]
  else Used := Used + 1;
  Str(Total:0:2, History[Used])
end;

begin
  Total := 0;
  Stored := 0;
  Used := 0;
  WriteLn('Use +, -, * or / followed by a number. q quits.');
  WriteLn('m stores the total, r recalls it, h shows recent results.');
  repeat
    Write('Total ', Total:0:2, '> ');
    ReadLn(Line);
    if Line = 'm' then
    begin
      Stored := Total;
      WriteLn('Stored.')
    end
    else if Line = 'r' then
    begin
      Total := Stored;
      RememberResult
    end
    else if Line = 'h' then
    begin
      if Used = 0 then WriteLn('No results yet.');
      for I := 1 to Used do WriteLn(History[I])
    end
    else if Line <> 'q' then
    begin
      Ready := ReadOperand(Line, Operand);
      if Ready then
      begin
        case Line[1] of
          '+': Next := Total + Operand;
          '-': Next := Total - Operand;
          '*': Next := Total * Operand;
          '/': if Operand = 0 then
                 Ready := False
               else Next := Total / Operand
        else
          Ready := False
        end;
        if Ready then Ready := Abs(Next) <= Limit
      end;
      if Ready then
      begin
        Total := Next;
        RememberResult
      end
      else WriteLn('Check the operation and the -9999..9999 limit.')
    end
  until Line = 'q'
end.
