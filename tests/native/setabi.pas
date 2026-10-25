program SetABI;
type narrow=set of 3..5;code=string[25];
const machine:code=#209#33#0#0#57#126#35#35#174#1#31#0#9#174#35#174#35#174#35#249#111#38#0#213#201;
function native(left:integer;v:narrow;last:byte):integer;external $8004;
begin
 if native($1234,[3,255],$AB)=5 then writeln('EXTERNAL-SET-32-BYTE-OK');
 if native($1234,[],$AB)=141 then writeln('EXTERNAL-EMPTY-SET-OK');
 if native($1234,[3],$AB)=133 then writeln('EXTERNAL-COMPRESSED-SET-OK');
end.
