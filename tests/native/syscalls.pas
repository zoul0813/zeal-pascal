program Syscalls;
uses ZOS;
var F, S, Zero, Step: Integer; Count, Result, Clock: Word; Offset: LongWord;
  Buffer: array[0..299] of Byte; St: TZOSStat; Entry: TZOSDirectoryEntry;
  Page, Child, SavedMapping: Byte; Regs: TZOSRegisters; Path: String[127];
procedure Check(B: Boolean);
begin Step:=Step+1; if not B then begin Writeln('SYSCALL CHECK ',Step); Zero:=1 div Zero end end;
procedure Local;
var B: array[0..299] of Byte; N: Word;
begin
  N:=300; Check(ZOS(ZOS_READ,F,B,N)=0); Check(N=300); Check(B[299]=43)
end;
begin
  Zero:=0; Step:=0;
  Check(SizeOf(Regs)=6); Check(SizeOf(St)=29); Check(SizeOf(Entry)=17);
  Check(ZOS(ZOS_OPEN,'H:/MISSING.BIN',ZOS_O_RDONLY)<0);
  F:=ZOS(ZOS_OPEN,'H:/ZOSIO.BIN',ZOS_O_RDWR or ZOS_O_CREAT or ZOS_O_TRUNC); Check(F>=0);
  for S:=0 to 299 do Buffer[S]:=S mod 256;
  Count:=300; Check(ZOS(ZOS_WRITE,F,Buffer,Count)=0); Check(Count=300);
  Offset:=0; Check(ZOS(ZOS_SEEK,F,Offset,ZOS_SEEK_SET)=0); Check(Offset=0);
  Local;
  Check(ZOS(ZOS_CLOSE,F)=0);
  Check(ZOS(ZOS_STAT,'H:/ZOSIO.BIN',St)=0); Check(St.Size=300);
  F:=ZOS(ZOS_OPEN,'H:/ZOSIO.BIN',ZOS_O_RDONLY); Check(F>=0);
  Check(ZOS(ZOS_DSTAT,F,St)=0); Check(St.Size=300); Check(ZOS(ZOS_CLOSE,F)=0);
  Check(ZOS(ZOS_CURDIR,Path)=0); Check(Length(Path)>0);
  Check(ZOS(ZOS_MKDIR,'H:/ZOSDIR')=0);
  F:=ZOS(ZOS_OPENDIR,'H:/ZOSDIR'); Check(F>=0);
  Check(ZOS(ZOS_READDIR,F,Entry)=ZOS_ERR_NO_MORE_ENTRIES);
  Check(ZOS(ZOS_CLOSE,F)=0);
  Check(ZOS(ZOS_CHDIR,'H:/ZOSDIR')=0); Check(ZOS(ZOS_CHDIR,Path)=0);
  Check(ZOS(ZOS_RM,'H:/ZOSDIR')=0);
  Check(ZOS(ZOS_IOCTL,0,5,$000F,Result)=0);
  Check(ZOSBuffer(ZOS_IOCTL,0,1,Buffer,4,Result)=0);
  Check(Buffer[0]>0); Check(Buffer[1]>0);
  Check(ZOS(ZOS_MOUNT,255,'Z',0)>0);
  Check(ZOS(ZOS_DUP,255,254)>0); Check(ZOS(ZOS_SWAP,255,254)>0);
  Check(ZOS(ZOS_PALLOC,Page)=0); Check(ZOS(ZOS_PFREE,Page)=0);
  Regs.A:=0; Regs.H:=0; Regs.BC:=0; Regs.DE:=0;
  Check(ZOSRegisters(ZOS_PALLOC,Regs)=0); Page:=Hi(Regs.BC);
  Regs.BC:=Word(Page) shl 8; Check(ZOSRegisters(ZOS_PFREE,Regs)=0);
  SavedMapping:=Port[$80F2]; Regs.H:=SavedMapping shr 2;
  Regs.BC:=Word(SavedMapping and 3) shl 14; Regs.DE:=$8000;
  Check(ZOSRegisters(ZOS_MAP,Regs)=0); Check(Port[$80F2]=SavedMapping);
  Regs.DE:=0; Check(ZOSRegisters(ZOS_MAP,Regs)=ZOS_ERR_INVALID_VIRT_PAGE);
  Check(ZOS(ZOS_MSLEEP,0)=0); Check(ZOS(ZOS_EXIT,256)=ZOS_ERR_INVALID_PARAMETER);
  S:=ZOS(ZOS_GETTIME,0,Clock); Check((S=0)or(S=ZOS_ERR_NOT_IMPLEMENTED));
  if S=0 then Check(ZOS(ZOS_SETTIME,0,Clock)=0);
  S:=ZOS(ZOS_GETDATE,St.Date); Check((S=0)or(S=ZOS_ERR_NOT_IMPLEMENTED));
  if S=0 then Check(ZOS(ZOS_SETDATE,St.Date)=0);
  Child:=99; S:=ZOS(ZOS_EXEC,1,'H:/CHILD.COM','hello',Child);
  Check((S=0)or(S=ZOS_ERR_CANNOT_REGISTER_MORE));
  if S=0 then Check(Child=42) else Check(Child=99);
  Child:=99; Check(ZOS(ZOS_EXEC,1,'H:/ABSENT.COM','',Child)>0); Check(Child=99);
  Count:=301; Check(ZOS(ZOS_READ,0,Buffer,Count)=ZOS_ERR_INVALID_PARAMETER); Check(Count=301);
  Check(ZOS(ZOS_CLOSE,256)=ZOS_ERR_INVALID_PARAMETER);
  Check(ZOS(ZOS_RM,'H:/ZOSIO.BIN')=0);
  Writeln('ZOS-SYSCALLS-OK')
end.
