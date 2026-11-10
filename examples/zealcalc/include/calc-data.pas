var
  frames: array[1..MaxFrames] of ParseFrame;
  entry: DiskCell; job: CalcRequest;
  device, viewdevice, level, nesting, visits, issue, records: integer;
  { Numeric token conversion has no recursive calls while these are live. }
  numberstart, numbercode: integer;
  exitstatus: byte; labelcell: boolean;
