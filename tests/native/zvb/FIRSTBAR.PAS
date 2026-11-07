program FirstBar;
uses ZVB, Crt;
var Context: TGfxContext;
  Palette: array[0..1] of Word;
  Row: array[0..19] of Byte;
  I, Y: Integer; Error: TGfxError; Key: Char;
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

begin
  VideoSaved := False;
  CrtInit;
  if SaveVideo then begin
    GfxEnableScreen(False);
    Error := GfxInitialize(GfxMode3208Bit, Context);
    Palette[0] := RGB888ToRGB565(0, 0, 64);
    Palette[1] := RGB888ToRGB565(255, 160, 0);
    if Error = GfxSuccess then
      Error := GfxPaletteLoad(Context, Palette, SizeOf(Palette), 0);
    if Error = GfxSuccess then
      Error := GfxTilesetAddColorTile(Context, 1, 1);
    for I := 0 to 127 do
      if Error = GfxSuccess then Error := GfxSpriteSetXY(Context, I, 0, 0);
    for I := 0 to 7 do Port[$94 + I] := 0;      { Neutral scrolling. }
    for I := 0 to 19 do Row[I] := 0;
    for Y := 0 to 14 do                         { Empty the visible layer 1. }
      if Error = GfxSuccess then
        Error := GfxTilemapLoad(Context, Row, 20, 1, 0, Y);
    for I := 0 to 19 do Row[I] := 1;
    if Error = GfxSuccess then
      Error := GfxTilemapLoad(Context, Row, 20, 0, 0, 7);
    if Error = GfxSuccess then begin
      GfxEnableScreen(True);
      if ParamCount = 0 then Key := ReadKey
    end
  end;
  RestoreVideo(Context);
  CrtExit
end.
