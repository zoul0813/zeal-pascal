{ SPDX-FileCopyrightText: 2024-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
  SPDX-License-Identifier: Apache-2.0
  Pascal rewrite; see ZVB-NOTICE.txt for source and changes. }
{ Software tileset decoders. All decoding happens with the OS mapped.
  VRAMTransfer borrows page zero only during a bounded, guarded copy.
  Malformed streams return GfxInvalidArg; prior complete chunks may be visible. }
function LoadPacked(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean;
  Bits: Byte): TGfxError;
var Pixels: array[0..255] of Byte;
  Address, I, Used, Destination, Limit: Word;
  Shift, Expansion, Mask: Integer;
  Value, Pixel, Error: Byte; Half: Boolean;

procedure Flush;
begin
  if Context.BitsPerPixel = 4 then
    Error := GfxTilesetLoadNone(Context, Pixels, Used, Destination, 0, False)
  else Error := GfxTilesetLoadNone(Context, Pixels, Used, Destination,
    PaletteOffset, Opacity);
  Destination := Destination + Used;
  Used := 0
end;

begin
  LoadPacked := GfxInvalidArg;
  if not GfxContextValid(Context) or
    (Size = 0) or not GfxBufferValid(Tileset, Size) then Exit;
  if Bits = 1 then Expansion := 3
  else if Bits = 2 then Expansion := 2
  else if Bits = 4 then Expansion := 1
  else Exit;
  if Context.BitsPerPixel = 4 then begin
    if Bits = 4 then Exit;
    Expansion := Expansion - 1
  end;
  if FromByte = 0 then Limit := Word(1) shl (16 - Expansion)
  else Limit := Word(0 - FromByte) shr Expansion;
  if Size > Limit then Exit;
  Mask := (1 shl Bits) - 1;
  Address := Word(Addr(Tileset));
  I := 0; Used := 0; Destination := FromByte; Error := 0; Half := False;
  while (I < Size) and (Error = 0) do begin
    Value := Mem[Address + I];
    Shift := 8 - Bits;
    while (Shift >= 0) and (Error = 0) do begin
      Pixel := (Value shr Shift) and Mask;
      if Context.BitsPerPixel = 4 then begin
        Pixel := (Pixel + PaletteOffset) and 15;
        if Half then begin
          Pixels[Used] := (Pixels[Used] shl 4) or Pixel;
          Used := Used + 1;
          if Used = 256 then Flush
        end else Pixels[Used] := Pixel;
        Half := not Half
      end else begin
        Pixels[Used] := Pixel; Used := Used + 1;
        if Used = 256 then Flush
      end;
      Shift := Shift - Bits
    end;
    I := I + 1
  end;
  if (Used <> 0) and (Error = 0) then Flush;
  LoadPacked := Error
end;

function LoadEncoded(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean;
  Kind: Byte): TGfxError;
var Pixels: array[0..255] of Byte;
  Address, I, Position, Known, Remaining, Destination, Count, Offset: Word;
  Full: Boolean;
  Command, Value, Error: Byte;

procedure EmitPixel(Pixel: Byte);
begin
  if Error <> 0 then Exit;
  if Remaining = 0 then begin
    if not Full then begin Error := GfxInvalidArg; Exit end;
    Full := False;
    Remaining := 65535
  end else Remaining := Remaining - 1;
  Pixels[Position] := Pixel;
  Position := Position + 1;
  if Known < 256 then Known := Known + 1;
  if Position = 256 then begin
    Error := GfxTilesetLoadNone(Context, Pixels, 256, Destination,
      PaletteOffset, Opacity);
    Destination := Destination + 256;
    Position := 0
  end
end;

procedure Literals;
begin
  if Count > Size - I then begin Error := GfxInvalidArg; Exit end;
  while (Count <> 0) and (Error = 0) do begin
    EmitPixel(Mem[Address + I]);
    I := I + 1; Count := Count - 1
  end
end;

