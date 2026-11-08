program Snake;
uses ZVB, Crt;
const BoardWidth = 20;
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
{$I NUMBERS.INC}
{$I SNAKEFONT.INC}
{ BEGIN GENERATED INCLUDE: PALETTES.INC }
const SnakePalette: array[0..37] of Byte = (
  31,220,0,0,192,8,161,138,98,83,64,218,155,75,223,91,255,91,195,5,
  127,108,220,124,131,102,133,94,191,157,190,247,168,174,39,158,68,108);

const FontPalette: array[0..11] of Byte = (
  0,0,131,168,203,241,199,252,82,255,255,255);
{ END GENERATED INCLUDE: PALETTES.INC }

const BoardHeight = 14; MaxLength = 255;
  BodyTile: array[0..15] of Byte =
    (12,12,12,5,12,13,8,12,12,4,12,12,10,12,12,12);
  Checker: array[0..20] of Byte =
    (32,33,32,33,32,33,32,33,32,33,32,33,32,33,32,33,32,33,32,33,32);
  ScoreLabel: array[0..5] of Byte = (83,67,79,82,69,58);
  TailTile: array[0..3] of Byte = (14,11,6,9);
  DeltaX: array[0..3] of Integer = (0,1,0,-1);
  DeltaY: array[0..3] of Integer = (-1,0,1,0);
var Context: TGfxContext;
  { Zero is empty;1..4 links an occupied cell toward the head. }
  Board: array[0..279] of Byte;
  TailX, TailY, HeadX, HeadY: Byte;
  SnakeLength, Score: Integer;
  Frames: Word;
  Direction, AppleX, AppleY, Tick: Byte;
  Error: Byte;
  GameOver, Quit, Demo, Waiting, JustAte, Controller: Boolean;
  Key: Char;
  Speed: Byte;
  Keys: array[0..31] of Byte;
  KeyCount: Word;
  InputError: Byte;

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

procedure ShowScore;
var Value: Integer; I: Integer;
begin
  Error := GfxTilemapLoad(Context, ScoreLabel, 6, 1, 10, 14);
  Value := Score;
  for I := 18 downto 16 do begin
    Error := GfxTilemapPlace(Context, 48 + Value mod 10, 1, I, 14);
    Value := Value div 10
  end
end;

{ Raw nonblocking stdin, using the same Zeal API and 32-byte bursts as C.
  Inline leaves take Pascal arguments; no program addresses are embedded. }
procedure KeyboardMode(Mode: Byte; var Status: Byte);
begin
  Inline($DD/$5E/$06/$16/$00/$21/$07/$01/$0E/$00/$CF/
    $DD/$6E/$04/$DD/$66/$05/$77)
end;

procedure RawRead(var Buffer; var Count: Word; var Status: Byte);
begin
  Inline($DD/$6E/$06/$DD/$66/$07/$4E/$23/$46/
    $DD/$5E/$08/$DD/$56/$09/$21/$00/$01/$CF/
    $DD/$6E/$04/$DD/$66/$05/$77/
    $DD/$6E/$06/$DD/$66/$07/$71/$23/$70)
end;

procedure PollKeys;
begin
  KeyCount := 32; RawRead(Keys, KeyCount, InputError);
  if (InputError <> 0) or (KeyCount > 32) then begin
    Quit := True; KeyCount := 0
  end
end;

{ Original refresh-register fruit algorithm, including its spatial bias.
  X and Y are adjacent bytes. Bound retries so a crowded board cannot hang. }
procedure PickFruit(var X: Byte);
begin
  Inline($DD/$6E/$04/$DD/$66/$05/$ED/$5F/$E6/$1F/$FE/$14/
    $38/$02/$D6/$14/$77/$23/$ED/$5F/$E6/$0F/$FE/$0E/
    $38/$02/$D6/$0E/$77)
end;

procedure NewApple;
var Cell, Attempts: Integer;
begin
  Attempts := 0;
  repeat
    PickFruit(AppleX);
    Cell := AppleY * BoardWidth + AppleX;
    Attempts := Attempts + 1
  until (Board[Cell] = 0) or (Attempts = 32);
  while Board[Cell] <> 0 do begin
    Cell := Cell + 1;
    if Cell = 280 then Cell := 0
  end;
  AppleX := Cell mod BoardWidth; AppleY := Cell div BoardWidth
end;

procedure DrawBackground;
var Y: Byte; Row: array[0..19] of Byte;
begin
  FillChar(Row, SizeOf(Row), 15);
  for Y := 0 to 13 do begin
    Error := GfxTilemapLoad(Context, Checker[Y and 1], 20, 0, 0, Y);
    Error := GfxTilemapLoad(Context, Row, 20, 1, 0, Y)
  end;
  Error := GfxTilemapLoad(Context, Row, 20, 1, 0, 14);
  FillChar(Row, SizeOf(Row), 34);
  Error := GfxTilemapLoad(Context, Row, 20, 0, 0, 14)
