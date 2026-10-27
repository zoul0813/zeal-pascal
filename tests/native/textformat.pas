program TextFormat;
var f:text;s:string[255];i:integer;
begin
  assign(f,'H:/TEXTFORMAT.TXT');rewrite(f);
  writeln(f,'A':3,-123:6,true:7,'Z':-1,1.5:8:2);
  writeln(f,'x':300);
  s:='';for i:=1 to 255 do s:=s+'y';
  writeln(f,s:256);
  close(f);writeln('TEXT-FORMAT-OK');
end.
