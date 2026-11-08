uses Crt, ZVB;
var Low, High: Byte;
begin
  Low := Port[ZVB_CTRL_L0_SCR_Y_LOW]; High := Port[ZVB_CTRL_L0_SCR_Y_HIGH];
  Port[ZVB_CTRL_L0_SCR_Y_LOW] := 0; Port[ZVB_CTRL_L0_SCR_Y_HIGH] := 0;
  Port[ZVB_CTRL_L0_SCR_Y_LOW] := Low; Port[ZVB_CTRL_L0_SCR_Y_HIGH] := High
end.
