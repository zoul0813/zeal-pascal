program HookError;
{$A-}
procedure capture(value:char);
var zero:integer;
begin zero:=0;zero:=1 div zero end;
begin conoutptr:=addr(capture);write('X')end.
