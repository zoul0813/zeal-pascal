program MapView;

uses ZVB, Crt;

var Context: TGfxContext;
    Palette: array[0..2] of Word;
    Row: array[0..19] of Byte;
    X, Y, Index: Integer;
    Error: TGfxError;
    Key: Char;

{$I H:/book/graphics/VIDEO.INC}

procedure DrawMap;
begin
  for Y := 0 to 14 do
  begin
    for X := 0 to 19 do Row[X] := 0;
    if Error = GfxSuccess then Error := GfxTilemapLoad(Context, Row, 20, 1, 0, Y);
    if (Y >= 3) and (Y <= 11) then
      for X := 3 to 16 do Row[X] := 1;
    if (Y = 5) or (Y = 9) then Row[10] := 0;
    if Y = 7 then Row[10] := 2;
    if Error = GfxSuccess then Error := GfxTilemapLoad(Context, Row, 20, 0, 0, Y)
  end
end;

begin
  VideoSaved := False;
  CrtInit;
  if SaveVideo then
  begin
    GfxEnableScreen(False);
    Error := GfxInitialize(GfxMode3208Bit, Context);
    Palette[0] := RGB888ToRGB565(8, 16, 40);
    Palette[1] := RGB888ToRGB565(35, 130, 150);
    Palette[2] := RGB888ToRGB565(255, 190, 60);
    if Error = GfxSuccess then Error := GfxPaletteLoad(Context, Palette, SizeOf(Palette), 0);
    if Error = GfxSuccess then Error := GfxTilesetAddColorTile(Context, 0, 0);
    if Error = GfxSuccess then Error := GfxTilesetAddColorTile(Context, 1, 1);
    if Error = GfxSuccess then Error := GfxTilesetAddColorTile(Context, 2, 2);
    for Index := 0 to 127 do
      if Error = GfxSuccess then Error := GfxSpriteSetXY(Context, Index, 0, 0);
    for Index := 0 to 7 do Port[$94 + Index] := 0;
    DrawMap;
    if Error = GfxSuccess then
    begin
      GfxEnableScreen(True);
      Key := ReadKey
    end
  end
  else Error := 255;
  RestoreVideo(Context);
  CrtExit;
  if Error <> GfxSuccess then WriteLn('Graphics setup error ', Error)
  else WriteLn('Map closed.')
end.
