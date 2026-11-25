{ Ordinary resident leaf: no Pascal or syscall executes while page0 is mapped.
  Capture IFF2, disable interrupts, copy, restore page0 and then the caller's IFF. }
procedure CacheCopy;
begin
  if (cachepage < 32) or (cachepage > 63) or (size < 1) or
     (size > 16384) or (offset > 16384 - size) then BackendFailure(37);
  Inline(
    $ED/$57/$F5/$F3/                 { ld a,i; push af; di }
    $AF/$DB/$F0/$F5/                 { select/read page0; push af }
    $3A/cachepage/$D3/$F0/           { map our data bank }
    $2A/offset/$ED/$5B/buffer/       { source offset / shared buffer pointer }
    $ED/$4B/size/                    { bounded byte count }
    $3A/reading/$B7/$20/$01/$EB/      { reverse HL/DE for writes }
    $ED/$B0/                        { ldir }
    $F1/$D3/$F0/$F1/$E2/*+3/$FB     { restore page0 and IFF2 }
  )
end;
