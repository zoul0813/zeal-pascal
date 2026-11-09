program KernelStatus;
uses ZOS;
var F,Z:Integer; N:Word; P:LongWord; R:TZOSRegisters;
  B:array[0..1023]of Byte;
procedure Check(Ok:Boolean);
begin if not Ok then Z:=1 div Z end;
begin
  Z:=0;
  F:=ZOS(ZOS_OPEN,'H:/KERNEL',ZOS_O_CREAT or ZOS_O_TRUNC or ZOS_O_RDWR);
  Check(F>=0); FillChar(B,SizeOf(B),42); N:=1024;
  Check(ZOS(ZOS_WRITE,F,B,N)=0); Check(N=1024);
  P:=0; Check(ZOS(ZOS_SEEK,F,P,ZOS_SEEK_SET)=0);
  B[0]:=77; R.A:=0; R.H:=F; R.BC:=1; R.DE:=Word(Addr(B));
  { Pinned kernel returns size high byte4 after the successful overwrite. }
  Check(ZOSRegisters(ZOS_WRITE,R)=4); Check(R.BC=1);
  P:=0; Check(ZOS(ZOS_SEEK,F,P,ZOS_SEEK_SET)=0); B[0]:=0; N:=1;
  Check(ZOS(ZOS_READ,F,B,N)=0); Check((N=1)and(B[0]=77));
  P:=0; Check(ZOS(ZOS_SEEK,F,P,ZOS_SEEK_SET)=0); B[0]:=88; N:=1;
  Check(ZOS(ZOS_WRITE,F,B,N)=4); Check(N=0);
  P:=0; Check(ZOS(ZOS_SEEK,F,P,ZOS_SEEK_SET)=0); B[0]:=0; N:=1;
  Check(ZOS(ZOS_READ,F,B,N)=0); Check((N=1)and(B[0]=88));
  Check(ZOS(ZOS_CLOSE,F)=0); Check(ZOS(ZOS_RM,'H:/KERNEL')=0);
  Writeln('ZOS-KERNEL-STATUS-OK')
end.
