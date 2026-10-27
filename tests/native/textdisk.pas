program TextDisk;
var f:text;s:string[20];c:char;
begin
  assign(f,'H:/TEXTDISK.TXT');rewrite(f);
  write(f,'ABC');writeln(f,'D');writeln(f,'  123');close(f);
  reset(f);read(f,c);
  if(c<>'A')or eoln(f)or eof(f)then halt;
  readln(f,s);if s<>'BCD' then halt;
  readln(f,s);if s<>'  123' then halt;
  if not eof(f)or not eoln(f)then halt;
  close(f);writeln('TEXT-DISK-OK');
end.
