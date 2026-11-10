{$R-}
{ Walk one row's validated disk history, retaining just the requested source. }
procedure LoadCell(r, c: integer);
var at: integer;
begin
  if (r < 1) or (r > MaxRows) or (c < 1) or (c > MaxCols) then Fatal(19);
  at := job.heads[r];
  while at <> 0 do
  begin
    ReadRecord(at);
    if (entry.row <> r) or (entry.previous >= at) then Fatal(20);
    if entry.col = c then Exit;
    at := entry.previous
  end;
  entry.text := ''
end;
{$R+}
