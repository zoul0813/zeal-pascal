program RealABI;
type bytes=array[0..5]of byte;bytecode=array[1..75]of byte;
 machinecode=record length:byte;payload:bytecode end;
const machine:machinecode=(length:75;payload:(
221,229,221,33,0,0,221,57,221,126,12,221,174,4,111,221,
126,13,221,174,5,221,174,10,103,221,126,14,221,174,6,95,
221,126,15,221,174,7,87,221,126,16,221,174,8,79,221,126,
17,221,174,9,221,174,11,71,221,225,241,51,51,51,51,51,
51,51,51,51,51,51,51,51,51,245,201));
      first:real=pi;last:real=-1.25;result:real=0.0;
      left:bytes=(0,0,0,0,0,0);right:bytes=(0,0,0,0,0,0);actual:bytes=(0,0,0,0,0,0);
      mark:integer=$1234;i:integer=0;attempt:integer=0;saved:integer=0;
      good:boolean=true;
{$A-}
function wire(a:real;n:integer;b:real):real;external $B800;
function returned:real;begin returned:=first end;
procedure checkbytes;
begin
 move(first,left,6);move(last,right,6);move(result,actual,6);
 for i:=0 to 5 do begin
  if i=1 then left[i]:=left[i]xor lo(mark);
  if i=5 then left[i]:=left[i]xor hi(mark);
  if actual[i]<>(left[i]xor right[i])then good:=false;
 end;
end;
begin
 move(machine.payload,mem[$B800],75);
 result:=wire(returned,mark,last);checkbytes;
 if good then writeln('EXTERNAL-REAL-ARGUMENT-RESULT-OK');
 first:=2;last:=3;mark:=$12FE;good:=true;
 result:=wire(2,mark,3);checkbytes;
 if good then writeln('EXTERNAL-REAL-INTEGER-PROMOTION-OK');
 saved:=stackptr;for attempt:=1 to 40 do result:=wire(first,mark,last);
 if stackptr=saved then writeln('EXTERNAL-REAL-REPEATED-STACK-OK');
end.
