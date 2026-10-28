program URepeat;
var f:text;
begin
 assign(f,'H:/UBREAK.TXT');rewrite(f);writeln(f,'closed');
 {$U+}
 repeat {$U-} until false
end.
