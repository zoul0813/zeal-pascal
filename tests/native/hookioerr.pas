program HookIOError;
{$A-}
procedure capture(value:char);
begin {$I-}erase(Usr);{$I+}end;
begin conoutptr:=addr(capture);write(Con,'X')end.
