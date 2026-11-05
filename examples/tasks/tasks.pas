{
  ERRANDS - A priority-ordered list allocated on the heap.

  Tasks live only for this run. Lower numbers have higher priority;
  equal priorities retain insertion order. All remaining nodes are
  released on normal exit.
}

program Errands;

{$R+}

type
  link = ^task;
  task = record
    title: string[40];
    priority: integer;
    next: link
  end;

var
  head, p, previous, fresh: link;
  cmd, s: string[40];
  i, n, priority, code, count, baseline, available, usedbytes: integer;
  quit, measured: boolean;

{ Stop before the first strictly lower-priority node. Passing equal
  priorities preserves their original order without a separate sort. }
procedure Enqueue(node: link);
var
  current, before: link;
  done: boolean;

begin
  current := head;
  before := nil;
  done := False;
  while (current <> nil) and not done do
  begin
    if current^.priority > node^.priority then
      done := True
    else
    begin
      before := current;
      current := current^.next
    end
  end;
  node^.next := current;
  if before = nil then
    head := node
  else
    before^.next := node
end;

{ Traverse without changing links; displayed numbers are positions,
  not permanent task identifiers. }
procedure Listing;
begin
  p := head;
  n := 0;
  while p <> nil do
  begin
    n := n + 1;
    WriteLn(n:3, ' [', p^.priority, '] ', p^.title);
    p := p^.next
  end;
  WriteLn(n, ' errands. Heap bytes used: ', usedbytes)
end;

begin
  WriteLn('Errands - a list that grows and shrinks. ? for help.');
  head := nil;
  count := 0;
  baseline := 0;
  measured := False;
  quit := False;
  repeat
    Write('> ');
    if Eof(Input) then
      cmd := 'Q'
    else
      ReadLn(Input, cmd);

    { Compare task storage with this initialized, empty-list baseline. }
    if not measured then
    begin
      baseline := MemAvail;
      measured := True
    end;
    for i := 1 to Length(cmd) do
      cmd[i] := UpCase(cmd[i]);
    if (cmd = '?') or (cmd = '') then
      WriteLn('A add, L list, D done, Q quit (list is in memory only)')
    else if cmd = 'L' then
    begin
      available := MemAvail;
      usedbytes := baseline - available;
      Listing
    end
    else if cmd = 'A' then
    begin
      if (MaxAvail < SizeOf(task)) or (count = 100) then
        WriteLn('List full.')
      else
      begin
        Write('Priority 1..9 (1 first)> ');
        if Eof(Input) then
          quit := True
        else
        begin
          ReadLn(Input, s);
          priority := 0;
          Val(s, priority, code);
          if (s = '') or (code <> 0) or (priority < 1) or (priority > 9) then
            WriteLn('Enter 1..9. Add again.')
          else
          begin
            Write('Task (40 chars)> ');
            if Eof(Input) then
              quit := True
            else
            begin
              ReadLn(Input, s);
              if s = '' then
                WriteLn('A task needs some text.')
              else
              begin
                New(fresh);
                fresh^.title := s;
                fresh^.priority := priority;
                Enqueue(fresh);
                count := count + 1;
                WriteLn('Added.')
              end
            end
          end
        end
      end
    end
    else if cmd = 'D' then
    begin
      Write('Task number from L> ');
      if Eof(Input) then
        quit := True
      else
      begin
        ReadLn(Input, s);
        n := 0;
        Val(s, n, code);
        if (s = '') or (code <> 0) or (n < 1) or (n > count) then
          WriteLn('No such task.')
        else
        begin
          p := head;
          previous := nil;
          for i := 1 to n - 1 do
          begin
            previous := p;
            p := p^.next
          end;
          if previous = nil then
            head := p^.next
          else
            previous^.next := p^.next;

          { Unlink first, then return the removed node to the heap. }
          Dispose(p);
          count := count - 1;
          WriteLn('Done.');
          available := MemAvail;
          usedbytes := baseline - available;
          Listing
        end
      end
    end
    else if cmd = 'Q' then
      quit := True
    else
      WriteLn('Unknown command. ? for help.')
  until quit;

  { No task records may remain allocated after a normal exit. }
  while head <> nil do
  begin
    p := head;
    head := head^.next;
    Dispose(p)
  end;
  available := MemAvail;
  WriteLn('Heap bytes after cleanup: ', baseline - available)
end.
