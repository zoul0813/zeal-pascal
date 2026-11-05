{
  CONCORDANCE - Word frequencies from a text file.

  Keeps up to 64 distinct, case-insensitive words. Words longer than
  20 letters share their first 20 letters; limit warnings flag partial
  counts. The input file is never modified.
}

program Concordance;

{$R+}
{$X-}

const
  limit = 64;

type
  entry = record
    word: string[20];
    hits: integer
  end;

var
  table: array[1..limit] of entry;
  source: text;
  path: string[127];
  token: string[20];
  ch: char;
  used, i, j, best, status: integer;
  words, characters, lines: real;
  inword, overflow, longword, any, lastlf: boolean;

{ Count this token once. Linear lookup is sufficient for 64 entries;
  a full table still counts words, but cannot add another distinct word. }
procedure Store;
var
  k: integer;
  found: boolean;

begin
  words := words + 1.0;
  k := 1;
  found := False;
  while (k <= used) and not found do
  begin
    if table[k].word = token then
      found := True
    else
      k := k + 1
  end;
  if found then
  begin
    if table[k].hits < 30000 then
      table[k].hits := table[k].hits + 1
    else
      overflow := True
  end
  else if used < limit then
  begin
    used := used + 1;
    table[used].word := token;
    table[used].hits := 1
  end
  else
    overflow := True;
  token := '';
  inword := False
end;

begin
  WriteLn('Concordance - count the words in a text file.');
  Write('File [sample.txt]> ');
  if Eof(Input) then
    Halt;
  ReadLn(Input, path);
  if path = '' then
    path := 'sample.txt';

  { Check file status explicitly; console input above uses normal I/O checks. }
  {$I-}
  Assign(source, path);
  status := IOResult;
  if status = 0 then
  begin
    Reset(source);
    status := IOResult
  end;
  if status <> 0 then
  begin
    WriteLn('Cannot open file: ', status);
    Halt
  end;
  used := 0;
  words := 0.0;
  characters := 0.0;
  lines := 0.0;
  token := '';
  inword := False;
  overflow := False;
  longword := False;
  any := False;
  lastlf := False;
  while not Eof(source) do
  begin
    Read(source, ch);
    status := IOResult;
    if status <> 0 then
    begin
      WriteLn('Read error: ', status);
      Close(source);
      Halt
    end;
    any := True;
    characters := characters + 1.0;
    lastlf := ch = #10;
    if lastlf then
      lines := lines + 1.0;
    ch := UpCase(ch);
    if (ch >= 'A') and (ch <= 'Z') then
    begin
      inword := True;
      if Length(token) < 20 then
        token := token + ch
      else
        longword := True
    end
    else if inword then
      Store
  end;
  status := IOResult;
  if status <> 0 then
  begin
    WriteLn('Read error: ', status);
    Close(source);
    Halt
  end;
  if inword then
    Store;
  if any and not lastlf then
    lines := lines + 1.0;
  Close(source);
  status := IOResult;
  if status <> 0 then
  begin
    WriteLn('Close error: ', status);
    Halt
  end;
  WriteLn('Lines: ', lines:0:0, '  Words: ', words:0:0,
          '  Characters: ', characters:0:0);
  WriteLn('Most frequent words (alphabetical ties):');

  { Repeatedly select the largest remaining count. Alphabetical order
    resolves ties; zeroing the selected count prevents printing it twice. }
  for j := 1 to 10 do
  begin
    best := 0;
    for i := 1 to used do
      if table[i].hits > 0 then
      begin
        if best = 0 then
          best := i
        else if table[i].hits > table[best].hits then
          best := i
        else if table[i].hits = table[best].hits then
          if table[i].word < table[best].word then
            best := i
      end;
    if best <> 0 then
    begin
      WriteLn(table[best].word:20, ' ', table[best].hits:5);
      table[best].hits := 0
    end
  end;
  if overflow then
    WriteLn('Table/count limit reached: frequencies are partial.');
  if longword then
    WriteLn('Words longer than 20 letters share their first 20 letters.')
end.
