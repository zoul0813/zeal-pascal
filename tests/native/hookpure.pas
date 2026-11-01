program HookPure;
{$A-}
procedure outer;
var hits,old,code:integer;ready:boolean;
function status:boolean;
begin hits:=hits+1;status:=true end;
begin
 hits:=0;old:=constptr;constptr:=addr(status);
 if not keypressed then halt;
 {$I-}close(Output);ready:=keypressed;code:=ioresult;{$I+}
 if ready or(code<>34)or(hits<>1)then halt;
 constptr:=old;
end;
begin recurptr:=stackptr-2048; outer;writeln('HOOK-PURE-OK')end.
