const
  ErrSyntax = 1; ErrRef = 2; ErrDiv = 3; ErrRange = 4;
  ErrCycle = 5; ErrDepth = 6; ErrLimit = 7;
  MaxFrames = 8;
type
  IssueText = string[8];
  DiskCell = record
    row, col: byte;
    previous: word;
    text: CellText;
    check: word
  end;
  ParseFrame = record
    row, col, pos, chars: byte
  end;
type ErrorTable = array[0..7] of IssueText;
const ErrorNames: ErrorTable =
  ('', '#SYNTAX', '#REF', '#DIV/0', '#RANGE', '#CYCLE', '#DEPTH', '#LIMIT');
