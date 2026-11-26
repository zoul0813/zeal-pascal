program SecondsPrompt;

const
  SecondsPerMinute = 60;
  MaxRows = 20;

type
  InputLine = string[3];

var
  Line: InputLine;
  Rows: Integer;

function InSeconds(Minutes: Integer): Integer;
begin
  InSeconds := Minutes * SecondsPerMinute
end;

procedure PrintTable(LastMinute: Integer);
var
  Minute: Integer;
begin
  WriteLn('Minutes  Seconds');
  for Minute := 1 to LastMinute do
    WriteLn(Minute:7, InSeconds(Minute):9)
end;

function ValidRows(Text: InputLine; var Count: Integer): Boolean;
var
  Value, Code: Integer;
begin
  ValidRows := False;
  if (Length(Text) > 0) and (Length(Text) <= 2) then
    if (Text[1] >= '0') and (Text[1] <= '9') then
    begin
      Val(Text, Value, Code);
      if Code = 0 then
        if (Value >= 1) and (Value <= MaxRows) then
        begin
          Count := Value;
          ValidRows := True
        end
    end
end;

begin
  repeat
    Write('How many rows (1..', MaxRows, ', or q)? ');
    ReadLn(Line);
    if Line <> 'q' then
      if ValidRows(Line, Rows) then
        PrintTable(Rows)
      else
        WriteLn('Enter a whole number from 1 to ', MaxRows, '.')
  until Line = 'q'
end.
