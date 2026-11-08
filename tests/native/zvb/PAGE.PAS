program ZVBSourcePageCrossing;
uses ZVB;
var Pad: array[0..16387] of Byte;
  Context: TGfxContext;
  Descriptor: TDMADescriptor; Config: TDMADescriptorConfig;
  Z, I: Integer; Offset: Word; Mode: Byte;
procedure Require(OK: Boolean);
begin if not OK then Z := 1 div Z end;
begin
  Z := 0; Mode := Port[$9C]; GfxEnableScreen(False);
  Offset := ($3FFE - (Word(Addr(Pad)) and $3FFF)) and $3FFF;
  Require(GfxInitialize(GfxMode3208Bit, Context) = 0);
  for I := 0 to 3 do Pad[Offset + I] := 90 + I;
  Require(GfxTilesetLoadNone(Context, Pad[Offset], 4, $3FFE, 0, False) = 0);
  Config.ReadAddress := $113FFE;
  Config.WriteAddress := DMAVirtToPhys(Pad);
  Config.Length := 4; Config.Flags := DMADescLast;
  Require(DMAPrepareDescriptor(Descriptor, Config) = 0);
  Require(DMAStartTransfer(Descriptor) = 0);
  for I := 0 to 3 do Require(Pad[I] = 90 + I);
  Require(GfxTilesetLoadNone(Context, Mem[0], 4, 0, 0, False) = GfxInvalidArg);
  Port[$9C] := Mode; GfxEnableScreen(True)
end.
