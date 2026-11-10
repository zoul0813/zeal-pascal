{$R-}
{ Frame and string indices are checked at entry and bounded by the parser.
  Precedence climbing shares one recursive parser across every operator. }
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
begin
  with frames[level] do
    if pos > Length(text) then Current := #0
    else Current := UpCase(text[pos])
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

function Evaluate(r, c: integer): real; forward;
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
      entry.text := Copy(frames[level].text, numberstart, frames[level].pos - numberstart);
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
function Evaluate;
var value: real; code, i: integer;
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
      row := r; col := c; LoadCell(r, c); text := entry.text;
      if text <> '' then
        if text[1] = '=' then
        begin
          pos := 2; value := Expression(1); Spaces;
          if Current <> #0 then Error(ErrSyntax)
        end
        else
        begin
          Val(text, value, code);
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
        if entry.text[1] = '=' then value := Evaluate(r, c) else value := Bounded(value);
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