begin
  LoadEncoded := GfxInvalidArg;
  if not GfxContextValid(Context) or (Context.BitsPerPixel <> 8) or
    (Size = 0) or not GfxBufferValid(Tileset, Size) then Exit;
  if (Kind <> TilesetCompRLE) and (Kind <> TilesetCompLZ) then Exit;
  Address := Word(Addr(Tileset));
  I := 0; Position := 0; Known := 0; Error := 0;
  Destination := FromByte;
  Full := FromByte = 0;
  Remaining := Word(0 - FromByte);
  while (I < Size) and (Error = 0) do begin
    Command := Mem[Address + I]; I := I + 1;
    if Kind = TilesetCompRLE then begin
      if Command < $80 then begin
        Count := Command + 1; Literals
      end else begin
        Count := Command - $80 + 1;
        if I = Size then Error := GfxInvalidArg
        else begin
          Value := Mem[Address + I]; I := I + 1;
          while (Count <> 0) and (Error = 0) do begin
            EmitPixel(Value); Count := Count - 1
          end
        end
      end
    end else begin
      if Command < $40 then EmitPixel(Command)
      else if Command < $80 then begin
        Count := (Command and $3F) + 1; Literals
      end else begin
        if Command < $C0 then begin
          Count := ((Command shr 4) and 3) + 3;
          Offset := (Command and 15) + 1
        end else begin
          Count := (Command and $3F) + 4;
          if I = Size then begin Error := GfxInvalidArg; Offset := 0 end
          else begin Offset := Mem[Address + I] + 1; I := I + 1 end
        end;
        if Offset > Known then Error := GfxInvalidArg;
        while (Count <> 0) and (Error = 0) do begin
          EmitPixel(Pixels[(Position + 256 - Offset) and 255]);
          Count := Count - 1
        end
      end
    end
  end;
  if (Position <> 0) and (Error = 0) then
    Error := GfxTilesetLoadNone(Context, Pixels, Position, Destination,
      PaletteOffset, Opacity);
  LoadEncoded := Error
end;

function GfxTilesetLoad1Bit(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte): TGfxError;
begin
  GfxTilesetLoad1Bit := LoadPacked(Context, Tileset, Size, FromByte,
    PaletteOffset, False, 1)
end;

function GfxTilesetLoad2Bit(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean): TGfxError;
begin
  GfxTilesetLoad2Bit := LoadPacked(Context, Tileset, Size, FromByte,
    PaletteOffset, Opacity, 2)
end;

function GfxTilesetLoad4Bit(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean): TGfxError;
begin
  GfxTilesetLoad4Bit := LoadPacked(Context, Tileset, Size, FromByte,
    PaletteOffset, Opacity, 4)
end;

function GfxTilesetLoadRLE(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean): TGfxError;
begin
  GfxTilesetLoadRLE := LoadEncoded(Context, Tileset, Size, FromByte,
    PaletteOffset, Opacity, TilesetCompRLE)
end;

function GfxTilesetLoadLZ(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean): TGfxError;
begin
  GfxTilesetLoadLZ := LoadEncoded(Context, Tileset, Size, FromByte,
    PaletteOffset, Opacity, TilesetCompLZ)
end;

function GfxTilesetLoad(var Context: TGfxContext; var Tileset;
  Size: Word; Options: PGfxTilesetOptions): TGfxError;
begin
  if Options = Nil then
    GfxTilesetLoad := GfxTilesetLoadNone(Context, Tileset, Size, 0, 0, False)
  else if Options^.Compression = TilesetCompNone then
    GfxTilesetLoad := GfxTilesetLoadNone(Context, Tileset, Size,
      Options^.FromByte, Options^.PaletteOffset, Options^.Opacity)
  else if Options^.Compression = TilesetComp1Bit then
    GfxTilesetLoad := GfxTilesetLoad1Bit(Context, Tileset, Size,
      Options^.FromByte, Options^.PaletteOffset)
  else if Options^.Compression = TilesetComp2Bit then
    GfxTilesetLoad := GfxTilesetLoad2Bit(Context, Tileset, Size,
      Options^.FromByte, Options^.PaletteOffset, Options^.Opacity)
  else if Options^.Compression = TilesetComp4Bit then
    GfxTilesetLoad := GfxTilesetLoad4Bit(Context, Tileset, Size,
      Options^.FromByte, Options^.PaletteOffset, Options^.Opacity)
  else if Options^.Compression = TilesetCompRLE then
    GfxTilesetLoad := GfxTilesetLoadRLE(Context, Tileset, Size,
      Options^.FromByte, Options^.PaletteOffset, Options^.Opacity)
  else if Options^.Compression = TilesetCompLZ then
    GfxTilesetLoad := GfxTilesetLoadLZ(Context, Tileset, Size,
      Options^.FromByte, Options^.PaletteOffset, Options^.Opacity)
  else GfxTilesetLoad := GfxInvalidArg
end;
