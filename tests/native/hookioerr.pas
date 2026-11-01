program HookIOError;
{$A-}
procedure capture(value:char);
begin {$I-}erase(Usr);{$I+}end;
begin recurptr:=stackptr-2048; conoutptr:=addr(capture);write(Con,'X')end.
