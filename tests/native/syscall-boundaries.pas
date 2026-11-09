program Bounds;
uses ZOS;
const Sizes: array[0..6] of Word = (0,1,127,128,129,256,1024);
var F, I, J, Z: Integer; N, SP: Word; Position: LongWord;
  B: array[0..1023] of Byte; Page, Mapping2, Mapping3: Byte;
procedure Check(Ok: Boolean);
begin if not Ok then Z:=1 div Z end;
begin
  Z:=0; SP:=StackPtr; Mapping2:=Port[$80F2]; Mapping3:=Port[$C0F3];
  F:=ZOS(ZOS_RM,'H:/BOUNDS');
  for I:=0 to 6 do begin
    F:=ZOS(ZOS_OPEN,'H:/BOUNDS',ZOS_O_CREAT or ZOS_O_TRUNC or ZOS_O_RDWR);
    Check(F>=0);
    Position:=0; Check(ZOS(ZOS_SEEK,F,Position,ZOS_SEEK_SET)=0);
    for J:=0 to 1023 do B[J]:=(J mod 251)+1; N:=Sizes[I];
    Check(ZOS(ZOS_WRITE,F,B,N)=0); Check(N=Sizes[I]);
    Position:=0; Check(ZOS(ZOS_SEEK,F,Position,ZOS_SEEK_SET)=0);
    FillChar(B,SizeOf(B),0); N:=Sizes[I];
    Check(ZOS(ZOS_READ,F,B,N)=0); Check(N=Sizes[I]);
    for J:=0 to Integer(N)-1 do Check(B[J]=(J mod 251)+1);
    if N<1024 then Check(B[N]=0);
    N:=1; Check(ZOS(ZOS_READ,F,B,N)=0); Check(N=0);
    Check(ZOS(ZOS_CLOSE,F)=0);
    if I<6 then Check(ZOS(ZOS_RM,'H:/BOUNDS')=0)
  end;
  F:=ZOS(ZOS_OPEN,'H:/BOUNDS',ZOS_O_RDONLY); Check(F>=0);
  Position:=960; Check(ZOS(ZOS_SEEK,F,Position,ZOS_SEEK_SET)=0);
  FillChar(B,SizeOf(B),0); N:=128;
  Check(ZOS(ZOS_READ,F,B,N)=0); Check((N=64)and(B[63]=20));
  Check(B[64]=0);
  Check(ZOS(ZOS_CLOSE,F)=0);
  for I:=1 to 64 do begin
    Check(ZOS(ZOS_PALLOC,Page)=0); Check(ZOS(ZOS_PFREE,Page)=0);
    F:=ZOS(ZOS_OPEN,'H:/BOUNDS',ZOS_O_RDONLY); Check(F>=0);
    N:=1; Check(ZOS(ZOS_READ,F,B,N)=0); Check((N=1)and(B[0]=1));
    Check(ZOS(ZOS_CLOSE,F)=0)
  end;
  Check(ZOS(ZOS_RM,'H:/BOUNDS')=0);
  Check(Port[$80F2]=Mapping2); Check(Port[$C0F3]=Mapping3);
  Check(Word(StackPtr)=SP); Writeln('ZOS-BOUNDS-OK')
end.
