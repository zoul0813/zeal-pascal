{ CRT text console for Zeal. Ordinary behavior is Pascal; native leaves own
  OS calls and uninterrupted VRAM row operations. }
{$A+}{$I-}{$R+}
unit Crt;
interface
procedure ClrScr;
procedure ClrEol;
procedure GotoXY(X, Y: Integer);
procedure InsLine;
procedure DelLine;
procedure CrtInit;
procedure CrtExit;
procedure LowVideo;
procedure NormVideo;
procedure HighVideo;
procedure Delay(Milliseconds: Integer);
procedure TextColor(Color: Integer);
procedure TextBackground(Color: Integer);
function WhereX: Byte;
function WhereY: Byte;
function ReadKey: Char;
implementation
{ Register names match the public ZVB Unit and SDK. }
const
  ZVB_PERI_TEXT_IDX = 0;
  Z80_MMU_PAGE0 = $F0;
  ZVB_TEXT_VRAM_PAGE = $40;
  ZVB_CONFIG_DEV_IDX = $8E;
  ZVB_PERI_TEXT_PRINT_CHAR = $A0;
  ZVB_PERI_TEXT_CURS_Y = $A1;
  ZVB_PERI_TEXT_CURS_X = $A2;
  ZVB_PERI_TEXT_SCROLL_Y = $A3;
  ZVB_PERI_TEXT_SCROLL_X = $A4;
  ZVB_PERI_TEXT_COLOR = $A5;
  ZVB_PERI_TEXT_CURS_TIME = $A6;
  ZVB_PERI_TEXT_CURS_CHAR = $A7;
  ZVB_PERI_TEXT_CURS_COLOR = $A8;
  ZVB_PERI_TEXT_CTRL = $A9;
var Snapshot: array[0..8] of Byte;


{ Read shared runtime state. }
function SessionActive: Byte; native;
begin
  Inline(
    $3A/Runtime(CrtActive)/                  { ld a,(Runtime(CrtActive)) }
    $6F/                                     { ld l,a }
    $26/$00/                                 { ld h,$00 }
    $C9                                      { ret }
  )
end;

{ Session lifetime is shared with the runtime Run/Chain reset. }
procedure SetSession(Value: Byte); native;
begin
  Inline(
    $C1/                                     { pop bc }
    $E1/                                     { pop hl }
    $C5/                                     { push bc }
    $7D/                                     { ld a,l }
    $32/Runtime(CrtActive)/                  { ld (Runtime(CrtActive)),a }
    $C9                                      { ret }
  )
end;

