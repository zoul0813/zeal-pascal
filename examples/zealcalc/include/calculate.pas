overlay procedure Calculate;
var frames: array[1..MaxFrames] of ParseFrame;
  level, nesting, visits, issue, numberstart, numbercode: integer;
  wire: array[0..3] of byte; complete: word;
  r, c, i: integer; offset: longword;
{$R-}
function Checksum: word;
var i: integer; total: word;
begin
  total := entry.row + entry.col + entry.previous + Length(entry.text);
  for i := 1 to Length(entry.text) do total := total + Ord(entry.text[i]);
  Checksum := total
end;

{$R+}
{$R-}
{ Read-only typed-sheet payload through checked ZOS I/O. The save overlay has
  already validated every record; the calculator still checks each fetched record. }
procedure Fatal(code: integer);
begin BackendFailure(code) end;

{ Centralize descriptor lifecycle and syscall staging in this overlay. }
procedure CloseDevice(var fd: integer);
var status: integer;
begin
  status := ZOS(ZOS_CLOSE, fd); fd := -1;
  if status <> 0 then Fatal(36)
end;
procedure SeekDevice(fd: integer; offset: longword);
var status: integer;
begin
  status := ZOS(ZOS_SEEK, fd, offset, ZOS_SEEK_SET);
  if status <> 0 then Fatal(3)
end;
function OpenRead: integer;
begin
  OpenRead := ZOS(ZOS_OPEN, job.path, ZOS_O_RDONLY)
end;

procedure Transfer(fd: integer; var buffer; size: word; writing: boolean);
var amount: word; status: integer;
begin
  amount := size;
  if writing then status := ZOS(ZOS_WRITE, fd, buffer, amount)
  else status := ZOS(ZOS_READ, fd, buffer, amount);
  if (status <> 0) or (amount <> size) then Fatal(31)
end;

procedure ReadRecord(index: integer);
var offset: longword; i: integer;
begin
  if (index < 0) or (index >= records) then Fatal(19);
  offset := index; offset := offset * 70 + 4;
  SeekDevice(device, offset);
  Transfer(device, entry, SizeOf(entry), False);
  if Length(entry.text) > 63 then Fatal(5);
  if entry.check <> Checksum then Fatal(6);
  for i := 1 to Length(entry.text) do
    if (entry.text[i] < ' ') or (entry.text[i] > '~') then Fatal(7)
end;

