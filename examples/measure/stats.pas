{
  MEASUREMENT REPORT - Streaming statistics for Measure Lab.

  Reads values.dat without changing it, then transfers control back
  to measure.bin. Only the running mean and squared deviations are
  needed; the complete batch is not kept in memory.
}

program MeasurementReport;

{$R+}
{$I-}

var
  samples: file of real;
  next: file;
  count, i, status, cleanup: integer;
  value, average, m2, delta, low, high, deviation: real;
  opened: boolean;

{ Preserve the original I/O status while attempting file cleanup. }
procedure Checked;
var
  error: integer;

begin
  error := IOResult;
  if error <> 0 then
  begin
    if opened then
    begin
      Close(samples);
      cleanup := IOResult;
      opened := False
    end;
    WriteLn('Report I/O error ', error);
    Halt
  end
end;

begin
  opened := False;
  Assign(samples, 'values.dat');
  Checked;
  Reset(samples);
  status := IOResult;
  if status <> 0 then
    WriteLn('Cannot read values.dat: ', status, '. Use N to collect a batch.')
  else
  begin
    opened := True;
    count := FileSize(samples);
    Checked;
    if (count < 0) or (count > 100) then
    begin
      Close(samples);
      status := IOResult;
      WriteLn('Invalid batch size.');
      Halt
    end;

    { Empty batches have no statistics; a single sample has deviation zero. }
    average := 0.0;
    m2 := 0.0;
    low := 0.0;
    high := 0.0;
    for i := 1 to count do
    begin
      Read(samples, value);
      Checked;
      if Abs(value) > 1000000.0 then
      begin
        Close(samples);
        status := IOResult;
        WriteLn('Invalid measurement.');
        Halt
      end;
      if i = 1 then
      begin
        low := value;
        high := value
      end;
      if value < low then
        low := value;
      if value > high then
        high := value;

      { Welford's streaming algorithm avoids subtracting two large sums. }
      delta := value - average;
      average := average + delta / i;
      m2 := m2 + delta * (value - average)
    end;
    Close(samples);
    Checked;
    opened := False;
    Write('Measurement report: ', count);
    if count = 1 then
      WriteLn(' sample.')
    else
      WriteLn(' samples.');
    if count > 0 then
    begin
      deviation := 0.0;
      if (count > 1) and (m2 > 0.0) then
        deviation := Sqrt(m2 / (count - 1));
      WriteLn('Mean: ', average:0:2);
      WriteLn('Minimum: ', low:0:2, '  Maximum: ', high:0:2);
      WriteLn('Sample standard deviation: ', deviation:0:2)
    end;
  end;
  WriteLn('Returning to Measure Lab...');

  { The collector starts afresh; the typed file is the shared state.
    Successful Execute never returns to this Pascal caller. }
  Assign(next, 'measure.bin');
  Checked;
  Execute(next);
  status := IOResult;
  WriteLn('Could not return to measure.bin: ', status)
end.
