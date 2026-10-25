program PointerABI;
type ip=^integer;code=string[6];
const machine:code=#209#193#225#9#213#201;
function native(p:ip;offset:integer):ip;external $8004;
begin
 if ord(native(ptr($1234),$0102))=$1336 then writeln('EXTERNAL-POINTER-WORDS-OK');
 if native(nil,0)=nil then writeln('EXTERNAL-POINTER-NIL-OK');
end.