end;

procedure PlayTitle;
const Title: array[0..5] of Byte = (83,78,65,75,69,0);
var I: Byte; Y, Velocity, Whole: Integer;
  Finished: Boolean; Sprite: TGfxSprite;
begin
  Sprite.Flags := 0; Sprite.Options := 0;
  for I := 0 to 5 do begin
    Sprite.X := 128 + I * 16; Sprite.Y := 0; Sprite.Tile := Title[I];
    Error := GfxWaitVBlank(Context);
    Error := GfxSpriteRender(Context, I, Sprite);
    Error := GfxWaitEndVBlank(Context);
    Y := 0; Velocity := 0; Finished := False;
    repeat
      Error := GfxWaitVBlank(Context);
      Velocity := Velocity + 36; Y := Y + Velocity; Whole := Y div 256;
      if Whole > 64 then begin
        Y := 16384;
        if Velocity <= 360 then Finished := True
        else begin Velocity := -Velocity div 2; Whole := 64 end
      end;
      if not Finished then Error := GfxSpriteSetY(Context, I, Whole);
      Error := GfxWaitEndVBlank(Context)
    until Finished;
    Error := GfxSpriteSetY(Context, I, 64)
  end;
  for I := 0 to 5 do Error := GfxSpriteSetY(Context, I, 0);
  Delay(100)
end;

procedure ResetGame;
var I: Word;
begin
  { Replay restores the post-title board without blanking the display. }
  DrawBackground;
  Error := GfxPaletteLoad(Context, SnakePalette, SizeOf(SnakePalette), 0);
  for I := 0 to 279 do Board[I] := 0;
  TailX := 9; TailY := 7; HeadX := 10; HeadY := 7;
  SnakeLength := 2; Score := 0; Speed := 0;
  Board[149] := 2; Board[150] := 2;
  Direction := 1; Tick := 0;
  GameOver := False; Waiting := False; JustAte := False;
  NewApple;
  if Demo then begin AppleX := 11; AppleY := 7 end;
  ShowScore
end;

procedure MoveSnake;
var X, Y, Cell, TailCell: Integer;
  Growing, Eating: Boolean; Mask, Tile, TailDirection, Previous: Byte;
begin
  { The score is committed before collision, as in the original. }
  Growing := JustAte; JustAte := False;
  if Growing then begin
    SnakeLength := SnakeLength + 1; Score := Score + 1;
    if (Score mod 8 = 0) and (Speed < 10) then Speed := Speed + 1;
    ShowScore
  end;
  Previous := Board[HeadY * BoardWidth + HeadX] - 1;
  X := HeadX + DeltaX[Direction]; Y := HeadY + DeltaY[Direction];
  if (X < 0) or (X >= BoardWidth) or (Y < 0) or (Y >= BoardHeight)
    then begin GameOver := True; Exit end;
  Cell := Y * BoardWidth + X; TailCell := TailY * BoardWidth + TailX;
  if (Board[Cell] <> 0) and (Growing or (Cell <> TailCell))
    then begin GameOver := True; Exit end;
  Eating := (X = AppleX) and (Y = AppleY);
  Board[HeadY * BoardWidth + HeadX] := Direction + 1;
  if not Growing then begin
    Error := GfxTilemapPlace(Context, 15, 1, TailX, TailY);
    TailDirection := Board[TailCell] - 1; Board[TailCell] := 0;
    TailX := TailX + DeltaX[TailDirection];
    TailY := TailY + DeltaY[TailDirection]
  end;
  Mask := (1 shl ((Previous + 2) and 3)) or (1 shl Direction);
  Tile := BodyTile[Mask]; Error := GfxTilemapPlace(Context, Tile, 1, HeadX, HeadY);
  HeadX := X; HeadY := Y; Board[Cell] := Direction + 1;
  Error := GfxTilemapPlace(Context, Direction, 1, HeadX, HeadY);
  TailDirection := Board[TailY * BoardWidth + TailX] - 1;
  Error := GfxTilemapPlace(Context, TailTile[TailDirection], 1, TailX, TailY);
  if Eating then begin
    JustAte := SnakeLength < MaxLength; NewApple
  end;
  Error := GfxTilemapPlace(Context, 7, 1, AppleX, AppleY)
end;

