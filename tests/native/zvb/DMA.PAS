program ZVBDMA;
uses ZVB;
var A, B: array[0..36] of Byte;
  Descriptors: array[0..1] of TDMADescriptor;
  Config: TDMADescriptorConfig;
  Z, I: Integer; Error: TDMAError;
  Source, Destination, Remaining, Chunk: Word;
  Expected: LongWord; Page: Byte;
procedure Require(OK: Boolean);
begin if not OK then Z := 1 div Z end;
begin
  Z := 0;
  for I := 0 to 36 do begin A[I] := I xor $A5; B[I] := 0 end;
  Source := Word(Addr(A)); Destination := Word(Addr(B));
  Page := Port[(Source and Word($C000)) or $F0];
  Expected := (LongWord(Page) shl 14) or (Source and $3FFF);
  Require(DMAVirtToPhys(A) = Expected);
  Remaining := 37;
  while Remaining <> 0 do begin
    Chunk := 16384 - (Source and $3FFF);
    if Chunk > 16384 - (Destination and $3FFF) then
      Chunk := 16384 - (Destination and $3FFF);
    if Chunk > Remaining then Chunk := Remaining;
    Config.ReadAddress := DMAVirtToPhys(Mem[Source]);
    Config.WriteAddress := DMAVirtToPhys(Mem[Destination]);
    Config.Length := Chunk; Config.Flags := DMADescLast;
    Require(DMAPrepareDescriptor(Descriptors[0], Config) = 0);
    Require(Descriptors[0].Reserved[0] = 0);
    Require(DMAStartTransfer(Descriptors[0]) = 0);
    Source := Source + Chunk; Destination := Destination + Chunk;
    Remaining := Remaining - Chunk
  end;
  for I := 0 to 36 do Require(A[I] = B[I]);
  { Two adjacent descriptors, including a zero-length link. }
  Config.ReadAddress := DMAVirtToPhys(A);
  Config.WriteAddress := DMAVirtToPhys(B);
  Config.Length := 0; Config.Flags := 0;
  Require(DMAPrepareDescriptor(Descriptors[0], Config) = 0);
  Config.Length := 37; Config.Flags := DMADescLast;
  Require(DMAPrepareDescriptor(Descriptors[1], Config) = 0);
  FillChar(B, SizeOf(B), 0);
  Require((Word(Addr(Descriptors)) and $3FFF) <= 16360);
  Require(DMAStartTransfer(Descriptors[0]) = 0);
  for I := 0 to 36 do Require(A[I] = B[I]);
  { Full physical addresses are stored as the low 24 bits. }
  Require(DMASetRead(Descriptors[0], $AB123456) = 0);
  Require((Descriptors[0].ReadAddressLow = $3456) and
    (Descriptors[0].ReadAddressHigh = $12));
  Require(DMASetWrite(Descriptors[0], $CD987654) = 0);
  Require((Descriptors[0].WriteAddressLow = $7654) and
    (Descriptors[0].WriteAddressHigh = $98));
  Require(DMASetReadVirt(Descriptors[0], A) = 0);
  Require(DMASetWriteVirt(Descriptors[0], B) = 0)
end.
