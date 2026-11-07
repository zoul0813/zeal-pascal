{ Intent adapted from the ZVB SDK examples/crc32 (CC0-1.0).
  Raw files use supported Zeal syscalls through three Pascal Inline leaves.
  Pascal typed files have headers; untyped files use 128-byte records. }
program CRC32Files;
uses ZVB;
type TMessage = string[60];
var InputFile: Byte;
  Path: array[0..127] of Char;
  Buffer: array[0..1023] of Byte;
  Value: LongWord;
  Name: string[127];
  Count: Word;
  Argument, Index, Digit: Integer;
  Status, CloseStatus: Byte;

{ Ordinary Pascal arguments, rightmost at IX+4. RST 8 invokes Zeal OS;
  L selects the syscall. Buffers remain mapped during each supported OS call. }
procedure RawOpen(var Name; var Handle, Error: Byte);
begin
  Inline($DD/$4E/$08/$DD/$46/$09/$26/$00/$2E/$02/$CF/
    $DD/$6E/$06/$DD/$66/$07/$77/$DD/$6E/$04/$DD/$66/$05/
    $36/$00/$CB/$7F/$28/$03/$ED/$44/$77)
end;
procedure RawRead(Handle: Byte; var Buffer; var Count: Word; var Error: Byte);
begin
  Inline($DD/$6E/$06/$DD/$66/$07/$4E/$23/$46/
    $DD/$5E/$08/$DD/$56/$09/$DD/$66/$0A/$2E/$00/$CF/
    $DD/$6E/$04/$DD/$66/$05/$77/
    $DD/$6E/$06/$DD/$66/$07/$71/$23/$70)
end;
procedure RawClose(Handle: Byte; var Error: Byte);
begin
  Inline($DD/$66/$06/$2E/$03/$CF/$DD/$6E/$04/$DD/$66/$05/$77)
end;

procedure Fail(Message: TMessage);
begin
  WriteLn(Message, ': ', Name, ' (Zeal error ', Status, ')');
  Halt
end;

procedure WriteCRC(CRC: LongWord);
var Shift: Integer;
begin
  for Shift := 7 downto 0 do begin
    Digit := Integer(Word((CRC shr (Shift * 4)) and 15));
    if Digit < 10 then Write(Chr(Ord('0') + Digit))
    else Write(Chr(Ord('A') + Digit - 10))
  end
end;

begin
  if ParamCount = 0 then begin
    WriteLn('Usage: CRC32 <file> [<file> ...]');
    Halt
  end;
  for Argument := 1 to ParamCount do begin
    Name := ParamStr(Argument);
    for Index := 1 to Length(Name) do Path[Index - 1] := Name[Index];
    Path[Length(Name)] := #0;
    RawOpen(Path, InputFile, Status);
    if Status <> 0 then Fail('Cannot open file');
    CRCInitialize(True);
    repeat
      Count := SizeOf(Buffer);
      RawRead(InputFile, Buffer, Count, Status);
      if Status <> 0 then begin
        RawClose(InputFile, CloseStatus);
        Fail('Cannot read file')
      end;
      { A zero-byte update also retrieves the CRC of an empty file. }
      Value := CRCUpdate(Buffer, Count)
    until Count = 0;
    RawClose(InputFile, Status);
    if Status <> 0 then Fail('Cannot close file');
    WriteCRC(Value);
    WriteLn('    ', Name)
  end
end.
