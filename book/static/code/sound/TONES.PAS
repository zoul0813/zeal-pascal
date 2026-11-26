program BeaconTones;

uses ZVB, Crt;

type Notes = array[1..4] of Word;
var Tune: Notes;
    Step: Integer;
    Key: Char;

procedure Note(Frequency: Word);
begin
  SoundSetVoices(SoundVoice0, SoundFrequencyToDivider(Frequency), SoundTriangle);
  SoundSetHold(SoundVoice0, False);
  Delay(220);
  SoundSetHold(SoundVoice0, True);
  Delay(70)
end;

begin
  WriteLn('Enable workbench sound. Press p to play, q to quit.');
  Tune[1] := 330;
  Tune[2] := 440;
  Tune[3] := 523;
  Tune[4] := 660;
  SoundInitialize(True);
  SoundSetChannels(SoundVoice0, SoundVoice0);
  SoundSetVolume(SoundVol25);
  repeat
    Key := ReadKey;
    if Key = 'p' then
    begin
      for Step := 1 to 4 do
      begin
        WriteLn('Note ', Step);
        Note(Tune[Step])
      end;
      WriteLn('p plays again, q quits.')
    end
  until Key = 'q';
  SoundSetHold(SoundVoice0, True);
  SoundSetVolume(SoundVol0)
end.