function ControllerDirection: Byte;
const Directions: array[0..3] of Byte = (0,2,3,1);
var I: Byte;
begin
  Port[$D0] := 12; Port[$D0] := 8;
  for I := 0 to 3 do begin Port[$D0] := 0; Port[$D0] := 8 end;
  ControllerDirection := 255;
  for I := 0 to 3 do begin
    if Port[$D0] and 1 = 0 then begin
      ControllerDirection := Directions[I]; Exit
    end;
    if I < 3 then begin Port[$D0] := 0; Port[$D0] := 8 end
  end
end;

procedure ReadControls;
var NewDirection, Chosen: Byte; I: Word;
begin
  Chosen := 255;
  repeat
    PollKeys;
    if (KeyCount = 0) and Controller then begin
      NewDirection := ControllerDirection;
      if (NewDirection <= 3) and (NewDirection <> Direction) and
        (NewDirection <> ((Direction + 2) and 3)) then Chosen := NewDirection
    end;
    I := 0;
    while I < KeyCount do begin
      if Keys[I] = 254 then I := I + 2
      else begin
        Key := UpCase(Chr(Keys[I])); NewDirection := 255;
        case Key of
          #27, #240, 'Q': Quit := True;
          'W', #160, #136: NewDirection := 0;
          'D', #163, #134: NewDirection := 1;
          'S', #161, #130: NewDirection := 2;
          'A', #162, #132: NewDirection := 3
        end;
        if (NewDirection <= 3) and (NewDirection <> Direction) and
          (NewDirection <> ((Direction + 2) and 3)) then begin
          Chosen := NewDirection; I := KeyCount
        end else I := I + 1
      end
    end
  until KeyCount = 0;
  if Chosen <= 3 then Direction := Chosen
end;

procedure EndGame;
const DeathPalette: array[0..1] of Byte = ($AE,0);
var X, Y: Byte;
begin
  { Match observed C colors: color6 changes; color7 remains unchanged. }
  Error := GfxPaletteLoad(Context, DeathPalette, 2, 6);
  for Y := 0 to 13 do
    for X := 0 to 19 do begin
      Error := GfxTilemapPlace(Context, 7, 1, X, Y); Delay(4)
    end;
  repeat PollKeys until KeyCount = 0;
  Waiting := True
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
  Error := GfxTilesetLoadLZ(Context, SnakeFont, SizeOf(SnakeFont), $4100, 32, True);
  if Error <> GfxSuccess then Exit;
  { Background tiles are generated, exactly as in the C game. }
  for I := 0 to 2 do begin
    Error := GfxTilesetAddColorTile(Context, 32 + I, 16 + I);
    if Error <> GfxSuccess then Exit
  end;
  { Y480 is below the 240-line field even for a pre-existing 32px sprite. }
  for I := 0 to 127 do Error := GfxSpriteSetY(Context, I, 480);
  { Neutral scrolling, explicitly owned by this demo. }
  for I := 0 to 7 do Port[$94 + I] := 0;
  DrawBackground;
  GfxEnableScreen(True);
  Initialize := True
end;

begin
  VideoSaved := False; Quit := False; Frames := 0; Score := 0;
  Demo := ParamStr(1) = '--demo'; Controller := ParamStr(1) = '-c';
  if Controller then begin
    Port[$D2] := $CF; Port[$D2] := 3; Port[$D2] := 3; Port[$D0] := 8
  end;
  CrtInit; KeyboardMode(4, InputError);
  if (InputError = 0) and Initialize then begin
    PlayTitle; ResetGame;
    while not Quit do begin
      Error := GfxWaitVBlank(Context);
      if Waiting then begin
        PollKeys;
        if KeyCount <> 0 then begin
          if (Keys[0] = 240) or (Keys[0] = 27) or (Keys[0] = Ord('q')) then Quit := True
          else ResetGame
        end
      end else begin
        ReadControls;
        if not GameOver and not Quit then begin
          Tick := Tick + 1;
          if Tick >= 20 - Speed then begin Tick := 0; MoveSnake end
        end
      end;
      Frames := Frames + 1;
      if Demo then begin
        if Frames = 100 then ResetGame;
        if Frames = 180 then Quit := True
      end;
      Error := GfxWaitEndVBlank(Context);
      if GameOver and not Waiting and not Demo then EndGame
    end
  end;
  RestoreVideo(Context); KeyboardMode(0, InputError); CrtExit;
  if Demo and (Frames = 180) then WriteLn('SNAKE-DEMO-OK');
  WriteLn('Snake finished. Score: ', Score);
  WriteLn('Snake: Clear_code CC0; JROB774 Pixel Block Font, adapted for Zeal, https://creativecommons.org/licenses/by/3.0/');
  WriteLn('https://opengameart.org/content/pixel-block-font')
end.
