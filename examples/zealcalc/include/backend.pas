type
  RowHeads = array[1..MaxRows] of word;
  ReplyBuffer = record
    case boolean of
      False: (header: SheetView);
      True: (visible: ViewCell)
  end;
  CalcRequest = record
    tag, generation: word;
    { The calculator no longer needs path after opening its data descriptor.
      It then reuses exactly those 128 bytes for reply staging. The index,
      generation and viewport coordinates remain live outside that overlay. }
    case boolean of
      False: (path: string[127]; top, left: byte; heads: RowHeads;
              records: word);
      True: (reply: ReplyBuffer)
  end;
