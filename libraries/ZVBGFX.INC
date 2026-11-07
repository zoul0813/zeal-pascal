{ SPDX-FileCopyrightText: 2024-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
  SPDX-License-Identifier: Apache-2.0
  Pascal rewrite; see ZVB-NOTICE.txt for source and changes. }
{ Page-zero VRAM access is confined to one raw instruction stream. No
  compiler guard, Pascal call or OS call can execute while the OS is unmapped.
  The original page zero and IFF2 are restored on every path. Flags bit0 keeps
  pixel zero transparent; bit1 fills with Add instead of reading Buffer.
  Bit2 updates only the byte whose address parity equals Add, preserving its
  companion directly in VRAM. Sprite pairs still commit low byte then high.
  Bit3 reads VRAM into Buffer instead of writing. }
procedure VRAMTransfer(Page: Byte; Offset: Word; var Buffer; Count: Word;
  Add, Flags: Byte);
begin
  Inline(
    $DD/$6E/$0A/$DD/$66/$0B/$DD/$5E/$0C/$DD/$56/$0D/$DD/
    $4E/$08/$DD/$46/$09/$ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/
    $DD/$7E/$0E/$D3/$F0/$78/$B1/$28/$4B/$DD/$CB/$04/$5E/
    $28/$04/$1A/$77/$18/$2D/$DD/$CB/$04/$56/$28/$0E/$7B/
    $E6/$01/$DD/$BE/$06/$28/$03/$1A/$18/$1B/$7E/$18/$18/
    $DD/$CB/$04/$4E/$20/$0F/$7E/$B7/$20/$06/$DD/$CB/$04/
    $46/$20/$08/$DD/$86/$06/$18/$03/$DD/$7E/$06/$12/$23/
    $13/$0B/$7A/$FE/$40/$20/$BD/$16/$00/$DD/$34/$0E/$DD/
    $7E/$0E/$D3/$F0/$18/$B1/$F1/$D3/$F0/$C1/$CB/$51/$28/
    $01/$FB)
end;

function GfxContextValid(var Context: TGfxContext): Boolean;
begin
  GfxContextValid := (Context.VideoMode >= 2) and (Context.VideoMode <= 7)
    and ((Context.BitsPerPixel = 8) or (Context.BitsPerPixel = 4))
end;

function GfxBufferValid(var Buffer; Size: Word): Boolean;
var Address: Word;
begin
  Address := Word(Addr(Buffer));
  GfxBufferValid := (Address >= 16384) and (Size <= Word(0 - Address))
end;

function RGB888ToRGB565(R, G, B: Byte): Word;
begin
  RGB888ToRGB565 := (Word(R shr 3) shl 11) or
    (Word(G shr 2) shl 5) or (B shr 3)
end;

function SpritePalette(Palette: Byte): Byte;
begin
  SpritePalette := (Palette and 15) shl 4
end;

function GfxInitialize(Mode: Byte; var Context: TGfxContext): TGfxError;
begin
  GfxInitialize := GfxInvalidArg;
  if (Mode < 2) or (Mode > 7) then Exit;
  Context.VideoMode := Mode;
  if (Mode and 2) <> 0 then Context.BitsPerPixel := 4
  else Context.BitsPerPixel := 8;
  Context.AlphaIndex := 0;
  Context.VirtualPage := 0;
  Context.BackupPage := Port[$F0];
  VRAMTransfer(68, 0, Context, 256, 0, 2);
  VRAMTransfer(64, 0, Context, 3200, 0, 2);
  VRAMTransfer(64, $E00, Context, 2, 0, 2);
  Port[$9C] := Mode;
  GfxInitialize := GfxSuccess
end;

procedure GfxEnableScreen(Enabled: Boolean);
begin
  if Enabled then Port[$9D] := $80 else Port[$9D] := 0
end;

function GfxPaletteLoad(var Context: TGfxContext; var Palette;
  Size: Word; FromColor: Byte): TGfxError;
var Offset: Word;
begin
  GfxPaletteLoad := GfxInvalidArg;
  Offset := Word(FromColor) shl 1;
  if not GfxContextValid(Context) or Odd(Size) or (Size = 0) or
    (Size > 512 - Offset) or not GfxBufferValid(Palette, Size) then Exit;
  VRAMTransfer(64, $E00 + Offset, Palette, Size, 0, 0);
  GfxPaletteLoad := GfxSuccess
end;

{ Pascal extension: snapshot a palette before graphics initialization changes it. }
function GfxPaletteRead(var Palette; Size: Word; FromColor: Byte): TGfxError;
var Offset: Word;
begin
  GfxPaletteRead := GfxInvalidArg;
  Offset := Word(FromColor) shl 1;
  if Odd(Size) or (Size = 0) or (Size > 512 - Offset) or
    not GfxBufferValid(Palette, Size) then Exit;
  VRAMTransfer(64, $E00 + Offset, Palette, Size, 0, 8);
  GfxPaletteRead := GfxSuccess
end;

function GfxTilesetLoadNone(var Context: TGfxContext; var Tileset;
  Size, FromByte: Word; PaletteOffset: Byte; Opacity: Boolean): TGfxError;
var TransferFlags: Byte;
begin
  GfxTilesetLoadNone := GfxInvalidArg;
  if not GfxContextValid(Context) or (Size = 0) or
    not GfxBufferValid(Tileset, Size) then Exit;
  if (FromByte <> 0) and (Size > Word(0 - FromByte)) then Exit;
  if Opacity then TransferFlags := 1 else TransferFlags := 0;
  VRAMTransfer(68 + Byte(FromByte shr 14), FromByte and $3FFF,
    Tileset, Size, PaletteOffset, TransferFlags);
  GfxTilesetLoadNone := GfxSuccess
