overlay procedure Stop;
var status: integer; amount, newline, cell: word; i: integer;
begin
  if device >= 0 then status := ZOS(ZOS_CLOSE, device);
  if outputdevice >= 0 then status := ZOS(ZOS_CLOSE, outputdevice);
  if viewdevice >= 0 then status := ZOS(ZOS_CLOSE, viewdevice);
  if ownview then status := ZOS(ZOS_RM, ViewName);
  if cachepage <> 255 then status := ZOS(ZOS_PFREE, cachepage);
  if active then
  begin
    Port[$8E] := 0; Port[$A1] := screenstate[1];
    for i := 3 to 8 do Port[$A0 + i] := screenstate[i];
    Port[$A9] := screenstate[0];
    if screenstate[2] < 80 then Port[$A2] := screenstate[2]
    else
    begin
      Port[$A2] := 79; Port[$A4] := 0;
      cell := ((screenstate[1] + screenstate[3]) mod 40) * 80 + 79;
      { Reprint the existing last cell to restore the ZVB pending-wrap state.
        Like CacheCopy, the entire page0 mapping is one interrupt-safe leaf. }
      Inline(
        $2A/cell/$ED/$57/$F5/$F3/$AF/$DB/$F0/$F5/
        $3E/$40/$D3/$F0/$5E/$01/$00/$10/$09/$7E/$D3/$A5/
        $7B/$D3/$A0/$F1/$D3/$F0/$F1/$E2/*+3/$FB
      );
      Port[$A5] := screenstate[5]; Port[$A4] := screenstate[4]
    end
  end;
  if notice <> '' then
  begin
    amount := Length(notice); Move(notice[1], job.reply, amount);
    status := ZOS(ZOS_WRITE, ZOS_DEV_STDOUT, job.reply, amount);
    newline := $0A0D; amount := 2;
    status := ZOS(ZOS_WRITE, ZOS_DEV_STDOUT, newline, amount)
  end;
  Halt
end;

