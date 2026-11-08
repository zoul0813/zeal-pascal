{ SPDX-FileCopyrightText: 2024-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
  SPDX-License-Identifier: Apache-2.0
  Pascal rewrite; see ZVB-NOTICE.txt for source and changes. }
{ Raw RAM-to-VRAM copies use LDIR within one bank. Solid fills write each
  byte directly, matching SDK memset_vram without reading back device memory.
  Cross-bank and transformed streams retain the bounded byte loop. }
{ VRAM leaves restore page zero and IFF2 without Pascal/OS calls while mapped.
  Flags: 1 transparent zero, 2 fill Add, 4 preserve companion byte selected
  by Add parity, 8 read to Buffer. Sprite pairs commit low then high. }
procedure VRAMTransfer(Page: Byte; Offset: Word; var Buffer; Count: Word;
  Add, Flags: Byte);
begin
  Inline(
    $DD/$6E/$0A/$DD/$66/$0B/$DD/$5E/$0C/$DD/$56/$0D/$DD/$4E/
    $08/$DD/$46/$09/$ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/$DD/$7E/
    $0E/$D3/$F0/$78/$B1/$28/$3F/$DD/$7E/$04/$E6/$FE/$28/$06/
    $FE/$02/$20/$34/$18/$06/$DD/$7E/$06/$B7/$20/$2C/$E5/$60/
    $69/$19/$38/$25/$7C/$FE/$40/$38/$06/$20/$1E/$7D/$B7/$20/
    $1A/$E1/$DD/$7E/$04/$E6/$02/$20/$04/$ED/$B0/$18/$5E/$DD/
    $7E/$06/$EB/$5F/$73/$23/$0B/$78/$B1/$20/$F9/$18/$50/$E1/
    $78/$B1/$28/$4B/$DD/$CB/$04/$5E/$28/$04/$1A/$77/$18/$2D/
    $DD/$CB/$04/$56/$28/$0E/$7B/$E6/$01/$DD/$BE/$06/$28/$03/
    $1A/$18/$1B/$7E/$18/$18/$DD/$CB/$04/$4E/$20/$0F/$7E/$B7/
    $20/$06/$DD/$CB/$04/$46/$20/$08/$DD/$86/$06/$18/$03/$DD/
    $7E/$06/$12/$23/$13/$0B/$7A/$FE/$40/$20/$BD/$16/$00/$DD/
    $34/$0E/$DD/$7E/$0E/$D3/$F0/$18/$B1/$F1/$D3/$F0/$C1/$CB/
    $51/$28/$01/$FB)
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
  Inline($DD/$6E/$04/$26/$00/$29/$11/$00/$0E/$19/$EB/$DD/$6E/$08/$DD/$66/
    $09/$DD/$4E/$06/$DD/$46/$07/$ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/$3E/
    $40/$D3/$F0/$ED/$B0/$F1/$D3/$F0/$D1/$CB/$53/$28/$01/$FB);
  GfxPaletteLoad := GfxSuccess
end;


function GfxPaletteRead(var Palette; Size: Word; FromColor: Byte): TGfxError;
var Offset: Word;
begin
  GfxPaletteRead := GfxInvalidArg;
  Offset := Word(FromColor) shl 1;
  if Odd(Size) or (Size = 0) or (Size > 512 - Offset) or
    not GfxBufferValid(Palette, Size) then Exit;
  Inline($DD/$6E/$04/$26/$00/$29/$11/$00/$0E/$19/$DD/$5E/$08/$DD/$56/$09/
    $DD/$4E/$06/$DD/$46/$07/$ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/$3E/$40/
    $D3/$F0/$ED/$B0/$F1/$D3/$F0/$D1/$CB/$53/$28/$01/$FB);
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
    Size := 256
  end else begin
    if Index > 511 then Exit;
    Size := 128
  end;
  Offset := Index * Size;
  Page := 68 + Byte(Offset shr 14);
  VRAMTransfer(Page, Offset and $3FFF, Context, Size, Color, 2);
  GfxTilesetAddColorTile := GfxSuccess
end;

function GfxTilemapLoad(var Context: TGfxContext; var Tiles;
  Size, Layer, X, Y: Byte): TGfxError;
begin
  GfxTilemapLoad := GfxInvalidArg;
  if not GfxContextValid(Context) or (Size = 0) or (Layer > 1) or
    (X > 79) or (Y > 39) or (Size > 80 - X) or
    not GfxBufferValid(Tiles, Size) then Exit;
  { Bounds ensure one VRAM bank; memcpy/LDIR, not the transformed loader. }
  Inline($DD/$6E/$04/$26/$00/$54/$5D/$29/$29/$19/$29/$29/$29/$29/$DD/$5E/
    $06/$16/$00/$19/$DD/$7E/$08/$B7/$28/$04/$11/$00/$10/$19/$EB/$DD/
    $6E/$0C/$DD/$66/$0D/$DD/$4E/$0A/$06/$00/$ED/$57/$F5/$F3/$AF/$DB/
    $F0/$F5/$3E/$40/$D3/$F0/$ED/$B0/$F1/$D3/$F0/$D1/$CB/$53/$28/$01/
    $FB);
  GfxTilemapLoad := GfxSuccess
end;


function GfxTilemapPlace(var Context: TGfxContext;
  Tile, Layer, X, Y: Byte): TGfxError;
