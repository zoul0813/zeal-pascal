{$C-}{$A+}
const
 Mmu0=$00F0; Mmu1=$40F1; Mmu2=$80F2; Mmu3=$C0F3;
 TextSelect=$8E; TextController=0; TextPrint=$A0;
var
 SavedI, SavedFlags, VectorI, Flags: Byte;
 Pages: array[0..3] of Byte;
 Failed: Boolean;
 CaseNumber, Zero: Integer;
procedure CheckMappings;
begin
 if (Port[Mmu0]<>Pages[0])or(Port[Mmu1]<>Pages[1])or
    (Port[Mmu2]<>Pages[2])or(Port[Mmu3]<>Pages[3])then Failed:=True;
 Inline($ED/$57/$F5/$E1/$7D/$32/Flags);
 if (Flags and 4)<>0 then Failed:=True;
 { Restore the kernel mapping even when a tested kernel failed. }
 Port[Mmu0]:=Pages[0]
end;
begin
 Failed:=False;Zero:=0;
 Port[TextSelect]:=TextController;
 { Preserve the board's real vector and IFF2 before selecting test vectors. }
 Inline($ED/$57/$32/SavedI/$F5/$E1/$7D/$32/SavedFlags/$F3);
 Pages[0]:=Port[Mmu0];Pages[1]:=Port[Mmu1];
 Pages[2]:=Port[Mmu2];Pages[3]:=Port[Mmu3];
 for CaseNumber:=1 to 3 do begin
  VectorI:=CaseNumber*64;
  Inline($3A/VectorI/$ED/$47);
  ClrScr;CheckMappings;
  GotoXY(1,1);ClrEol;CheckMappings;
  InsLine;CheckMappings;
  DelLine;CheckMappings;
  GotoXY(80,1);Port[TextPrint]:=Ord('X');CrtInit;
  GotoXY(2,2);CrtExit;CheckMappings
 end;
 Inline($3A/SavedI/$ED/$47);
 if (SavedFlags and 4)<>0 then Inline($FB);
 if Failed then Zero:=1 div Zero
end.
