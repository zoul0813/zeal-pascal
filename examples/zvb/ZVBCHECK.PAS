program ZVBCheck;
uses ZVB, Crt;
type TMessage = string[60];
const Digits: array[1..9] of Char = '123456789';
var Source, Destination: array[0..36] of Byte;
  Samples: array[0..63] of Byte;
  Descriptor: TDMADescriptor;
  Config: TDMADescriptorConfig;
  SampleConfig: TSoundSamplesConfig;
  CRC: LongWord;
  SourceAddress, DestinationAddress, Left, Chunk: Word;
  I: Integer; Error: Byte;

procedure Require(Condition: Boolean; Message: TMessage);
begin
  if not Condition then begin
    SoundSetHold($FF, True);
    SoundSetVolume(SoundVol0);
    WriteLn('FAIL: ', Message);
    Halt
  end
end;

begin
  Require(SizeOf(TDMADescriptor) = 12, 'DMA descriptor layout');
  Require(SizeOf(TDMADescriptorConfig) = 11, 'DMA configuration layout');
  Require(SizeOf(TGfxSprite) = 8, 'Sprite layout');
  CRCInitialize(True);
  CRC := CRCUpdate(Digits, 4);
  SoundInitialize(True);
  SoundSetChannels(SoundVoice0 or SoundSampleTable, SoundVoice0 or SoundSampleTable);
  SoundSetVolume(SoundVol50);
  CRC := CRCUpdate(Digits[5], 5);
  Require(CRC = $CBF43926, 'CRC32 incremental result after sound calls');
  WriteLn('CRC32 OK: ', CRC);

  for I := 0 to 36 do begin
    Source[I] := I xor $A5;
    Destination[I] := 0
  end;
  SourceAddress := Word(Addr(Source));
  DestinationAddress := Word(Addr(Destination));
  Left := 37;
  while Left <> 0 do begin
    Chunk := 16384 - (SourceAddress and $3FFF);
    if Chunk > 16384 - (DestinationAddress and $3FFF) then
      Chunk := 16384 - (DestinationAddress and $3FFF);
    if Chunk > Left then Chunk := Left;
    Config.ReadAddress := DMAVirtToPhys(Mem[SourceAddress]);
    Config.WriteAddress := DMAVirtToPhys(Mem[DestinationAddress]);
    Config.Length := Chunk;
    Config.Flags := DMADescLast;
    Error := DMAPrepareDescriptor(Descriptor, Config);
    Require(Error = 0, 'Prepare DMA descriptor');
    Error := DMAStartTransfer(Descriptor);
    Require(Error = 0, 'Start DMA copy');
    Left := Left - Chunk;
    SourceAddress := SourceAddress + Chunk;
    DestinationAddress := DestinationAddress + Chunk
  end;
  for I := 0 to 36 do Require(Destination[I] = Source[I], 'DMA copied bytes');
  WriteLn('DMA OK: 37 bytes copied');

  SoundSetVoicesVol(SoundVoice0, SoundVol50);
  SoundSetVoices(SoundVoice0, SoundFrequencyToDivider(440), SoundTriangle);
  SoundSetHold(SoundVoice0, False);
  Delay(200);
  SoundSetHold(SoundVoice0, True);
  for I := 0 to 63 do Samples[I] := (I mod 32) * 8;
  SampleConfig.Mode := SoundSampleUInt8;
  SampleConfig.Divider := 7;
  SoundPlaySamples(SampleConfig, Samples, SizeOf(Samples));
  SoundSetHold($FF, True);
  SoundSetVolume(SoundVol0);
  WriteLn('SOUND OK: triangle tone and sample FIFO completed')
end.
