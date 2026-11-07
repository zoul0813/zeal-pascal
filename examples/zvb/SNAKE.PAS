program Snake;
uses ZVB, Crt;
{ BEGIN GENERATED INCLUDE: SNAKETIL.INC }
{ Original SDK pixels, LZ encoded. See CREDITS.md and assets.json. }
const SnakeTiles: array[0..609] of Byte = (
  0,144,8,160,170,174,7,160,196,16,195,14,196,16,197,14,195,33,250,15,8,199,78,7,
  7,8,142,6,6,196,111,153,8,15,6,6,15,194,143,169,8,14,15,15,14,194,159,169,
  8,7,14,195,14,7,153,8,200,158,194,194,218,175,0,0,0,8,144,135,195,2,8,7,
  14,15,7,197,15,8,158,6,6,7,196,36,8,7,191,160,8,175,192,47,195,0,8,175,
  199,2,8,255,15,205,79,203,111,202,143,203,175,204,207,194,194,198,223,0,0,8,199,0,
  0,128,8,7,197,0,159,199,14,192,16,8,7,14,14,196,32,137,142,15,15,195,16,153,
  8,7,15,6,6,15,194,65,153,8,7,7,6,6,196,80,137,8,8,202,79,8,205,111,
  253,15,198,192,190,197,209,196,14,196,242,193,44,0,195,0,8,7,7,7,8,196,12,175,
  15,14,195,16,8,144,7,6,6,192,16,8,194,41,7,7,191,7,8,196,14,7,192,47,
  7,7,8,197,14,194,37,255,15,202,79,204,111,205,143,205,175,205,207,198,212,0,0,0,
  0,0,8,7,197,0,8,0,201,14,159,200,14,236,15,207,78,204,109,215,15,193,30,198,
  157,190,196,173,8,195,14,194,189,197,13,0,0,8,144,197,13,222,8,0,0,8,7,197,
  0,8,0,128,199,15,193,16,200,15,192,33,220,15,217,64,204,97,209,15,199,146,191,198,
  163,195,15,0,8,195,181,196,15,196,17,198,15,192,35,129,199,15,209,10,0,209,0,8,
  128,200,13,7,128,197,17,173,193,17,202,14,195,16,255,15,255,79,228,143,0,228,0,3,
  0,9,9,201,15,199,14,5,128,4,3,201,14,192,18,204,15,195,16,197,14,243,15,204,
  79,205,111,195,128,225,218,0,230,0,8,144,199,13,7,128,196,15,189,197,15,195,13,196,
  31,197,14,194,47,198,14,209,15,199,30,208,15,200,30,236,15,203,94,8,0,0,0,219,
  0,8,199,0,175,7,198,0,192,16,200,15,192,16,217,15,7,8,250,15,219,95,193,143,
  203,175,220,223,0,219,0,8,144,200,15,7,128,199,17,175,199,17,195,15,7,7,195,52,
  197,15,195,16,198,15,194,33,215,15,193,66,216,15,192,99,220,15,205,80,194,209,195,4,
  0,0,0,223,0,8,199,0,158,7,199,0,203,14,206,15,202,30,238,15,220,95,205,143,
  205,175,217,226,0,219,0,8,203,0,7,202,0,255,15,255,79,199,143,204,175,220,223,0,
  0,8,160,7,144,8,0,128,186,255,15,255,79,255,143,219,207,0,0,8,7,197,0,8,
  0,128,255,15,255,79,230,143,198,192,190,0,0,8,193,210,8,196,14,193,17,200,13,200,
  9,0,255,0,255,67,255,134,242,201
);
{ END GENERATED INCLUDE: SNAKETIL.INC }
{ BEGIN GENERATED INCLUDE: NUMBERS.INC }
{ Original SDK pixels, LZ encoded. See CREDITS.md and assets.json. }
const Numbers: array[0..793] of Byte = (
  0,1,201,0,0,1,2,3,4,197,0,3,2,1,1,142,5,5,197,16,159,157,2,2,
  198,15,192,47,1,1,229,15,193,95,194,47,3,175,145,220,7,2,2,197,39,4,194,49,
  192,55,192,143,195,137,193,180,2,2,192,192,199,0,1,0,0,201,224,142,203,2,0,144,
  1,195,0,172,141,2,3,4,128,3,2,193,16,1,172,5,5,4,4,3,191,174,193,32,
  195,31,3,193,46,197,31,2,194,62,197,47,1,194,65,196,63,193,49,193,95,199,15,177,
  229,15,4,192,49,198,79,2,2,192,175,2,194,175,194,130,145,196,14,196,224,197,14,199,
  8,0,1,201,0,0,1,2,3,4,197,0,3,2,1,1,142,5,5,197,16,159,197,13,
  175,2,198,46,193,31,195,79,194,69,193,47,202,31,3,200,95,175,145,2,195,0,196,15,
  195,5,204,15,193,127,198,33,193,127,199,128,2,2,192,192,199,0,1,0,0,201,224,142,
  203,2,0,1,201,0,0,1,2,3,4,197,0,3,2,1,1,142,5,5,197,16,159,197,
  13,175,2,198,46,193,31,195,79,194,69,193,47,202,31,3,200,95,192,63,195,0,3,160,
  1,1,195,8,195,5,192,47,203,15,4,198,32,193,127,199,128,2,2,192,192,199,0,1,
  0,0,201,224,142,203,2,0,1,160,0,196,7,1,2,3,4,4,4,3,1,1,166,2,
  167,5,5,4,1,1,4,144,193,23,5,4,4,201,15,192,25,204,15,3,195,6,196,31,
  195,4,192,87,199,16,3,175,200,1,1,1,2,160,197,15,195,159,198,15,160,1,3,4,
  192,55,4,197,15,2,2,192,159,2,2,196,31,195,70,195,46,196,221,197,14,198,7,0,
  1,201,0,0,1,2,3,4,197,0,3,2,1,1,142,5,5,197,16,159,197,13,195,15,
  199,0,194,47,194,40,195,88,201,31,192,63,200,95,3,1,1,2,195,0,3,160,143,184,
  195,5,192,47,203,15,4,198,32,192,143,2,2,199,126,2,192,192,199,0,1,0,0,201,
  224,142,203,2,0,1,201,0,0,1,2,3,4,197,0,3,2,1,1,142,5,5,197,16,
  159,197,13,4,4,194,31,194,24,195,72,199,62,195,47,203,15,193,95,2,2,193,95,175,
  145,220,7,2,2,197,39,4,194,49,192,55,192,143,194,73,195,142,2,192,192,199,0,1,
  1,0,204,224,203,0,0,1,201,0,0,1,2,3,4,197,0,3,2,1,1,141,5,5,
  197,16,159,197,13,175,3,176,197,31,2,160,198,15,194,95,197,31,0,160,1,194,48,3,
  199,15,194,71,229,15,4,193,116,197,79,2,2,192,181,2,2,195,95,192,16,192,136,196,
  14,0,195,230,197,14,197,23,0,1,201,0,0,1,2,3,4,197,0,3,2,1,1,142,
  5,5,197,16,159,157,2,2,198,15,192,47,1,1,202,15,3,195,6,196,31,199,63,193,
  95,194,63,3,175,145,220,7,2,2,197,39,4,194,49,192,55,192,143,195,137,193,180,2,
  2,192,192,199,0,1,0,0,201,224,142,203,2,0,1,201,0,0,1,2,3,4,197,0,
  3,2,1,1,142,5,5,197,16,159,157,2,2,198,15,192,47,1,1,218,15,199,47,200,
  95,175,200,1,1,1,2,195,0,197,15,203,31,3,4,198,49,192,143,195,137,195,101,192,
  192,199,0,1,0,0,201,224,142,203,2,0,224,0,1,160,198,14,3,160,196,16,1,3,
  5,5,4,128,196,16,143,158,198,15,2,194,47,2,197,48,2,160,199,80,255,95,231,95,
  200,243
);
{ END GENERATED INCLUDE: NUMBERS.INC }
{ BEGIN GENERATED INCLUDE: PALETTES.INC }
const SnakePalette: array[0..37] of Byte = (
  31,220,0,0,192,8,161,138,98,83,64,218,155,75,223,91,255,91,195,5,
  127,108,220,124,131,102,133,94,191,157,190,247,168,174,39,158,68,108);

