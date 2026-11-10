{$R-}
function Checksum: word;
var i: integer; total: word;
begin
  total := entry.row + entry.col + entry.previous + Length(entry.text);
  for i := 1 to Length(entry.text) do total := total + Ord(entry.text[i]);
  Checksum := total
end;

{$R+}
