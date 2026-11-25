program SheetOvr;
{ Compile to COM. Keep the generated ZPxxxx directory beside this program. }
var Cells:array[1..3] of Integer;
    Seen,Total,Zero:Integer;

procedure Check(OK:Boolean);
begin
  if not OK then Total:=1 div Zero
end;

overlay function Sum:Integer;
const Calls:Integer=5;
var I,N:Integer;
begin
  Calls:=Calls+1;
  Seen:=Calls;
  N:=0;
  for I:=1 to 3 do N:=N+Cells[I];
  Sum:=N
end;

overlay procedure Scale(Factor:Integer);
var I:Integer;
begin
  for I:=1 to 3 do Cells[I]:=Cells[I]*Factor
end;

procedure Separator;
begin end;

overlay procedure Unused;
begin
  Total:=1 div Zero
end;

begin
  Zero:=0;
  OvrDrive(8); { H: for this HostFS example; use 20 for T: }
  Cells[1]:=10;Cells[2]:=20;Cells[3]:=30;
  Total:=Sum;Check((Total=60) and (Seen=6));
  Total:=Sum;Check((Total=60) and (Seen=7));
  Scale(2);
  Total:=Sum;Check((Total=120) and (Seen=8));
  WriteLn('Sheet total: ',Total);
  WriteLn('OVERLAYS OK')
end.
