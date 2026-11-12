{$R-}
{ Private indices follow the viewport, frame and string bounds checked at entry. }
overlay procedure Paint;
const BlankBand = '                                                               ';
var rr, cc, x, y: integer; statusline: LineText; redraw, refreshed, moved: boolean;
{ The renderer already owns the ZVB ports and validated 80x40 coordinates. }
procedure Position(x, y: integer);
begin
  Port[$8E] := 0; Port[$A1] := y - 1; Port[$A2] := x - 1
end;

procedure Pen(inverse: boolean);
begin
  Port[$8E] := 0;
  if inverse then Port[$A5] := $F0 else Port[$A5] := $0F
end;

{ The documented ZVB text-print port advances the hardware cursor. Rendering
  printable fields directly avoids importing the general file-output engine. }
procedure Say(text: CellText);
var i: integer;
begin
  for i := 1 to Length(text) do Port[$A0] := Ord(text[i])
end;


procedure Pad(text: CellText; width: integer);
var i: integer;
begin
  for i := Length(text) + 1 to width do Port[$A0] := 32;
  Say(text)
end;




begin
  if row < top then top := row;
  if row >= top + ViewRows then top := row - ViewRows + 1;
  if col < left then left := col;
  if col >= left + ViewCols then left := col - ViewCols + 1;
  redraw := not painted or (top <> showntop) or (left <> shownleft);
  refreshed := redraw or (request.edit <> 0);
  moved := (row <> shownrow) or (col <> showncol);
  if refreshed then View(0, 0, 0);
  View(1, row, col); request.source := job.reply.visible.source; entry.text := job.reply.visible.shown;
  if job.reply.visible.labelcell then entry.text := '(L)' else entry.text := '(V)';
  Str(row, statusline);
  { Pad once using the runtime's bounded string operations. }
  entry.text := Copy(Chr(Ord('A') + col - 1) + statusline + ' ' + entry.text + ' ' +
                     request.source + BlankBand, 1, 63);
  statusline := entry.text + ' '; Str(records - 1, request.source);
  while Length(request.source) < 4 do request.source := ' ' + request.source;
  Line(1, statusline + request.source + '/4096 edits');
  Line(2, notice); Line(3, '');
  if redraw then
  begin
    Pen(True); Position(1, 4); Say('    ');
    for x := 0 to ViewCols - 1 do
    begin
      entry.text := Chr(Ord('A') + left + x - 1); Pad(entry.text, 5); Say('     ')
    end
  end;
  for y := 0 to ViewRows - 1 do
  begin
    rr := top + y;
    if rr <= MaxRows then
    begin
      if redraw then
      begin
        Pen(True); Position(1, 5 + y); Str(rr, entry.text); Pad(entry.text, 3); Say(' ')
      end;
      for x := 0 to ViewCols - 1 do
      begin
        cc := left + x;
        if (cc <= MaxCols) and (refreshed or
           (moved and (((rr = row) and (cc = col)) or
            ((rr = shownrow) and (cc = showncol))))) then
        begin
          CacheCopy(GlyphCacheBase + (y * ViewCols + x) * 10, entry.text, 10, True);
          CacheCopy((y * ViewCols + x) * 10, job.reply.visible.source, 10, True);
          fetchedrow := 0; { Source staging now holds the prior display string. }
          if redraw or (entry.text <> job.reply.visible.source) or moved and
             (((rr = row) and (cc = col)) or
              ((rr = shownrow) and (cc = showncol))) then
          begin
            Pen((rr = row) and (cc = col));
            Position(5 + x * CellWidth, 5 + y);
            Pad(entry.text, 9); Say(' '); CacheCopy((y * ViewCols + x) * 10, entry.text, 10, False)
          end
        end
      end
    end
  end;
  Line(37, 'Arrows/TAB move  Type: replace  ENTER: edit  ESC: cancel  /: commands');
  Line(38, 'Labels ''entry.text   Formulas =A1+B1*2  =@SUM(A1:B10)   > go to   /? help');
  Line(39, 'File: ' + Copy(request.path, 1, 42) + '   Saved after each edit');
  painted := True; shownrow := row; showncol := col;
  showntop := top; shownleft := left;
  Pen(False); Position(5 + (col - left) * CellWidth, 5 + row - top)
end;

overlay procedure Command;
var key: char;
{ The renderer already owns the ZVB ports and validated 80x40 coordinates. }
procedure Position(x, y: integer);
begin
  Port[$8E] := 0; Port[$A1] := y - 1; Port[$A2] := x - 1
end;

procedure Pen(inverse: boolean);
begin
  Port[$8E] := 0;
  if inverse then Port[$A5] := $F0 else Port[$A5] := $0F
end;

{ The documented ZVB text-print port advances the hardware cursor. Rendering
  printable fields directly avoids importing the general file-output engine. }
{ Cell entry accepts arrows as commit-and-move; request.path/address entry does not. }
function EditLine(prompt: CellText; var text: CellText; moving: boolean): boolean;
var key: char; done, accepted: boolean;
begin
  done := False; accepted := False; finishkey := #0;
  repeat
    Line(2, prompt); Line(3, text); Pen(False);
    Position(Length(text) + 1, 3);
    key := ReadKey;
    if IOResult <> 0 then Fatal('Keyboard read failed.');
    case key of
      #13: begin accepted := True; done := True end;
      #27: done := True;
      #160, #161, #162, #163, #9:
        if moving then
        begin
          finishkey := key; accepted := True; done := True
        end;
      #8, #127:
        begin if Length(text) > 0 then Delete(text, Length(text), 1) end
    else
      if (key >= ' ') and (key <= '~') then
        if Length(text) < 63 then text := text + key
    end
  until done;
  EditLine := accepted
end;

procedure GoToCell;
var r, c, error: integer;
begin
  request.source := '';
  if EditLine('Go to (A1..Z254): ', request.source, False) then
  begin
    r := 0; c := 0; error := 1;
    if Length(request.source) >= 2 then
    begin
      c := Ord(UpCase(request.source[1])) - Ord('A') + 1;
      Val(Copy(request.source, 2, 62), r, error)
    end;
    if (error = 0) and (r >= 1) and (r <= MaxRows) and (c >= 1) and (c <= MaxCols) then
    begin row := r; col := c; notice := '' end
    else notice := 'Use an address from A1 through Z254.'
  end
end;

procedure MoveCell(key: char);
begin
  case key of
    #160: if row > 1 then row := row - 1;
    #161: if row < MaxRows then row := row + 1;
    #162: if col > 1 then col := col - 1;
    #163, #9: if col < MaxCols then col := col + 1
  end
end;

procedure EditCell(replace: boolean; first: char);
begin
  if replace then request.source := first
  else begin View(1, row, col); request.source := job.reply.visible.source end;
  if EditLine('Entry: ENTER/arrows save, ESC cancels', request.source, True) then
  begin
    QueueEdit; MoveCell(finishkey)
  end
end;


begin
  key := ReadKey;
  if IOResult <> 0 then Fatal('Keyboard read failed.');
  notice := '';
  case key of
    #160, #161, #162, #163, #9: MoveCell(key);
    #13: EditCell(False, #0);
    '>': GoToCell;
    '/':
      begin
        Line(2, 'Command: B)lank E)dit G)oto Q)uit ?)help  ESC cancel');
        key := UpCase(ReadKey);
        if IOResult <> 0 then Fatal('Keyboard read failed.');
        case key of
          'G': GoToCell;
          'B': begin request.source := ''; QueueEdit end;
          'E': EditCell(False, #0);
          'Q':
            begin
              Line(2, 'Quit: Y to confirm');
              key := UpCase(ReadKey);
              if IOResult <> 0 then Fatal('Keyboard read failed.');
              quitting := key = 'Y'
            end;
          '?': notice := 'A1..Z254. = formulas, @SUM(A1:B10). See README for limits.';
          #27: notice := ''
        else notice := 'Unknown command. /? for help.'
        end
      end
  else
    if (key >= ' ') and (key <= '~') then EditCell(True, key)
  end
end;

{$R+}
