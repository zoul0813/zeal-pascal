program StringABI;
type short=string[3]; code=string[36];
const machine:code=#209'!'#0#0'9~'#245'##N'#6#0#3#9'F##6'#3'#p#'#13'q#'#241'w+++'#249'!'#173#222#213#201;
      a='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+-';
      b='abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789+';
function native(a:integer; v:short; b:integer):short; external $B800;
begin
 move(machine[1],mem[$B800],36);
 if native(123,'abcdef',456)=chr(123)+chr(6)+chr(200) then writeln('EXTERNAL-STRING-ABI-OK');
 if native(123,''+'',456)=chr(123)+chr(0)+chr(200) then writeln('EXTERNAL-ZERO-ARGUMENT-OK');
 if native(123,'X'+'',456)=chr(123)+chr(1)+chr(200) then writeln('EXTERNAL-ONE-ARGUMENT-OK');
 if native(123,a+a+a+b,456)=chr(123)+chr(255)+chr(200) then writeln('EXTERNAL-255-ARGUMENT-OK');
end.
