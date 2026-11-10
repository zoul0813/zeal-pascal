{$R-}
{ Internal indices follow explicit address, frame, length and loop bounds. }
{ Monochrome status/prompt/entry band, inverse borders and active cell.
  Row 40 stays untouched; painting never writes the bottom-right corner. }
procedure Pen(inverse: boolean);
begin
  Port[$8E] := 0;
  if inverse then Port[$A5] := $F0 else Port[$A5] := $0F
end;

{ The documented ZVB text-print port advances the hardware cursor. Rendering
  printable fields directly avoids importing the general file-output engine. }
procedure Say(text: LineText);
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

procedure Line(y: integer; text: LineText);
begin
  Pen(False); GotoXY(1, y); ClrEol; Say(text)
end;

function Reference(r, c: integer): CellText;
var text: CellText;
begin
  Str(r, text); Reference := Chr(Ord('A') + c - 1) + text
end;

procedure Paint;
var rr, cc, x, y: integer; text, raw: CellText; redraw: boolean;
begin
  if row < top then top := row;
  if row >= top + ViewRows then top := row - ViewRows + 1;
  if col < left then left := col;
  if col >= left + ViewCols then left := col - ViewCols + 1;
  redraw := not painted or (top <> showntop) or (left <> shownleft);
  if redraw then RefreshView;
  raw := Cell(row, col); text := DisplayCell(row, col);
  if labelcell then text := '(L)' else text := '(V)';
  Line(1, Copy(Reference(row, col) + ' ' + text + ' ' + raw, 1, 63));
  Pen(False); GotoXY(65, 1); Str(records - 1, text); Pad(text, 4); Say('/4096 edits');
  Line(2, notice); Line(3, '');
  if redraw then
  begin
    Pen(True); GotoXY(1, 4); Say('    ');
    for x := 0 to ViewCols - 1 do
    begin
      text := Chr(Ord('A') + left + x - 1); Pad(text, 5); Say('     ')
    end
  end;
  for y := 0 to ViewRows - 1 do
  begin
    rr := top + y;
    if rr <= MaxRows then
    begin
      if redraw then
      begin
        Pen(True); GotoXY(1, 5 + y); Str(rr, text); Pad(text, 3); Say(' ')
      end;
      for x := 0 to ViewCols - 1 do
      begin
        cc := left + x;
        if (cc <= MaxCols) and (redraw or
           ((rr = row) and (cc = col)) or
           ((rr = shownrow) and (cc = showncol))) then
        begin
          text := DisplayCell(rr, cc);
          Pen((rr = row) and (cc = col));
          GotoXY(5 + x * CellWidth, 5 + y);
          Pad(text, 9); Say(' ')
        end
      end
    end
  end;
  Line(37, 'Arrows/TAB move  Type: replace  ENTER: edit  ESC: cancel  /: commands');
  Line(38, 'Labels ''text   Formulas =A1+B1*2  =@SUM(A1:B10)   > go to   /? help');
  Line(39, 'File: ' + Copy(path, 1, 42) + '   Saved after each edit');
  painted := True; shownrow := row; showncol := col;
  showntop := top; shownleft := left;
  Pen(False); GotoXY(5 + (col - left) * CellWidth, 5 + row - top)
end;

{ Cell entry accepts arrows as commit-and-move; path/address entry does not. }
function EditLine(prompt: CellText; var text: CellText; moving: boolean): boolean;
var key: char; done, accepted: boolean;
begin
  done := False; accepted := False; finishkey := #0;
  repeat
    Line(2, prompt); Line(3, text);
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
var text: CellText; r, c, error: integer;
begin
  text := '';
  if EditLine('Go to (A1..Z254): ', text, False) then
  begin
    r := 0; c := 0; error := 1;
    if Length(text) >= 2 then
    begin
      c := Ord(UpCase(text[1])) - Ord('A') + 1;
      Val(Copy(text, 2, 62), r, error)
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
var text: CellText;
begin
  if replace then text := first else text := Cell(row, col);
  if EditLine('Entry: ENTER/arrows save, ESC cancels', text, True) then
  begin
    PutCell(text); MoveCell(finishkey)
  end
end;

procedure Command;
var key: char;
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
          'B': PutCell('');
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