const FontPalette: array[0..11] of Byte = (
  0,0,131,168,203,241,199,252,82,255,255,255);
{ END GENERATED INCLUDE: PALETTES.INC }

const BoardWidth = 20; BoardHeight = 14; MaxLength = 256;
  BodyTile: array[0..15] of Byte =
    (12,12,12,5,12,13,8,12,12,4,12,12,10,12,12,12);
type TPoint = record X, Y: Byte end;
var Context: TGfxContext;
  Body: array[0..255] of TPoint;
  Occupied: array[0..279] of Boolean;
  Tail, Head, SnakeLength, Score: Integer;
  Frames: Word;
  Direction, NextDirection, AppleX, AppleY, Tick, SoundFrames: Byte;
  Error, OldDirection: Byte;
  GameOver, Quit, Demo, SoundUsed: Boolean;
  Key: Char;
  Waiting: Boolean;

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

procedure Place(Tile, X, Y: Byte);
begin
  Error := GfxTilemapPlace(Context, Tile, 1, X, Y)
end;

procedure ShowScore;
var Value: Integer; I: Integer;
begin
  Value := Score;
  for I := 9 downto 6 do begin
    Place(48 + Value mod 10, I, 14);
    Value := Value div 10
  end
end;

procedure Beep(Divider: Word; Duration: Byte; Wave: Byte);
begin
  SoundSetVoices(SoundVoice0, Divider, Wave);
  SoundSetHold(SoundVoice0, False);
  SoundFrames := Duration
