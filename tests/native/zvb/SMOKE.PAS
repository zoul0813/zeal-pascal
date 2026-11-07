program ZVBLink;
uses ZVB;
var Z: Integer; Color: Word;
begin
  Z := 0;
  Color := RGB888ToRGB565(255, 255, 255);
  if Color <> 65535 then Z := 1 div Z;
  if (SpritePalette(31) <> 240) or (SizeOf(TGfxSprite) <> 8) or
    (SizeOf(TDMADescriptor) <> 12) or (SizeOf(TDMADescriptorConfig) <> 11) then
    Z := 1 div Z
end.
