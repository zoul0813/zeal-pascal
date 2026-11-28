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