end;

procedure NewApple;
var Cell: Integer;
begin
  { Start randomly, then scan every cell so a nearly full board terminates. }
  Cell := Random(280);
  while Occupied[Cell] do begin
    Cell := Cell + 1;
    if Cell = 280 then Cell := 0
  end;
  AppleX := Cell mod BoardWidth;
  AppleY := Cell div BoardWidth;
  Place(7, AppleX, AppleY)
end;

procedure ResetGame;
var Y: Byte; I: Word; Row: array[0..19] of Byte;
begin
  GfxEnableScreen(False);
  for I := 0 to 279 do Occupied[I] := False;
  FillChar(Row, SizeOf(Row), 15);
  for Y := 0 to 14 do begin
    Error := GfxTilemapLoad(Context, Row, 20, 0, 0, Y);
    Error := GfxTilemapLoad(Context, Row, 20, 1, 0, Y)
  end;
  Tail := 0; Head := 1; SnakeLength := 2; Score := 0;
  Body[0].X := 9; Body[0].Y := 7;
  Body[1].X := 10; Body[1].Y := 7;
  Occupied[149] := True; Occupied[150] := True;
  Direction := 1; NextDirection := 1; Tick := 0;
  GameOver := False; Waiting := not Demo;
  Place(11, 9, 7); Place(1, 10, 7);
  NewApple;
  if Demo then begin
    Place(15, AppleX, AppleY); AppleX := 11; AppleY := 7;
    Place(7, AppleX, AppleY)
  end;
  ShowScore;
  Beep(654, 5, SoundSquare or SoundDuty50);
  GfxEnableScreen(True)
end;

procedure Lose;
begin
  GameOver := True;
  Place(14, 11, 14); { Tail icon marks game over. }
  Beep(163, 20, SoundNoise)
end;

procedure MoveSnake;
var X, Y: Integer; Cell, OldHead, NextTail: Integer;
  Growing: Boolean; Mask, Tile: Byte;
begin
  X := Body[Head].X; Y := Body[Head].Y;
  OldDirection := Direction; Direction := NextDirection;
  if Direction = 0 then Y := Y - 1
  else if Direction = 1 then X := X + 1
  else if Direction = 2 then Y := Y + 1
  else X := X - 1;
  if (X < 0) or (X >= BoardWidth) or (Y < 0) or (Y >= BoardHeight)
    then begin Lose; Exit end;
  Cell := Y * BoardWidth + X;
  Growing := (X = AppleX) and (Y = AppleY);
  if Occupied[Cell] and (Growing or (X <> Body[Tail].X) or
    (Y <> Body[Tail].Y)) then begin Lose; Exit end;
  OldHead := Head;
  if not Growing then begin
    Place(15, Body[Tail].X, Body[Tail].Y);
    Occupied[Body[Tail].Y * BoardWidth + Body[Tail].X] := False;
    Tail := (Tail + 1) and 255
  end else begin
    SnakeLength := SnakeLength + 1; Score := Score + 1;
    ShowScore;
    Beep(1308, 6, SoundSquare or SoundDuty50)
  end;
  Head := (Head + 1) and 255;
  Body[Head].X := X; Body[Head].Y := Y;
  Occupied[Cell] := True;
  Mask := (1 shl ((OldDirection + 2) and 3)) or (1 shl Direction);
  Tile := BodyTile[Mask];
  Place(Tile, Body[OldHead].X, Body[OldHead].Y);
  Place(Direction, X, Y);
  NextTail := (Tail + 1) and 255;
  if Body[NextTail].X > Body[Tail].X then Tile := 11
  else if Body[NextTail].X < Body[Tail].X then Tile := 9
  else if Body[NextTail].Y > Body[Tail].Y then Tile := 6
  else Tile := 14;
  Place(Tile, Body[Tail].X, Body[Tail].Y);
  if Growing then begin
    if SnakeLength = MaxLength then begin
      GameOver := True;
      Place(7, 11, 14) { Apple icon marks maximum length. }
    end else NewApple
  end
