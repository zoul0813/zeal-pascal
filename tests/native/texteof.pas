program TextEofNesting;
var f:text;c:char;
begin
  assign(f,'H:/NESTIN.TXT');reset(f);
  writeln('NEST=',eof(f));writeln('DONE');
  read(f,c);if c<>'A' then halt;
  close(f);writeln('TEXT-EOF-NESTING-OK');
end.
