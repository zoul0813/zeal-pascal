program SecondsTable;

const
  SecondsPerMinute = 60;
  LastMinute = 5;

var
  Minute: Integer;

function InSeconds(Minutes: Integer): Integer;
begin
  InSeconds := Minutes * SecondsPerMinute
end;

begin
  WriteLn('Minutes  Seconds');
  for Minute := 1 to LastMinute do
    WriteLn(Minute:7, InSeconds(Minute):9)
end.
