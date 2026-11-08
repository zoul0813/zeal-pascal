{ SPDX-FileCopyrightText: 2024-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
  SPDX-License-Identifier: Apache-2.0
  Pascal rewrite; see ZVB-NOTICE.txt for source and changes. }
{ Software tileset decoders. All decoding happens with the OS mapped.
  VRAMTransfer borrows page zero only during a bounded, guarded copy.
  Malformed streams return GfxInvalidArg; prior complete chunks may be visible. }
{$I ZVBDECODE.INC}
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
  State: TEncodedState;
  Destination: Word; Error: Byte;
begin
  LoadEncoded := GfxInvalidArg;
  if not GfxContextValid(Context) or (Context.BitsPerPixel <> 8) or
    (Size = 0) or not GfxBufferValid(Tileset, Size) then Exit;
  if (Kind <> TilesetCompRLE) and (Kind <> TilesetCompLZ) then Exit;
  FillChar(State, SizeOf(State), 0);
  State.InputAddress := Word(Addr(Tileset)); State.InputLeft := Size;
  State.Remaining := Word(0 - FromByte); State.Full := FromByte = 0;
  State.Kind := Kind; Destination := FromByte; Error := 0;
  repeat
    State.Position := 0;
    DecodeEncoded(State, Pixels);
    if (State.Error = 0) and (State.Position <> 0) then begin
      Error := GfxTilesetLoadNone(Context, Pixels, State.Position,
        Destination, PaletteOffset, Opacity);
      Destination := Destination + State.Position
    end
  until State.Done or (State.Error <> 0) or (Error <> 0);
  if State.Error <> 0 then Error := State.Error;
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