{$R+}
{$R-}
{ Walk one row's validated disk history, retaining just the requested source. }
procedure LoadCell(r, c: integer);
var at: integer;
begin
  if (r < 1) or (r > MaxRows) or (c < 1) or (c > MaxCols) then Fatal(19);
  at := job.heads[r];
  while at <> 0 do
  begin
    ReadRecord(at);
    if (entry.row <> r) or (entry.previous >= at) then Fatal(20);
    if entry.col = c then Exit;
    at := entry.previous
  end;
  entry.text := ''
end;
{$R+}
{$R-}
{ Frame and string indices are checked at entry and bounded by the parser.
  Precedence climbing shares one recursive parser across every operator. }
overlay function Evaluate(r, c: integer): real;
var value: real; code, i: integer;
procedure Error(code: integer);
begin
  if issue = 0 then issue := code
end;

function Bounded(value: real): real;
begin
  if Abs(value) > 1000000000.0 then
  begin Error(ErrRange); value := 0.0 end;
  Bounded := value
end;

function Current: char;
var key: char;
begin
  with frames[level] do
    if pos > chars then Current := #0
    else
    begin
      CacheCopy(2580 + (level - 1) * 64 + pos, key, 1, True);
      Current := UpCase(key)
    end
end;

procedure Advance;
begin
  frames[level].pos := frames[level].pos + 1
end;

procedure Spaces;
begin
  while Current = ' ' do Advance
end;

procedure Address(var r, c: integer);
begin
  Spaces; c := 0; r := 0;
  if (Current >= 'A') and (Current <= 'Z') then
  begin
    c := Ord(Current) - Ord('A') + 1; Advance;
    while (Current >= '0') and (Current <= '9') do
    begin
      if r > 254 then Error(ErrRef);
      if issue <> 0 then Exit;
      r := r * 10 + Ord(Current) - Ord('0'); Advance
    end
  end;
  if (r < 1) or (r > MaxRows) or (c = 0) then Error(ErrRef)
end;

{$A-}
function Expression(minimum: integer): real;
var value, right: real; op: char; precedence: integer;
  r, c, r2, c2, rr, cc: integer; done: boolean;
begin
  value := 0.0; Spaces; nesting := nesting + 1;
  if nesting > 16 then Error(ErrDepth);
  if issue = 0 then
  begin
    op := Current;
    if (op = '+') or (op = '-') then
    begin
      Advance; value := Expression(3);
      if op = '-' then value := -value
    end
    else if op = '(' then
    begin
      Advance; value := Expression(1); Spaces;
      if Current <> ')' then Error(ErrSyntax) else Advance
    end
    else if op = '@' then
    begin
      Advance;
      if Current <> 'S' then Error(ErrSyntax); Advance;
      if Current <> 'U' then Error(ErrSyntax); Advance;
      if Current <> 'M' then Error(ErrSyntax); Advance;
      Spaces;
      if Current <> '(' then Error(ErrSyntax) else Advance;
      if issue = 0 then Address(r, c);
      Spaces;
      if Current <> ':' then Error(ErrSyntax) else Advance;
      if issue = 0 then Address(r2, c2);
      Spaces;
      if Current <> ')' then Error(ErrSyntax) else Advance;
      if issue = 0 then
        if (r2 < r) or (c2 < c) then Error(ErrRef)
        else if (r2 - r + 1) * (c2 - c + 1) > 256 then Error(ErrRange)
        else
          for rr := r to r2 do
            for cc := c to c2 do
              if issue = 0 then
              begin right := Evaluate(rr, cc); value := Bounded(value + right) end
    end
    else if (op >= 'A') and (op <= 'Z') then
    begin
      Address(r, c);
      if issue = 0 then value := Evaluate(r, c)
    end
    else if ((op >= '0') and (op <= '9')) or (op = '.') then
    begin
      numberstart := frames[level].pos;
      while ((Current >= '0') and (Current <= '9')) or (Current = '.') do Advance;
      { Numeric conversion finishes before any recursive call. Reuse the disk
        buffer; active sources already live in frames, not entry. }
      CacheCopy(2580 + (level - 1) * 64, entry.text, 64, True);
      entry.text := Copy(entry.text, numberstart, frames[level].pos - numberstart);
      if entry.text = '.' then Error(ErrSyntax)
      else
      begin
        Val(entry.text, value, numbercode);
        if numbercode <> 0 then Error(ErrSyntax) else value := Bounded(value)
      end
    end
    else Error(ErrSyntax)
  end;
  done := False;
  while (issue = 0) and not done do
  begin
    Spaces; op := Current; precedence := 0;
    if (op = '+') or (op = '-') then precedence := 1;
    if (op = '*') or (op = '/') then precedence := 2;
    if precedence < minimum then done := True
    else
    begin
      Advance; right := Expression(precedence + 1);
      if issue = 0 then
      begin
        case op of
          '+': value := value + right;
          '-': value := value - right;
          '*': value := value * right;
          '/':
            if right = 0.0 then Error(ErrDiv)
            else if Abs(right) < 0.000000001 then Error(ErrRange)
            else value := value / right
        end;
        value := Bounded(value)
      end
    end
  end;
  nesting := nesting - 1; Expression := value
end;

{$A+}
{ No scalar workspace survives a recursive Expression call: coordinates/source
  already belong to frames[level], and value is assigned only after it returns.
  Keep recursion storage in Expression, whose left operand and SUM loop do live
  across those calls. }
begin
  value := 0.0;
  if level = 0 then visits := 0;
  visits := visits + 1;
  if visits > 2048 then Error(ErrLimit);
  for i := 1 to level do
    if (frames[i].row = r) and (frames[i].col = c) then Error(ErrCycle);
  if level = MaxFrames then Error(ErrDepth);
  if issue = 0 then
  begin
    level := level + 1;
    with frames[level] do
    begin
      row := r; col := c; LoadCell(r, c); chars := Length(entry.text);
      CacheCopy(2580 + (level - 1) * 64, entry.text, 64, False);
      if chars <> 0 then
        if entry.text[1] = '=' then
        begin
          pos := 2; value := Expression(1); Spaces;
          if Current <> #0 then Error(ErrSyntax)
        end
        else
        begin
          Val(entry.text, value, code);
          if code <> 0 then value := 0.0 else value := Bounded(value)
        end
    end;
    level := level - 1
  end;
  Evaluate := value
end;
{$A+}

{ Rendering owns the shared entry buffer after Evaluate has returned. Sources
  needed by recursive calls have already been copied into their bounded frames. }
procedure RenderCell;
var value: real; code, i, start: integer;
begin
  LoadCell(r, c); job.reply.visible.source := entry.text;
  labelcell := True;
  if entry.text <> '' then
    if entry.text[1] <> '''' then
    begin
      Val(entry.text, value, code);
      if (entry.text[1] = '=') or (code = 0) then
      begin
        labelcell := False; issue := 0; level := 0; nesting := 0;
        value := Evaluate(r, c);
        if issue <> 0 then entry.text := ErrorNames[issue]
        else Str(value:0:2, entry.text);
        if Length(entry.text) > 9 then job.reply.visible.shown := '>>>>>>>>>'
        else job.reply.visible.shown := entry.text
      end
    end;
  if labelcell then
  begin
    job.reply.visible.shown := '         '; start := 1; i := 1;
    if Length(job.reply.visible.source) > 0 then
      if job.reply.visible.source[1] = '''' then start := 2;
    while (i <= 9) and (start <= Length(job.reply.visible.source)) do
    begin
      job.reply.visible.shown[i] := job.reply.visible.source[start];
      i := i + 1; start := start + 1
    end
  end;
  job.reply.visible.labelcell := labelcell
end;
{$R+}
begin
  { Keep checked ZOS calls below the CPU stack even at the combined parser/
    dependency limit. Expression workspaces grow down below this boundary. }
  RecurPtr := StackPtr - 3200;
  records := job.records;
  for i := 1 to MaxRows do
    if job.heads[i] >= records then Fatal(34);
  device := OpenRead;
  if device < 0 then Fatal(2);
  Transfer(device, wire, 4, False);
  if (wire[0] + wire[1] * 256 <> records) or (wire[2] <> 70) or (wire[3] <> 0) then Fatal(34);
  outputdevice := ZOS(ZOS_OPEN, ViewName, ZOS_O_WRONLY);
  if outputdevice < 0 then Fatal(35);
  job.reply.header.tag := 0; job.reply.header.generation := job.generation;
  job.reply.header.top := job.top; job.reply.header.left := job.left;
  job.reply.header.records := records; job.reply.header.notice := notice;
  Transfer(outputdevice, job.reply.header, SizeOf(job.reply.header), True);
  for r := job.top to job.top + ViewRows - 1 do
    for c := job.left to job.left + ViewCols - 1 do
    begin
      RenderCell; Transfer(outputdevice, job.reply.visible, SizeOf(job.reply.visible), True)
    end;
  CloseDevice(device);
  offset := 0; SeekDevice(outputdevice, offset);
  complete := WireTag; Transfer(outputdevice, complete, SizeOf(complete), True);
  CloseDevice(outputdevice);
end;

