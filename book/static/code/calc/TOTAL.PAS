program WholeTotal;

const Limit = 999;
type Entry = string[7];
var
  Line: Entry;
  Op: Char;
  Total, Value, Next, Code: Integer;
  Ready: Boolean;

begin
  Total := 0;
  WriteLn('Use +, - or * followed by a whole number. Type q to quit.');
  repeat
    Write('Total ', Total, '> ');
    ReadLn(Line);
    if Line <> 'q' then
    begin
      Ready := False;
      if (Length(Line) >= 2) and (Length(Line) <= 5) then
      begin
        Op := Line[1];
        if Line[2] <> '$' then
        begin
          Val(Copy(Line, 2, Length(Line) - 1), Value, Code);
          if Code = 0 then
            Ready := (Value >= -Limit) and (Value <= Limit)
        end
      end;
      if Ready then
      begin
        case Op of
          '+': Next := Total + Value;
          '-': Next := Total - Value;
          '*': if Value = 0 then Next := 0
               else if Abs(Total) <= Limit div Abs(Value) then
                 Next := Total * Value
               else Ready := False
        else
          Ready := False
        end;
        if Ready then
          Ready := (Next >= -Limit) and (Next <= Limit)
      end;
      if Ready then Total := Next
      else WriteLn('Use +, - or * and stay within -999..999.')
    end
  until Line = 'q'
end.