end;

procedure ReadControls;
var NewDirection: Byte;
begin
  if KeyPressed then begin
    Key := UpCase(ReadKey);
    NewDirection := 255;
    case Key of
      #27, 'Q': Quit := True;
      'R': ResetGame;
      'W', #160: NewDirection := 0;
      'D', #163: NewDirection := 1;
      'S', #161: NewDirection := 2;
      'A', #162: NewDirection := 3
    end;
    if (NewDirection <= 3) and (NewDirection <> ((Direction + 2) and 3)) then
      begin NextDirection := NewDirection; Waiting := False end
  end
end;

function Initialize: Boolean;
var I: Byte;
begin
  Initialize := False;
  if not SaveVideo then Exit;
  GfxEnableScreen(False);
  Error := GfxInitialize(GfxMode3208Bit, Context);
  if Error <> GfxSuccess then Exit;
  Error := GfxPaletteLoad(Context, SnakePalette, SizeOf(SnakePalette), 0);
  if Error <> GfxSuccess then Exit;
  Error := GfxPaletteLoad(Context, FontPalette, SizeOf(FontPalette), 32);
  if Error <> GfxSuccess then Exit;
  Error := GfxTilesetLoadLZ(Context, SnakeTiles, SizeOf(SnakeTiles), 0, 0, False);
  if Error <> GfxSuccess then Exit;
  Error := GfxTilesetLoadLZ(Context, Numbers, SizeOf(Numbers), $3000, 32, True);
  if Error <> GfxSuccess then Exit;
  { Palette zero is transparent on layer1, and the field color on layer0. }
  Error := GfxPaletteLoad(Context, SnakePalette[32], 2, 0);
  if Error <> GfxSuccess then Exit;
  { Y480 is below the 240-line field even for a pre-existing 32px sprite. }
  for I := 0 to 127 do Error := GfxSpriteSetY(Context, I, 480);
  { Neutral scrolling, explicitly owned by this demo. }
  for I := 0 to 7 do Port[$94 + I] := 0;
  SoundUsed := True;
  SoundSetHold($FF, True);
  SoundSetChannels(SoundVoice0, SoundVoice0);
  SoundSetVoicesVol(SoundVoice0, SoundVol50);
  SoundSetVolumes(SoundVol50, SoundVol50);
  ResetGame;
  GfxEnableScreen(True);
  Initialize := True
end;

begin
  VideoSaved := False; SoundUsed := False; Quit := False; Frames := 0; Score := 0;
  Demo := False;
  if ParamCount > 0 then Demo := ParamStr(1) = '--demo';
  if not Demo then Randomize;
  CrtInit;
  if Initialize then begin
    while not Quit do begin
      Error := GfxWaitVBlank(Context);
      ReadControls;
      if SoundFrames <> 0 then begin
        SoundFrames := SoundFrames - 1;
        if SoundFrames = 0 then SoundSetHold(SoundVoice0, True)
      end;
      if not GameOver and not Waiting then begin
        Tick := Tick + 1;
        if Tick = 6 then begin Tick := 0; MoveSnake end
      end;
      Frames := Frames + 1;
      if Demo then begin
        if Frames = 100 then ResetGame;
        if Frames = 180 then Quit := True
      end;
      Error := GfxWaitEndVBlank(Context)
    end
  end;
  if SoundUsed then begin
    SoundSetHold($FF, True); SoundSetVolumes(SoundVol0, SoundVol0)
  end;
  RestoreVideo(Context);
  CrtExit;
  if Demo and (Frames = 180) then WriteLn('SNAKE-DEMO-OK');
  WriteLn('Snake finished. Score: ', Score);
  WriteLn('Snake: Clear_code CC0; JROB774 Pixel Block Font, adapted for Zeal, https://creativecommons.org/licenses/by/3.0/');
  WriteLn('https://opengameart.org/content/pixel-block-font')
end.
