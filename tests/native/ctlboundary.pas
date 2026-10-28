program ConsoleBoundary;
var ready:boolean;pad:array[1..15197]of byte;saved:integer;
begin
 saved:=stackptr;pad[1]:=73;pad[15197]:=149;ready:=false;
 {$U+}while not ready do begin end;{$U-}
 write('B');
 if(pad[1]<>73)or(pad[15197]<>149)then halt;
 if stackptr<>saved then halt;
end.
