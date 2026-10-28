program URaw;
var c:char;
begin
 repeat until keypressed;
 read(kbd,c);
 if c<>#3 then halt;
 writeln('U-RAW-OK');
end.
