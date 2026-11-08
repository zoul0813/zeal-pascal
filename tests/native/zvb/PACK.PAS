program ZVBPackedTiles;
uses ZVB;
{$I ZVBSTATE.INC}
var Context: TGfxContext; Data: array[0..1025] of Byte;
  Result: array[0..63] of Byte;
  Descriptor: TDMADescriptor; Config: TDMADescriptorConfig;
  Z, I, J: Integer; Mode: Byte; State: TEncodedState;
procedure Require(OK: Boolean);
begin if not OK then Z := 1 div Z end;
procedure ReadVRAM(Offset: Word; Count: Word);
begin
  Config.ReadAddress := $110000 + LongWord(Offset);
  Config.WriteAddress := DMAVirtToPhys(Result);
  Config.Length := Count; Config.Flags := DMADescLast;
  Require(DMAPrepareDescriptor(Descriptor, Config) = 0);
  Require(DMAStartTransfer(Descriptor) = 0)
end;
begin
  Z := 0;
  Require(SizeOf(State) = 19);
  Require(Word(Addr(State.Full)) - Word(Addr(State)) = 16);
  Require(Word(Addr(State.Error)) - Word(Addr(State)) = 18);
  Mode := Port[$9C]; GfxEnableScreen(False);
  Require(GfxInitialize(GfxMode3208Bit, Context) = 0);
  Data[0] := $1B;
  Require(GfxTilesetLoad2Bit(Context, Data, 1, 0, 16, True) = 0);
  ReadVRAM(0, 4);
  Require((Result[0] = 0) and (Result[1] = 17) and
    (Result[2] = 18) and (Result[3] = 19));
  Data[0] := $A5;
  Require(GfxTilesetLoad1Bit(Context, Data, 1, 0, 16) = 0);
  ReadVRAM(0, 8);
  Require((Result[0] = 17) and (Result[1] = 16) and
    (Result[2] = 17) and (Result[3] = 16));
  Require((Result[4] = 16) and (Result[5] = 17) and
    (Result[6] = 16) and (Result[7] = 17));
  Data[0] := $01; Data[1] := $20;
  Require(GfxTilesetLoad4Bit(Context, Data, 2, 0, 32, True) = 0);
  ReadVRAM(0, 4);
  Require((Result[0] = 0) and (Result[1] = 33) and
    (Result[2] = 34) and (Result[3] = 0));
  Data[0] := $FF; Data[1] := 7; Data[2] := $FF;
  Data[3] := 8; Data[4] := 0; Data[5] := 9;
  Require(GfxTilesetLoadRLE(Context, Data, 6, $3F80, 0, False) = 0);
  ReadVRAM($3FFF, 2); Require((Result[0] = 7) and (Result[1] = 8));
  ReadVRAM($4080, 1); Require(Result[0] = 9);
  { A long overlapping LZ match crosses the decoder's 256-byte history wrap. }
  for I := 0 to 249 do Data[I] := I mod 3 + 1;
  Data[250] := $FF; Data[251] := 2;
  Require(GfxTilesetLoadLZ(Context, Data, 252, 0, 0, False) = 0);
  J := 0;
  while J < 317 do begin
    I := 317 - J; if I > 64 then I := 64;
    ReadVRAM(Word(J), Word(I));
    for I := 0 to 63 do
      if J + I < 317 then Require(Result[I] = (J + I) mod 3 + 1);
    J := J + 64
  end;
  { Maximum-distance history survives a 256-byte flush and VRAM page crossing. }
  for I := 0 to 3 do begin
    Data[I * 65] := $7F;
    for J := 0 to 63 do Data[I * 65 + J + 1] := I * 64 + J
  end;
  Data[260] := $C0; Data[261] := 255;
  Require(GfxTilesetLoadLZ(Context, Data, 262, $3F80, 0, False) = 0);
  ReadVRAM($3FFF, 2); Require((Result[0] = 127) and (Result[1] = 128));
  ReadVRAM($4080, 4);
  Require((Result[0] = 0) and (Result[1] = 1) and (Result[2] = 2) and (Result[3] = 3));
  Data[0] := $42; Data[1] := 200; Data[2] := 201; Data[3] := 202;
  Require(GfxTilesetLoadLZ(Context, Data, 4, 0, 0, False) = 0);
  ReadVRAM(0, 3); Require((Result[0] = 200) and (Result[2] = 202));
  Data[0] := $80;
  Require(GfxTilesetLoadRLE(Context, Data, 1, 0, 0, False) = GfxInvalidArg);
  Require(GfxTilesetLoadLZ(Context, Data, 1, 0, 0, False) = GfxInvalidArg);
  Data[0] := $C0;
  Require(GfxTilesetLoadLZ(Context, Data, 1, 0, 0, False) = GfxInvalidArg);
  Data[0] := 2; Data[1] := 1;
  Require(GfxTilesetLoadRLE(Context, Data, 2, 0, 0, False) = GfxInvalidArg);
  Data[0] := $81; Data[1] := 7;
  Require(GfxTilesetLoadRLE(Context, Data, 2, 65535, 0, False) = GfxInvalidArg);
  { Exact64KB output is representable even though a Word length is not. }
  for I := 0 to 511 do begin Data[I * 2] := $FF; Data[I * 2 + 1] := 42 end;
  Require(GfxTilesetLoadRLE(Context, Data, 1024, 0, 0, False) = 0);
  ReadVRAM(65535, 1); Require(Result[0] = 42);
  Data[1024] := 0; Data[1025] := 99;
  Require(GfxTilesetLoadRLE(Context, Data, 1026, 0, 0, False) = GfxInvalidArg);
  ReadVRAM(0, 1); Require(Result[0] = 42);
  Data[0] := 0; Data[1] := 7;
  Require(GfxTilesetLoadRLE(Context, Data, 2, 65535, 0, False) = 0);
  ReadVRAM(65535, 1); Require(Result[0] = 7);
  Require(GfxInitialize(GfxMode3204Bit, Context) = 0);
  Require(GfxTilesetLoad4Bit(Context, Data, 2, 0, 0, False) = GfxInvalidArg);
  Data[0] := $A5;
  Require(GfxTilesetLoad1Bit(Context, Data, 1, $3FFF, 14) = 0);
  ReadVRAM($3FFF, 4);
  Require((Result[0] = $FE) and (Result[1] = $FE) and
    (Result[2] = $EF) and (Result[3] = $EF));
  Data[0] := $1B;
  Require(GfxTilesetLoad2Bit(Context, Data, 1, $3FFF, 31, True) = 0);
  ReadVRAM($3FFF, 2); Require((Result[0] = $F0) and (Result[1] = $12));
  Port[$9C] := Mode; GfxEnableScreen(True)
end.