end;

function GfxTilesetAddColorTile(var Context: TGfxContext;
  Index: Word; Color: Byte): TGfxError;
var Page: Byte; Offset, Size: Word;
begin
  GfxTilesetAddColorTile := GfxInvalidArg;
  if not GfxContextValid(Context) then Exit;
  if Context.BitsPerPixel = 8 then begin
    if Index > 255 then Exit;
    Page := 68 + Byte(Index shr 6);
    Offset := (Index and 63) shl 8;
    Size := 256
  end else begin
    if Index > 511 then Exit;
    Page := 68 + Byte(Index shr 7);
    Offset := (Index and 127) shl 7;
    Size := 128
  end;
  VRAMTransfer(Page, Offset, Context, Size, Color, 2);
  GfxTilesetAddColorTile := GfxSuccess
end;

function GfxTilemapLoad(var Context: TGfxContext; var Tiles;
  Size, Layer, X, Y: Byte): TGfxError;
var Offset: Word;
begin
  GfxTilemapLoad := GfxInvalidArg;
  if not GfxContextValid(Context) or (Size = 0) or (Layer > 1) or
    (X > 79) or (Y > 39) or (Size > 80 - X) or
    not GfxBufferValid(Tiles, Size) then Exit;
  Offset := Word(Y) * 80 + X;
  if Layer <> 0 then Offset := Offset + $1000;
  VRAMTransfer(64, Offset, Tiles, Size, 0, 0);
  GfxTilemapLoad := GfxSuccess
end;

function GfxTilemapPlace(var Context: TGfxContext;
  Tile, Layer, X, Y: Byte): TGfxError;
begin
  GfxTilemapPlace := GfxTilemapLoad(Context, Tile, 1, Layer, X, Y)
end;

function GfxWaitVBlank(var Context: TGfxContext): TGfxError;
begin
  GfxWaitVBlank := GfxInvalidArg;
  if not GfxContextValid(Context) then Exit;
  while (Port[$9D] and 2) = 0 do begin end;
  GfxWaitVBlank := GfxSuccess
end;

function GfxWaitEndVBlank(var Context: TGfxContext): TGfxError;
begin
  GfxWaitEndVBlank := GfxInvalidArg;
  if not GfxContextValid(Context) then Exit;
  while (Port[$9D] and 2) <> 0 do begin end;
  GfxWaitEndVBlank := GfxSuccess
end;

function GfxSpriteRenderArray(var Context: TGfxContext; FromIndex: Byte;
  var Sprites; Count: Byte): TGfxError;
var Size: Word;
begin
  GfxSpriteRenderArray := GfxInvalidArg;
  Size := Word(Count) shl 3;
  if not GfxContextValid(Context) or (Count = 0) or
    (FromIndex + Count > 128) or not GfxBufferValid(Sprites, Size) then Exit;
  VRAMTransfer(64, $2800 + (Word(FromIndex) shl 3), Sprites, Size, 0, 0);
  GfxSpriteRenderArray := GfxSuccess
end;

function GfxSpriteRender(var Context: TGfxContext; SpriteIndex: Byte;
  var Sprite: TGfxSprite): TGfxError;
begin
  GfxSpriteRender := GfxSpriteRenderArray(Context, SpriteIndex, Sprite, 1)
end;

function SpriteField(var Context: TGfxContext; SpriteIndex, Offset: Byte;
  var Value; Size: Byte): TGfxError;
begin
  SpriteField := GfxInvalidArg;
  if not GfxContextValid(Context) or (SpriteIndex >= 128) then Exit;
  VRAMTransfer(64, $2800 + (Word(SpriteIndex) shl 3) + Offset,
    Value, Size, 0, 0);
  SpriteField := GfxSuccess
end;

function GfxSpriteSetXY(var Context: TGfxContext; SpriteIndex: Byte;
  X, Y: Word): TGfxError;
var Pair: record Y, X: Word end;
begin
  Pair.Y := Y; Pair.X := X;
  GfxSpriteSetXY := SpriteField(Context, SpriteIndex, 0, Pair, 4)
end;

function GfxSpriteSetX(var Context: TGfxContext; SpriteIndex: Byte;
  X: Word): TGfxError;
begin
  GfxSpriteSetX := SpriteField(Context, SpriteIndex, 2, X, 2)
end;

function GfxSpriteSetY(var Context: TGfxContext; SpriteIndex: Byte;
  Y: Word): TGfxError;
begin
  GfxSpriteSetY := SpriteField(Context, SpriteIndex, 0, Y, 2)
end;

function GfxSpriteSetTile(var Context: TGfxContext; SpriteIndex: Byte;
  Tile: Byte): TGfxError;
var Pair: array[0..1] of Byte;
begin
  GfxSpriteSetTile := GfxInvalidArg;
  if not GfxContextValid(Context) or (SpriteIndex >= 128) then Exit;
  Pair[0] := Tile;
  VRAMTransfer(64, $2804 + (Word(SpriteIndex) shl 3), Pair, 2, 0, 4);
  GfxSpriteSetTile := GfxSuccess
end;

function GfxSpriteSetFlags(var Context: TGfxContext; SpriteIndex: Byte;
  Flags: Byte): TGfxError;
var Pair: array[0..1] of Byte;
begin
  GfxSpriteSetFlags := GfxInvalidArg;
  if not GfxContextValid(Context) or (SpriteIndex >= 128) then Exit;
  Pair[1] := Flags;
  VRAMTransfer(64, $2804 + (Word(SpriteIndex) shl 3), Pair, 2, 1, 4);
  GfxSpriteSetFlags := GfxSuccess
end;

{$I ZVBPACK.INC}
