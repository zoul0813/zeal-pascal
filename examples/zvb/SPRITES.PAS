program SpriteDemo;
uses ZVB, Crt;
var Context: TGfxContext;
  Palette: array[0..3] of Word;
  Tile: array[0..63] of Byte;
  Sprites: array[0..3] of TGfxSprite;
  I: Integer; Frame: Word; Error: Byte;
  Quit, Demo: Boolean; Key: Char;
  EmptyRow: array[0..19] of Byte; Y: Integer;

{ BEGIN GENERATED INCLUDE: VIDEO.INC }
{ Shared demo cleanup. Save palette and video controls before changing them.
  Palette read-back uses the unit's bounded VRAM transfer, before initialization. }
var SavedPalette: array[0..511] of Byte;
  SavedScroll: array[0..7] of Byte;
  SavedMode, SavedScreen, SavedDevice: Byte;
  VideoSaved, PaletteSaved: Boolean;

function SaveVideo: Boolean;
var I, Error: Byte;
begin
  SaveVideo := False;
  SavedMode := Port[$9C];
  SavedScreen := Port[$9D] and $80;
  SavedDevice := Port[$8E];
  for I := 0 to 7 do SavedScroll[I] := Port[$94 + I];
  VideoSaved := True; PaletteSaved := False;
  Error := GfxPaletteRead(SavedPalette, 512, 0);
  if Error <> GfxSuccess then Exit;
  PaletteSaved := True;
  SaveVideo := True
end;

procedure RestoreVideo(var Context: TGfxContext);
var I, Error: Byte;
begin
  if not VideoSaved then Exit;
  GfxEnableScreen(False);
  if PaletteSaved then Error := GfxPaletteLoad(Context, SavedPalette, 512, 0);
  for I := 0 to 7 do Port[$94 + I] := SavedScroll[I];
  Port[$9C] := SavedMode;
  Port[$9D] := SavedScreen;
  Port[$8E] := SavedDevice;
  if SavedMode <= GfxModeText320 then ClrScr;
  VideoSaved := False
end;
{ END GENERATED INCLUDE: VIDEO.INC }

function Initialize: Boolean;
begin
  Initialize := False;
  if not SaveVideo then Exit;
  GfxEnableScreen(False);
  Error := GfxInitialize(GfxMode3208Bit, Context);
  if Error <> GfxSuccess then Exit;
  for I := 0 to 19 do EmptyRow[I] := 0;
  for Y := 0 to 14 do begin
    Error := GfxTilemapLoad(Context, EmptyRow, 20, 1, 0, Y);
    if Error <> GfxSuccess then Exit
  end;
  Palette[0] := RGB888ToRGB565(0, 0, 0);
  Palette[1] := RGB888ToRGB565(255, 90, 40);
  Palette[2] := RGB888ToRGB565(50, 210, 255);
  Palette[3] := RGB888ToRGB565(255, 255, 255);
  Error := GfxPaletteLoad(Context, Palette, SizeOf(Palette), 0);
  if Error <> GfxSuccess then Exit;
  { An asymmetric 16x16 tile makes each flip visibly different. }
  for I := 0 to 63 do begin
    if I < 16 then Tile[I] := $1B
    else if I < 32 then Tile[I] := $55
    else if I < 48 then Tile[I] := $AA
    else Tile[I] := $FF
  end;
  Error := GfxTilesetLoad2Bit(Context, Tile, SizeOf(Tile), 256, 0, True);
  if Error <> GfxSuccess then Exit;
  for I := 0 to 3 do begin
    Sprites[I].X := 16 + I * 70;
    Sprites[I].Y := 70 + I * 25;
    Sprites[I].Tile := 1;
    Sprites[I].Flags := 0;
    Sprites[I].Options := 0
  end;
  Sprites[1].Flags := SpriteFlipX;
  Sprites[2].Flags := SpriteFlipY;
  Sprites[3].Flags := SpriteFlipX or SpriteFlipY;
  { Hide every hardware slot left by an earlier graphics program. }
  for I := 0 to 127 do Error := GfxSpriteSetXY(Context, I, 0, 0);
  Error := GfxSpriteRenderArray(Context, 0, Sprites, 4);
  for I := 0 to 7 do Port[$94 + I] := 0;
  GfxEnableScreen(True);
  Initialize := Error = GfxSuccess
end;

begin
  VideoSaved := False; Quit := False; Demo := False; Frame := 0;
  if ParamCount > 0 then Demo := ParamStr(1) = '--demo';
  CrtInit;
  if Initialize then begin
    while not Quit do begin
      Error := GfxWaitVBlank(Context);
      Error := GfxSpriteSetX(Context, 0, 16 + (Frame mod 280));
      Error := GfxSpriteSetY(Context, 1, 30 + (Frame mod 180));
      Error := GfxSpriteSetXY(Context, 2, 296 - (Frame mod 280), 100);
      Frame := Frame + 1;
      if KeyPressed then begin Key := ReadKey; Quit := True end;
      if Demo and (Frame = 180) then Quit := True;
      Error := GfxWaitEndVBlank(Context)
    end
  end;
  for I := 0 to 127 do Error := GfxSpriteSetXY(Context, I, 0, 0);
  RestoreVideo(Context);
  CrtExit;
  WriteLn('Sprites finished.')
end.
