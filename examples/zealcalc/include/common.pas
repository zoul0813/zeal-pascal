const
  MaxRows = 254; MaxCols = 26; MaxEdits = 4096;
  ViewRows = 30; ViewCols = 7; CellWidth = 10;
  RequestName = 'zc.req'; ViewName = 'zc.view';
  WireTag = 17242;
type
  CellText = string[63]; LineText = string[79];
  PathText = string[127];
  SheetRequest = record
    tag, generation: word;
    path: string[127];
    top, left, editrow, editcol, edit: byte;
    source: CellText;
    padding: array[0..54] of byte
  end;
  SheetView = record
    tag, generation: word;
    top, left: byte;
    records: word;
    notice: string[75];
    padding: array[0..43] of byte
  end;
  ViewCell = record
    source: CellText;
    shown: string[9];
    labelcell: boolean;
    padding: array[0..52] of byte
  end;
