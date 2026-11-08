{ SPDX-FileCopyrightText: 2024-2026 Zeal 8-bit Computer <contact@zeal8bit.com>
  SPDX-License-Identifier: Apache-2.0
  Pascal rewrite; see ZVB-NOTICE.txt for source and changes. }
{ Pascal hardware implementation, referenced against the public ZVB SDK.
  No C calling convention or C runtime is used. Inline leaves preserve IFF.
  The IX offsets are the compiler's ordinary Pascal argument stack layout. }

procedure WritePeripheral(Device, Register, Value: Byte);
begin
{ LD A,I / save flags / DI; select mapper; OUT (C),A; restore IFF2. }
  Inline($ED/$57/$F5/$F3/$DD/$7E/$08/$D3/$8E/$DD/$4E/$06/
    $06/$00/$DD/$7E/$04/$ED/$79/$C1/$CB/$51/$28/$01/
    $FB)
end;

procedure ReadPeripheralRaw(Device, Register: Byte; var Value: Byte);
begin
  Inline($ED/$57/$F5/$F3/$DD/$7E/$08/$D3/$8E/$DD/$4E/$06/
    $06/$00/$DD/$6E/$04/$DD/$66/$05/$ED/$78/$77/$C1/
    $CB/$51/$28/$01/$FB)
end;

function ReadPeripheral(Device, Register: Byte): Byte;
var Value: Byte;
begin
  ReadPeripheralRaw(Device, Register, Value);
  ReadPeripheral := Value
end;

procedure CRCFeed(var Buffer; Size: Word; var Value: LongWord);
begin
  { SDK OTIR chunks: low-byte count first, then full 256-byte blocks.
    Count zero reads only; a wrapped loop count represents 256 blocks. }
  Inline($DD/$6E/$08/$DD/$66/$09/$DD/$4E/$06/$DD/$46/$07/$DD/$5E/$04/$DD/
    $56/$05/$ED/$57/$F5/$F3/$3E/$02/$D3/$8E/$78/$B1/$28/$0E/$78/$41/
    $0E/$A1/$05/$04/$28/$01/$3C/$ED/$B3/$3D/$20/$FB/$DB/$A4/$12/$13/
    $DB/$A5/$12/$13/$DB/$A6/$12/$13/$DB/$A7/$12/$C1/$CB/$51/$28/$01/
    $FB)
end;


procedure CRCInitialize(Reset: Boolean);
var Control: Byte;
begin
  if Reset then WritePeripheral(2, $A0, 1)
  else Control := ReadPeripheral(2, $A0)
end;

procedure CRCReset;
begin
  CRCInitialize(True)
end;

function CRCUpdate(var Buffer; Size: Word): TCRC32;
var Value: LongWord;
begin
  CRCFeed(Buffer, Size, Value);
  CRCUpdate := Value
end;

function SoundFrequencyToDivider(Frequency: Word): Word;
begin
  SoundFrequencyToDivider := Word((LongWord(Frequency) shl 16) div 44091)
end;

procedure SoundSetHold(Voices: TSoundVoices; Hold: Boolean);
begin
  Inline($ED/$57/$F5/$F3/$3E/$03/$D3/$8E/$DB/$AD/$4F/$DD/$7E/$04/$B7/$28/
    $08/$DD/$7E/$06/$B1/$D3/$AD/$18/$07/$DD/$7E/$06/$2F/$A1/$D3/$AD/
    $C1/$CB/$51/$28/$01/$FB)
end;


procedure SoundSetVoices(Voices: TSoundVoices; Divider: Word;
  Waveform: TSoundWaveform);
begin
  { One device selection and the SDK register sequence; preserve prior holds. }
  Inline($ED/$57/$F5/$F3/$3E/$03/$D3/$8E/$DB/$AD/$47/$DD/$4E/$08/$B1/$D3/
    $AD/$79/$D3/$AF/$DD/$7E/$06/$D3/$A0/$DD/$7E/$07/$D3/$A1/$DD/$7E/
    $04/$D3/$A2/$78/$D3/$AD/$C1/$CB/$51/$28/$01/$FB)
end;


procedure SoundSetVoicesVol(Voices: TSoundVoices; Volume: TSoundVolume);
begin
  Inline($ED/$57/$F5/$F3/$3E/$03/$D3/$8E/$DD/$7E/$06/$D3/$AF/$DD/$7E/$04/
    $D3/$A3/$C1/$CB/$51/$28/$01/$FB)
end;


procedure SoundSetChannels(LeftVoices, RightVoices: TSoundVoices);
begin
  Inline($ED/$57/$F5/$F3/$3E/$03/$D3/$8E/$DD/$7E/$06/$D3/$AB/$DD/$7E/$04/
    $D3/$AC/$C1/$CB/$51/$28/$01/$FB)
end;


procedure SoundSetVolumes(Left, Right: TSoundVolume);
var Value: Byte;
begin
  if Left = SoundVol0 then Value := $40 else Value := Left and 3;
  if Right = SoundVol0 then Value := Value or $80
  else Value := Value or ((Right and 3) shl 2);
  WritePeripheral(3, $AE, Value)
end;

procedure SoundSetVolume(Volume: TSoundVolume);
begin
  SoundSetVolumes(Volume, Volume)
end;

procedure SoundInitialize(Reset: Boolean);
var Held: Byte;
begin
  if Reset then begin
    SoundSetHold($FF, True);
    SoundSetVoicesVol(15, SoundVol100);
    SoundSetChannels(15, 15);
    SoundSetVolume(SoundVol0);
    WritePeripheral(3, $AF, 0)
  end else Held := ReadPeripheral(3, $AD)
end;

procedure SoundReset;
begin
  SoundInitialize(True)
end;

procedure SoundPlaySamples(var Config: TSoundSamplesConfig;
  var Samples; Length: Word);
var Address: Word;
begin
  if (Length = 0) or (Config.Mode > SoundSampleSInt16) then Exit;
  if (Config.Mode <> SoundSampleUInt8) and Odd(Length) then Exit;
  Address := Word(Addr(Samples));
  if LongWord(Address) + Length > 65536 then Exit;
  { SDK FIFO process: configure once, stream while not full, drain, restore hold. }
  Inline($ED/$57/$F5/$F3/$3E/$03/$D3/$8E/$3E/$80/$D3/$AF/$DD/$6E/$08/$DD/
    $66/$09/$7E/$B7/$20/$04/$3E/$01/$18/$08/$FE/$02/$3E/$00/$20/$02/
    $3E/$04/$D3/$A2/$23/$7E/$D3/$A1/$DB/$AD/$5F/$E6/$7F/$D3/$AD/$DD/
    $6E/$06/$DD/$66/$07/$DD/$4E/$04/$DD/$46/$05/$DB/$A2/$CB/$77/$20/
    $FA/$7E/$D3/$A0/$23/$0B/$78/$B1/$20/$F1/$DB/$A2/$CB/$7F/$28/$FA/
    $7B/$D3/$AD/$C1/$CB/$51/$28/$01/$FB)
end;

function DMAVirtToPhys(var Buffer): TPhysicalAddress;
var Address: Word; Page: Byte;
begin
  Address := Word(Addr(Buffer));
  Page := Port[(Address and Word($C000)) or $F0];
  DMAVirtToPhys := (LongWord(Page) shl 14) or (Address and $3FFF)
end;

function DMASetRead(var Descriptor: TDMADescriptor;
  Address: TPhysicalAddress): TDMAError;
begin
  Descriptor.ReadAddressLow := Word(Address);
  Descriptor.ReadAddressHigh := Byte(Address shr 16);
  DMASetRead := 0
end;

function DMASetWrite(var Descriptor: TDMADescriptor;
  Address: TPhysicalAddress): TDMAError;
begin
  Descriptor.WriteAddressLow := Word(Address);
  Descriptor.WriteAddressHigh := Byte(Address shr 16);
  DMASetWrite := 0
end;

function DMASetReadVirt(var Descriptor: TDMADescriptor; var Buffer): TDMAError;
begin
  DMASetReadVirt := DMASetRead(Descriptor, DMAVirtToPhys(Buffer))
end;

function DMASetWriteVirt(var Descriptor: TDMADescriptor; var Buffer): TDMAError;
begin
  DMASetWriteVirt := DMASetWrite(Descriptor, DMAVirtToPhys(Buffer))
end;

function DMAPrepareDescriptor(var Descriptor: TDMADescriptor;
  var Config: TDMADescriptorConfig): TDMAError;
var Error: Byte;
begin
  Error := DMASetRead(Descriptor, Config.ReadAddress);
  Error := DMASetWrite(Descriptor, Config.WriteAddress);
  Descriptor.Length := Config.Length;
  Descriptor.Flags := Config.Flags;
  Descriptor.Reserved[0] := 0;
  Descriptor.Reserved[1] := 0;
  Descriptor.Reserved[2] := 0;
  DMAPrepareDescriptor := Error
end;

function DMAStartTransfer(var Descriptor: TDMADescriptor): TDMAError;
begin
  if (Word(Addr(Descriptor)) and $3FFF) > 16372 then begin
    DMAStartTransfer := GfxInvalidArg;
    Exit
  end;
  { Convert the descriptor address and select/write/start DMA once, as the SDK. }
  Inline($ED/$57/$F5/$F3/$DD/$6E/$04/$DD/$66/$05/$5D/$7C/$E6/$3F/$57/$7C/
    $DB/$F0/$0F/$0F/$67/$E6/$C0/$B2/$57/$7C/$E6/$3F/$6F/$3E/$04/$D3/
    $8E/$7B/$D3/$A1/$7A/$D3/$A2/$7D/$D3/$A3/$3E/$80/$D3/$A0/$C1/$CB/
    $51/$28/$01/$FB);
  DMAStartTransfer := 0
end;