{ Write a contiguous character/attribute span. Caller supplies 1..80 cells within one row. }
procedure ClearSpan(Start, Count: Word; Color: Byte); native;
begin
  Inline(
    $DD/$E5/                                 { push ix }
    $DD/$21/$00/$00/                         { ld ix,$0000 }
    $DD/$39/                                 { add ix,sp }
    $DD/$6E/$08/                             { ld l,(ix+8) }
    $DD/$66/$09/                             { ld h,(ix+9) }
    $DD/$4E/$06/                             { ld c,(ix+6) }
    $DD/$46/$07/                             { ld b,(ix+7) }
    $78/                                     { ld a,b }
    $B1/                                     { or c }
    $CA/*+58/                                { jp z,*+58 }
    $ED/$57/                                 { ld a,i (capture IFF2 in parity flag) }
    $F5/                                     { push af }
    $F3/                                     { di }
    $AF/                                     { xor a (select MMU window 0 for read) }
    $DB/Z80_MMU_PAGE0/                       { in a,(Z80_MMU_PAGE0) }
    $F5/                                     { push af }
    $3E/ZVB_TEXT_VRAM_PAGE/                  { ld a,ZVB_TEXT_VRAM_PAGE }
    $D3/Z80_MMU_PAGE0/                       { out (Z80_MMU_PAGE0),a }
    $E5/                                     { push hl }
    $C5/                                     { push bc }
    $54/                                     { ld d,h }
    $5D/                                     { ld e,l }
    $13/                                     { inc de }
    $36/$00/                                 { ld (hl),$00 }
    $0B/                                     { dec bc }
    $78/                                     { ld a,b }
    $B1/                                     { or c }
    $CA/*+4/                                 { jp z,*+4 }
    $ED/$B0/                                 { ldir }
    $C1/                                     { pop bc }
    $E1/                                     { pop hl }
    $11/$00/$10/                             { ld de,$1000 }
    $19/                                     { add hl,de }
    $54/                                     { ld d,h }
    $5D/                                     { ld e,l }
    $13/                                     { inc de }
    $DD/$7E/$04/                             { ld a,(ix+4) }
    $77/                                     { ld (hl),a }
    $0B/                                     { dec bc }
    $78/                                     { ld a,b }
    $B1/                                     { or c }
    $CA/*+4/                                 { jp z,*+4 }
    $ED/$B0/                                 { ldir }
    $F1/                                     { pop af }
    $D3/Z80_MMU_PAGE0/                       { out (Z80_MMU_PAGE0),a }
    $F1/                                     { pop af }
    $E2/*+3/                                 { jp po,*+3 }
    $FB/                                     { ei }
    $DD/$E1/                                 { pop ix }
    $C1/                                     { pop bc }
    $D1/                                     { pop de }
    $D1/                                     { pop de }
    $D1/                                     { pop de }
    $C5/                                     { push bc }
    $C9                                      { ret }
  )
end;

{ Copy 80 characters and their attributes between disjoint physical rows. }
procedure CopyRow(Source, Destination: Word); native;
begin
  Inline(
    $DD/$E5/                                 { push ix }
    $DD/$21/$00/$00/                         { ld ix,$0000 }
    $DD/$39/                                 { add ix,sp }
    $DD/$6E/$06/                             { ld l,(ix+6) }
    $DD/$66/$07/                             { ld h,(ix+7) }
    $DD/$5E/$04/                             { ld e,(ix+4) }
    $DD/$56/$05/                             { ld d,(ix+5) }
    $ED/$57/                                 { ld a,i (capture IFF2 in parity flag) }
    $F5/                                     { push af }
    $F3/                                     { di }
    $AF/                                     { xor a (select MMU window 0 for read) }
    $DB/Z80_MMU_PAGE0/                       { in a,(Z80_MMU_PAGE0) }
    $F5/                                     { push af }
    $3E/ZVB_TEXT_VRAM_PAGE/                  { ld a,ZVB_TEXT_VRAM_PAGE }
    $D3/Z80_MMU_PAGE0/                       { out (Z80_MMU_PAGE0),a }
    $01/$50/$00/                             { ld bc,$0050 }
    $ED/$B0/                                 { ldir }
    $01/$B0/$0F/                             { ld bc,$0FB0 }
    $09/                                     { add hl,bc }
    $EB/                                     { ex de,hl }
    $09/                                     { add hl,bc }
    $EB/                                     { ex de,hl }
    $01/$50/$00/                             { ld bc,$0050 }
    $ED/$B0/                                 { ldir }
    $F1/                                     { pop af }
    $D3/Z80_MMU_PAGE0/                       { out (Z80_MMU_PAGE0),a }
    $F1/                                     { pop af }
    $E2/*+3/                                 { jp po,*+3 }
    $FB/                                     { ei }
    $DD/$E1/                                 { pop ix }
    $C1/                                     { pop bc }
    $D1/                                     { pop de }
    $D1/                                     { pop de }
    $C5/                                     { push bc }
    $C9                                      { ret }
  )
end;

{ Reprint the final cell to reconstruct pending wrap; no Pascal runs while VRAM is mapped. }
procedure RestoreWrap(Cell: Word); native;
begin
  Inline(
    $DD/$E5/                                 { push ix }
    $DD/$21/$00/$00/                         { ld ix,$0000 }
    $DD/$39/                                 { add ix,sp }
    $DD/$6E/$04/                             { ld l,(ix+4) }
    $DD/$66/$05/                             { ld h,(ix+5) }
    $ED/$57/                                 { ld a,i (capture IFF2 in parity flag) }
    $F5/                                     { push af }
    $F3/                                     { di }
    $AF/                                     { xor a (select MMU window 0 for read) }
    $DB/Z80_MMU_PAGE0/                       { in a,(Z80_MMU_PAGE0) }
    $F5/                                     { push af }
    $3E/ZVB_TEXT_VRAM_PAGE/                  { ld a,ZVB_TEXT_VRAM_PAGE }
    $D3/Z80_MMU_PAGE0/                       { out (Z80_MMU_PAGE0),a }
    $5E/                                     { ld e,(hl) }
    $01/$00/$10/                             { ld bc,$1000 }
    $09/                                     { add hl,bc }
    $7E/                                     { ld a,(hl) }
    $D3/ZVB_PERI_TEXT_COLOR/                 { out (ZVB_PERI_TEXT_COLOR),a }
    $7B/                                     { ld a,e }
    $D3/ZVB_PERI_TEXT_PRINT_CHAR/            { out (ZVB_PERI_TEXT_PRINT_CHAR),a }
    $F1/                                     { pop af }
    $D3/Z80_MMU_PAGE0/                       { out (Z80_MMU_PAGE0),a }
    $F1/                                     { pop af }
    $E2/*+3/                                 { jp po,*+3 }
    $FB/                                     { ei }
    $DD/$E1/                                 { pop ix }
    $C1/                                     { pop bc }
    $D1/                                     { pop de }
    $C5/                                     { push bc }
    $C9                                      { ret }
  )
end;

procedure ClrScr;
var Row: Byte; Color: Byte;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  Color := Port[ZVB_PERI_TEXT_COLOR];
  for Row := 0 to 39 do ClearSpan(Row * 80, 80, Color);
  Port[ZVB_PERI_TEXT_SCROLL_Y] := 0;
  Port[ZVB_PERI_TEXT_SCROLL_X] := 0;
  Port[ZVB_PERI_TEXT_CURS_Y] := 0;
  Port[ZVB_PERI_TEXT_CURS_X] := 0
end;

procedure ClrEol;
var X, Y, PhysicalX, Count, First, Color: Byte; RowStart: Word;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  X := Port[ZVB_PERI_TEXT_CURS_X];
  Y := Port[ZVB_PERI_TEXT_CURS_Y];
  if (X < 80) and (Y < 40) then begin
    RowStart := ((Y + Port[ZVB_PERI_TEXT_SCROLL_Y]) mod 40) * 80;
    PhysicalX := (X + Port[ZVB_PERI_TEXT_SCROLL_X]) mod 80;
    Count := 80 - X;
    First := 80 - PhysicalX;
    if First > Count then First := Count;
    Color := Port[ZVB_PERI_TEXT_COLOR];
    ClearSpan(RowStart + PhysicalX, First, Color);
    if Count > First then ClearSpan(RowStart, Count - First, Color)
  end
end;

procedure InsLine;
var CursorRow, ScrollY, Color: Byte; Row: Integer; FromRow, ToRow: Word;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  CursorRow := Port[ZVB_PERI_TEXT_CURS_Y];
  if CursorRow < 40 then begin
    ScrollY := Port[ZVB_PERI_TEXT_SCROLL_Y];
    Color := Port[ZVB_PERI_TEXT_COLOR];
    for Row := 39 downto CursorRow + 1 do begin
      FromRow := ((Row - 1 + ScrollY) mod 40) * 80;
      ToRow := ((Row + ScrollY) mod 40) * 80;
      CopyRow(FromRow, ToRow)
    end;
    ToRow := ((CursorRow + ScrollY) mod 40) * 80;
    ClearSpan(ToRow, 80, Color)
  end
end;

procedure DelLine;
var CursorRow, ScrollY, Color: Byte; Row: Integer; FromRow, ToRow: Word;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  CursorRow := Port[ZVB_PERI_TEXT_CURS_Y];
  if CursorRow < 40 then begin
    ScrollY := Port[ZVB_PERI_TEXT_SCROLL_Y];
    Color := Port[ZVB_PERI_TEXT_COLOR];
    for Row := CursorRow to 38 do begin
      FromRow := ((Row + 1 + ScrollY) mod 40) * 80;
      ToRow := ((Row + ScrollY) mod 40) * 80;
      CopyRow(FromRow, ToRow)
    end;
    ToRow := ((39 + ScrollY) mod 40) * 80;
    ClearSpan(ToRow, 80, Color)
  end
end;

procedure GotoXY(X, Y: Integer);
var Column: 1..80; Row: 1..40;
begin
  Column := X; Row := Y;
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  Port[ZVB_PERI_TEXT_CURS_Y] := Row - 1;
  Port[ZVB_PERI_TEXT_CURS_X] := Column - 1
end;

procedure CrtInit;
var I: Byte;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  for I := 1 to 8 do Snapshot[I] := Port[ZVB_PERI_TEXT_PRINT_CHAR + I];
  Snapshot[0] := Port[ZVB_PERI_TEXT_CTRL] and $38;
  SetSession(1);
  Port[ZVB_PERI_TEXT_COLOR] := White;
  Port[ZVB_PERI_TEXT_CURS_COLOR] := $F0
end;

procedure CrtExit;
var I: Byte; Cell: Word;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  if SessionActive <> 0 then begin
    Port[ZVB_PERI_TEXT_CURS_Y] := Snapshot[1];
    for I := 3 to 8 do Port[ZVB_PERI_TEXT_PRINT_CHAR + I] := Snapshot[I];
    Port[ZVB_PERI_TEXT_CTRL] := Snapshot[0];
    if Snapshot[2] < 80 then Port[ZVB_PERI_TEXT_CURS_X] := Snapshot[2]
    else begin
      Port[ZVB_PERI_TEXT_CURS_X] := 79;
      Port[ZVB_PERI_TEXT_SCROLL_X] := 0;
      Cell := ((Snapshot[1] + Snapshot[3]) mod 40) * 80 + 79;
      RestoreWrap(Cell);
      Port[ZVB_PERI_TEXT_COLOR] := Snapshot[5];
      Port[ZVB_PERI_TEXT_SCROLL_X] := Snapshot[4]
    end;
    SetSession(0)
  end
end;

procedure LowVideo;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  Port[ZVB_PERI_TEXT_COLOR] := LightGray;
  Port[ZVB_PERI_TEXT_CURS_COLOR] := $70
end;

procedure NormVideo;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  Port[ZVB_PERI_TEXT_COLOR] := White;
  Port[ZVB_PERI_TEXT_CURS_COLOR] := $F0
end;

procedure HighVideo;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  Port[ZVB_PERI_TEXT_COLOR] := White;
  Port[ZVB_PERI_TEXT_CURS_COLOR] := $F0
end;

procedure TextColor(Color: Integer);
var CheckedColor: 0..15; Rendition: Byte;
begin
  CheckedColor := Color;
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  Rendition := (Port[ZVB_PERI_TEXT_COLOR] and $F0) or CheckedColor;
  Port[ZVB_PERI_TEXT_COLOR] := Rendition;
  Port[ZVB_PERI_TEXT_CURS_COLOR] := (Rendition shr 4) or ((Rendition and $0F) shl 4)
end;

procedure TextBackground(Color: Integer);
var CheckedColor: 0..15; Rendition: Byte;
begin
  CheckedColor := Color;
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  Rendition := (Port[ZVB_PERI_TEXT_COLOR] and $0F) or (CheckedColor shl 4);
  Port[ZVB_PERI_TEXT_COLOR] := Rendition;
  Port[ZVB_PERI_TEXT_CURS_COLOR] := (Rendition shr 4) or ((Rendition and $0F) shl 4)
end;

function WhereX: Byte;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  WhereX := Port[ZVB_PERI_TEXT_CURS_X] + 1
end;

function WhereY: Byte;
begin
  Port[ZVB_CONFIG_DEV_IDX] := ZVB_PERI_TEXT_IDX;
  WhereY := Port[ZVB_PERI_TEXT_CURS_Y] + 1
end;

procedure Delay(Milliseconds: Integer); native; callerio;
begin
  Inline(
    $2A/Runtime(PC)/                         { ld hl,(Runtime(PC)) }
    $22/*+7/                                 { ld (*+7),hl }
    $CD/Runtime(Guard)/                      { call Runtime(Guard) }
    $60/$00/                                 { dw stack reservation in bytes }
    $00/$00/                                 { dw caller source PC (patched before call) }
    $DD/$E5/                                 { push ix }
    $DD/$21/$00/$00/                         { ld ix,$0000 }
    $DD/$39/                                 { add ix,sp }
    $32/*+22/                                { ld (*+22),a }
    $DD/$6E/$04/                             { ld l,(ix+4) }
    $DD/$66/$05/                             { ld h,(ix+5) }
    $11/$00/$00/                             { ld de,$0000 }
    $01/$FF/$7F/                             { ld bc,$7FFF }
    $CD/Runtime(Range)/                      { call Runtime(Range) }
    $CD/Runtime(IOCall)/                     { call Runtime(IOCall) }
    *+11/                                    { dw operation entry }
    $01/                                     { db I/O checking policy }
    $DD/$F9/                                 { ld sp,ix }
    $DD/$E1/                                 { pop ix }
    $C1/                                     { pop bc }
    $D1/                                     { pop de }
    $C5/                                     { push bc }
    $C9/                                     { ret }
    $3A/Runtime(IOStatus)/                   { ld a,(Runtime(IOStatus)) }
    $B7/                                     { or a }
    $C2/Runtime(IOFail)/                     { jp nz,Runtime(IOFail) }
    $EB/                                     { ex de,hl }
    $2A/Runtime(PC)/                         { ld hl,(Runtime(PC)) }
    $E5/                                     { push hl }
    $21/$12/$00/                             { ld hl,$0012 }
    $CF/                                     { rst $08 (Zeal OS syscall) }
    $E1/                                     { pop hl }
    $22/Runtime(PC)/                         { ld (Runtime(PC)),hl }
    $B7/                                     { or a }
    $C8/                                     { ret z }
    $6F/                                     { ld l,a }
    $26/$00/                                 { ld h,$00 }
    $22/Runtime(NativeError)/                { ld (Runtime(NativeError)),hl }
    $3E/$F0/                                 { ld a,$F0 }
    $C3/Runtime(IOFail)                      { jp Runtime(IOFail) }
  )
end;

function ReadKey: Char; native; callerio;
begin
  Inline(
    $2A/Runtime(PC)/                         { ld hl,(Runtime(PC)) }
    $22/*+7/                                 { ld (*+7),hl }
    $CD/Runtime(Guard)/                      { call Runtime(Guard) }
    $00/$02/                                 { dw stack reservation in bytes }
    $00/$00/                                 { dw caller source PC (patched before call) }
    $DD/$E5/                                 { push ix }
    $DD/$21/$00/$00/                         { ld ix,$0000 }
    $DD/$39/                                 { add ix,sp }
    $32/*+16/                                { ld (*+16),a }
    $21/$00/$00/                             { ld hl,$0000 }
    $E5/                                     { push hl }
    $21/$00/$00/                             { ld hl,$0000 }
    $39/                                     { add hl,sp }
    $EB/                                     { ex de,hl }
    $CD/Runtime(IOCall)/                     { call Runtime(IOCall) }
    *+11/                                    { dw operation entry }
    $01/                                     { db I/O checking policy }
    $E1/                                     { pop hl }
    $DD/$F9/                                 { ld sp,ix }
    $DD/$E1/                                 { pop ix }
    $C1/                                     { pop bc }
    $C5/                                     { push bc }
    $C9/                                     { ret }
    $3A/Runtime(IOStatus)/                   { ld a,(Runtime(IOStatus)) }
    $B7/                                     { or a }
    $C2/Runtime(IOFail)/                     { jp nz,Runtime(IOFail) }
    $3A/Runtime(EchoFD)/                     { ld a,(Runtime(EchoFD)) }
    $F5/                                     { push af }
    $3A/Runtime(InputModeValue)/             { ld a,(Runtime(InputModeValue)) }
    $F5/                                     { push af }
    $3E/$FF/                                 { ld a,$FF }
    $32/Runtime(EchoFD)/                     { ld (Runtime(EchoFD)),a }
    $CD/Runtime(IOCall)/                     { call Runtime(IOCall) }
    *+33/                                    { dw operation entry }
    $00/                                     { db I/O checking policy }
    $21/$01/$00/                             { ld hl,$0001 }
    $39/                                     { add hl,sp }
    $5E/                                     { ld e,(hl) }
    $16/$00/                                 { ld d,$00 }
    $CD/Runtime(InputMode)/                  { call Runtime(InputMode) }
    $47/                                     { ld b,a }
    $D1/                                     { pop de }
    $D1/                                     { pop de }
    $7A/                                     { ld a,d }
    $32/Runtime(EchoFD)/                     { ld (Runtime(EchoFD)),a }
    $3A/Runtime(IOStatus)/                   { ld a,(Runtime(IOStatus)) }
    $B7/                                     { or a }
    $C2/Runtime(IOFail)/                     { jp nz,Runtime(IOFail) }
    $78/                                     { ld a,b }
    $B7/                                     { or a }
    $C2/Runtime(IOError)/                    { jp nz,Runtime(IOError) }
    $C9/                                     { ret }
    $11/$00/$00/                             { ld de,$0000 }
    $CD/Runtime(InputMode)/                  { call Runtime(InputMode) }
    $B7/                                     { or a }
    $C2/Runtime(InputFail)/                  { jp nz,Runtime(InputFail) }
    $CD/Runtime(InputKey)/                   { call Runtime(InputKey) }
    $B7/                                     { or a }
    $CA/*-5/                                 { jp z,*-5 }
    $DD/$77/$FE/                             { ld (ix-2),a }
    $C9                                      { ret }
  )
end;

end.
