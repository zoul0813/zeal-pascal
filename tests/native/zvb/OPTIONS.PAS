program ZVBOptions;
uses ZVB;
var Context: TGfxContext; Data: array[0..1] of Byte;
  Options: TGfxTilesetOptions;
  Z: Integer; Mode: Byte;
procedure Require(OK: Boolean);
begin if not OK then Z := 1 div Z end;
begin
  Z := 0; Mode := Port[$9C]; GfxEnableScreen(False);
  Require(GfxInitialize(GfxMode3208Bit, Context) = 0);
  Data[0] := $12; Data[1] := $34;
  Options.Compression := TilesetCompNone; Options.FromByte := 256;
  Options.PaletteOffset := 0; Options.Opacity := False;
  Require(GfxTilesetLoad(Context, Data, 2, Ptr(Addr(Options))) = 0);
  Options.Compression := 255;
  Require(GfxTilesetLoad(Context, Data, 2, Ptr(Addr(Options))) = GfxInvalidArg);
  Require(GfxTilesetLoad(Context, Data, 2, Nil) = 0);
  Port[$9C] := Mode; GfxEnableScreen(True)
end.
