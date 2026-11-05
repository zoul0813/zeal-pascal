unit PrivateOps;
interface
function Exercise(BufLen:Integer):Integer;
procedure ShadowHook(var ConOutPtr:Integer);
function EchoByte(V:Byte):Byte;
function EchoChar(V:Char):Char;
procedure Untyped(var V);
implementation
type Cell=record X,Y:Integer end;
var Slots:array[1..3]of Integer;Bias:Integer;CBreak:Boolean;
    A:Integer absolute $100;
function Forwarded(N:Integer):Integer;forward;
function Transform(N:Integer):Integer;
var I:Integer;R:Cell;S:string[4];
 function Step(V:Integer):Integer;
 begin Step:=V+Bias end;
begin
 S:='abc';S[2]:='Z';
 with R do begin X:=N;Y:=Step(X) end;
 for I:=1 to 3 do Slots[I]:=R.Y+I;
 case Forwarded(N) of
  4:Transform:=Slots[1]+Slots[3];
  else Transform:=-100
 end;
 if S<>'aZc' then Transform:=-200
end;
{$A-}
function Factorial(V:Integer):Integer;
var Saved,Product:Integer;
begin
 Saved:=V;
 if V<=1 then Product:=1 else Product:=Factorial(V-1);
 if Saved<>V then Factorial:=-500 else Factorial:=Saved*Product
end;
{$A+}
function UnusedSibling(V:Integer):Integer;
var S:string[32];
begin S:='PRIVATE-DEAD-CODE-9B71';UnusedSibling:=V+length(S) end;
function Forwarded;
begin Forwarded:=N end;
function HeapPtr(A:Integer):Integer;
begin HeapPtr:=Transform(A) end;
function Exercise(BufLen:Integer):Integer;
begin
 Bias:=BufLen;CBreak:=false;CBreak:=not CBreak;
 if CBreak then Exercise:=HeapPtr(4)+Factorial(4) else Exercise:=-300
end;
procedure ShadowHook(var ConOutPtr:Integer);
begin ConOutPtr:=ConOutPtr+1 end;
function EchoByte(V:Byte):Byte;
begin EchoByte:=V end;
function EchoChar(V:Char):Char;
begin EchoChar:=V end;
procedure Untyped(var V);
var N:Integer;
begin N:=17;Move(N,V,2) end;
end.
