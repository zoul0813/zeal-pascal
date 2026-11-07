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
{ Buffer is IX+8, Size IX+6, result pointer IX+4. No OS call while DI. }
  Inline($DD/$6E/$08/$DD/$66/$09/$DD/$4E/$06/$DD/$46/$07/
    $DD/$5E/$04/$DD/$56/$05/$ED/$57/$F5/$F3/$3E/$02/
    $D3/$8E/$78/$B1/$28/$07/$7E/$D3/$A1/$23/$0B/$18/
    $F5/$DB/$A4/$12/$13/$DB/$A5/$12/$13/$DB/$A6/$12/
    $13/$DB/$A7/$12/$13/$C1/$CB/$51/$28/$01/$FB)
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
var Value: Byte;
begin
  Value := ReadPeripheral(3, $AD);
  if Hold then Value := Value or Voices
  else Value := Value and not Voices;
  WritePeripheral(3, $AD, Value)
end;

procedure SoundSetVoices(Voices: TSoundVoices; Divider: Word;
  Waveform: TSoundWaveform);
var Held: Byte;
begin
  Held := ReadPeripheral(3, $AD);
  WritePeripheral(3, $AD, Held or Voices);
  WritePeripheral(3, $AF, Voices);
  WritePeripheral(3, $A0, Lo(Divider));
  WritePeripheral(3, $A1, Hi(Divider));
  WritePeripheral(3, $A2, Waveform);
  WritePeripheral(3, $AD, Held)
end;

procedure SoundSetVoicesVol(Voices: TSoundVoices; Volume: TSoundVolume);
begin
  WritePeripheral(3, $AF, Voices);
  WritePeripheral(3, $A3, Volume)
end;

procedure SoundSetChannels(LeftVoices, RightVoices: TSoundVoices);
begin
  WritePeripheral(3, $AB, LeftVoices);
  WritePeripheral(3, $AC, RightVoices)
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
var Address, I: Word; Held, Mode: Byte;
begin
  if (Length = 0) or (Config.Mode > SoundSampleSInt16) then Exit;
  if (Config.Mode <> SoundSampleUInt8) and Odd(Length) then Exit;
  Address := Word(Addr(Samples));
  if LongWord(Address) + Length > 65536 then Exit;
  if Config.Mode = SoundSampleUInt8 then Mode := 1
  else if Config.Mode = SoundSampleSInt16 then Mode := 4
  else Mode := 0;
  WritePeripheral(3, $AF, SoundSampleTable);
  WritePeripheral(3, $A2, Mode);
  WritePeripheral(3, $A1, Config.Divider);
  Held := ReadPeripheral(3, $AD);
  WritePeripheral(3, $AD, Held and $7F);
  I := 0;
  while I < Length do begin
    while (ReadPeripheral(3, $A2) and $40) <> 0 do begin end;
    WritePeripheral(3, $A0, Mem[Address + I]);
    I := I + 1
  end;
  while (ReadPeripheral(3, $A2) and $80) = 0 do begin end;
  WritePeripheral(3, $AD, Held)
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
var Address: LongWord;
begin
  if (Word(Addr(Descriptor)) and $3FFF) > 16372 then begin
    DMAStartTransfer := GfxInvalidArg;
    Exit
  end;
  Address := DMAVirtToPhys(Descriptor);
  WritePeripheral(4, $A1, Byte(Address));
  WritePeripheral(4, $A2, Byte(Address shr 8));
  WritePeripheral(4, $A3, Byte(Address shr 16));
  WritePeripheral(4, $A0, $80);
  DMAStartTransfer := 0
end;