begin
  GfxTilemapPlace := GfxInvalidArg;
  if not GfxContextValid(Context) or (Layer > 1) or
    (X > 79) or (Y > 39) then Exit;
  { Same direct single-byte store as the SDK; coordinates in IX+4..+8,
    tile at IX+10. No Pascal/OS call while page zero holds VRAM. }
  Inline($DD/$6E/$04/$26/$00/$54/$5D/$29/$29/$19/$29/$29/$29/$29/$DD/$5E/
    $06/$16/$00/$19/$DD/$7E/$08/$B7/$28/$04/$11/$00/$10/$19/$DD/$4E/
    $0A/$ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/$3E/$40/$D3/$F0/$71/$F1/$D3/
    $F0/$D1/$CB/$53/$28/$01/$FB);
  GfxTilemapPlace := GfxSuccess
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
  Inline($DD/$6E/$08/$26/$00/$29/$29/$29/$11/$00/$28/$19/$EB/$DD/$6E/$04/
    $26/$00/$29/$29/$29/$44/$4D/$DD/$6E/$06/$DD/$66/$07/$ED/$57/$F5/
    $F3/$AF/$DB/$F0/$F5/$3E/$40/$D3/$F0/$ED/$B0/$F1/$D3/$F0/$D1/$CB/
    $53/$28/$01/$FB);
  GfxSpriteRenderArray := GfxSuccess
end;


function GfxSpriteRender(var Context: TGfxContext; SpriteIndex: Byte;
  var Sprite: TGfxSprite): TGfxError;
begin
  GfxSpriteRender := GfxInvalidArg;
  if not GfxContextValid(Context) or (SpriteIndex >= 128) or
    not GfxBufferValid(Sprite, 8) then Exit;
  Inline($DD/$6E/$06/$26/$00/$29/$29/$29/$11/$00/$28/$19/$EB/$DD/$6E/$04/
    $DD/$66/$05/$01/$08/$00/$ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/$3E/$40/
    $D3/$F0/$ED/$B0/$F1/$D3/$F0/$D1/$CB/$53/$28/$01/$FB);
  GfxSpriteRender := GfxSuccess
end;


function GfxSpriteSetXY(var Context: TGfxContext; SpriteIndex: Byte;
  X, Y: Word): TGfxError;
begin
  GfxSpriteSetXY := GfxInvalidArg;
  if not GfxContextValid(Context) or (SpriteIndex >= 128) then Exit;
  { The SDK writes X then Y, each low byte before its high byte. }
  Inline($DD/$6E/$08/$26/$00/$29/$29/$29/$11/$02/$28/$19/$ED/$57/$F5/$F3/
    $AF/$DB/$F0/$F5/$3E/$40/$D3/$F0/$DD/$7E/$06/$77/$23/$DD/$7E/$07/
    $77/$2B/$2B/$2B/$DD/$7E/$04/$77/$23/$DD/$7E/$05/$77/$F1/$D3/$F0/
    $D1/$CB/$53/$28/$01/$FB);
  GfxSpriteSetXY := GfxSuccess
end;


function GfxSpriteSetX(var Context: TGfxContext; SpriteIndex: Byte;
  X: Word): TGfxError;
begin
  GfxSpriteSetX := GfxInvalidArg;
  if not GfxContextValid(Context) or (SpriteIndex >= 128) then Exit;
  Inline($DD/$6E/$06/$26/$00/$29/$29/$29/$11/$02/$28/$19/$DD/$5E/$04/$DD/
    $56/$05/$ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/$3E/$40/$D3/$F0/$73/$23/
    $72/$F1/$D3/$F0/$D1/$CB/$53/$28/$01/$FB);
  GfxSpriteSetX := GfxSuccess
end;


function GfxSpriteSetY(var Context: TGfxContext; SpriteIndex: Byte;
  Y: Word): TGfxError;
begin
  GfxSpriteSetY := GfxInvalidArg;
  if not GfxContextValid(Context) or (SpriteIndex >= 128) then Exit;
  Inline($DD/$6E/$06/$26/$00/$29/$29/$29/$11/$00/$28/$19/$DD/$5E/$04/$DD/
    $56/$05/$ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/$3E/$40/$D3/$F0/$73/$23/
    $72/$F1/$D3/$F0/$D1/$CB/$53/$28/$01/$FB);
  GfxSpriteSetY := GfxSuccess
end;


function GfxSpriteSetTile(var Context: TGfxContext; SpriteIndex: Byte;
  Tile: Byte): TGfxError;
begin
  GfxSpriteSetTile := GfxInvalidArg;
  if not GfxContextValid(Context) or (SpriteIndex >= 128) then Exit;
  { Preserve the companion byte and commit tile/flags low then high. }
  Inline($DD/$6E/$06/$26/$00/$29/$29/$29/$11/$04/$28/$19/$DD/$4E/$04/$ED/
    $57/$F5/$F3/$AF/$DB/$F0/$F5/$3E/$40/$D3/$F0/$71/$23/$7E/$77/$F1/
    $D3/$F0/$D1/$CB/$53/$28/$01/$FB);
  GfxSpriteSetTile := GfxSuccess
end;


function GfxSpriteSetFlags(var Context: TGfxContext; SpriteIndex: Byte;
  Flags: Byte): TGfxError;
begin
  GfxSpriteSetFlags := GfxInvalidArg;
  if not GfxContextValid(Context) or (SpriteIndex >= 128) then Exit;
  { Preserve the companion byte and commit tile/flags low then high. }
  Inline($DD/$6E/$06/$26/$00/$29/$29/$29/$11/$04/$28/$19/$ED/$57/$F5/$F3/
    $AF/$DB/$F0/$F5/$3E/$40/$D3/$F0/$7E/$77/$23/$DD/$7E/$04/$77/$F1/
    $D3/$F0/$D1/$CB/$53/$28/$01/$FB);
  GfxSpriteSetFlags := GfxSuccess
end;


{$I ZVBPACK.INC}
