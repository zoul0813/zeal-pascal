uses ZVB, Crt;
var Low, High: Byte;
begin
  Low := Port[$94]; High := Port[$95];
  Port[$94] := 0; Port[$95] := 0;
  Port[$94] := Low; Port[$95] := High
end.
