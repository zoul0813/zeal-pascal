{
  QUEENS - Enumerate non-attacking queen placements.

  Searches boards of size 1 through 8, counting every solution and
  retaining the first complete board for display. Sets describe the
  columns and diagonals already occupied.
}

program Queens;

{$R+}
{$A-}
{ Backtracking needs a separate set of local trial coordinates per call. }

type
  columns = set of 0..7;
  diagonals = set of 0..14;
  arrangement = array[0..7] of integer;

var
  occupied: columns;
  descending, ascending: diagonals;
  board, first: arrangement;
  size, solutions, code, i, row: integer;
  tried: real;
  text: string[20];
  quit: boolean;

{ Place one queen per row. Reserve its column and both diagonals,
  recurse, then undo exactly those reservations for the next trial. }
procedure Search(row: integer);
var
  col, down, up: integer;

begin
  if row = size then
  begin
    solutions := solutions + 1;

    { An array assignment preserves the first board while search continues. }
    if solutions = 1 then
      first := board
  end
  else
    for col := 0 to size - 1 do
    begin
      { Count every candidate position, including those rejected below. }
      tried := tried + 1.0;
      down := row + col;
      up := row - col + 7;
      if not (col in occupied) and
         not (down in descending) and not (up in ascending) then
      begin
        board[row] := col;
        occupied := occupied + [col];
        descending := descending + [down];
        ascending := ascending + [up];
        Search(row + 1);
        occupied := occupied - [col];
        descending := descending - [down];
        ascending := ascending - [up]
      end
    end
end;

begin
  WriteLn('Queens - count every non-attacking arrangement.');
  quit := False;
  repeat
    Write('Board size 1..8 (blank quits)> ');
    if Eof(Input) then
      quit := True
    else
    begin
      ReadLn(Input, text);
      if text = '' then
        quit := True
      else
      begin
        size := 0;
        Val(text, size, code);
        if (code <> 0) or (size < 1) or (size > 8) then
          WriteLn('Enter 1..8.')
        else
        begin
          occupied := [];
          descending := [];
          ascending := [];
          solutions := 0;
          tried := 0.0;
          Search(0);
          Write('N=', size, ': ', solutions);
          if solutions = 1 then
            Write(' solution; ')
          else
            Write(' solutions; ');
          WriteLn(tried:0:0, ' positions tried.');
          if solutions > 0 then
          begin
            WriteLn('First solution:');
            for row := size - 1 downto 0 do
            begin
              Write(row + 1, ' ');
              for i := 0 to size - 1 do
                if first[row] = i then
                  Write('Q ')
                else
                  Write('. ');
              WriteLn
            end;
            Write('  ');
            for i := 0 to size - 1 do
              Write(Chr(Ord('a') + i), ' ');
            WriteLn
          end
        end
      end
    end
  until quit
end.
