var
  entry: DiskCell; job: CalcRequest;
  records, row, col, level, nesting, visits, issue: integer;
  path: string[127]; notice: string[75];
  exitstatus: integer;
  sheet: file of DiskCell;
  opened: boolean;
  labelcell: boolean;
