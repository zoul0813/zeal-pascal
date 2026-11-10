{$R-}
procedure PutCell(text: CellText);
begin
  LoadCell(row, col);
  if text = entry.text then begin notice := 'Unchanged.'; Exit end;
  if records > MaxEdits then notice := '4096 edits reached. Back up this file; start another sheet.'
  else
  begin
    FillChar(entry, SizeOf(entry), 0);
    entry.row := row; entry.col := col; entry.previous := job.heads[row]; entry.text := text;
    AppendRecord; job.heads[row] := records; records := records + 1;
    notice := 'Saved.'
  end
end;
{$R+}
