#!/usr/bin/env python3
"""Native compiler and saved/RAM execution of the shipped ZOS unit."""
import importlib.util,json,os,re,hashlib,subprocess,tempfile,argparse
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
for parent in ROOT.parents:
 if (parent/'zeal-dev-environment').is_dir():os.environ.setdefault('ZP_ZEAL_ROOT',str(parent));break
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py');n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
parser=argparse.ArgumentParser();parser.add_argument('--host-only',action='store_true');parser.add_argument('--cycles',type=int,default=120000000000);args=parser.parse_args()
with tempfile.TemporaryDirectory(prefix='zos-transport-')as folder:
 exe=Path(folder)/'transport';sdk=n.ZEAL/'zeal-dev-environment/home/Zeal-8-bit-OS/sdk/sdcc/include'
 subprocess.run(['cc','-std=c99','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-Wno-misleading-indentation','-DZS_HOST','-Isrc','-I'+str(sdk),'tests/host/syscalls.c','src/runtime/syscalls/operations.c','-o',str(exe)],cwd=ROOT,check=True)
 subprocess.run([str(exe)],check=True)
if args.host_only:raise SystemExit(0)
target='zsyscalltest';symbols=n.symbols(target)
emitted_offset=int(re.search(r'cc.emitted[\s\S]*?#_cc \+ (\d+)',(ROOT/'build/CMakeFiles/unit_runtime_core.dir/src/compiler/codegen/emit.c.asm').read_text())[1])
fixtures={target+'.'+ext:(ROOT/f'build/{target}.{ext}').read_bytes()for ext in ('bin','zpl')}
fixtures.update({name:(ROOT/'build'/name).read_bytes()for name in ('ZOS.ZPU','SYSFILE.ZPU')})
fixtures['SYSCALLS.PAS']=(ROOT/'tests/native/syscalls.pas').read_bytes()
fixtures['ZBOUND.PAS']=(ROOT/'tests/native/syscall-boundaries.pas').read_bytes()
fixtures['ZKERNEL.PAS']=(ROOT/'tests/native/syscall-kernel.pas').read_bytes()
measure_spec=importlib.util.spec_from_file_location('measure_syscalls',ROOT/'tools/measure-syscalls.py')
measure=importlib.util.module_from_spec(measure_spec);measure_spec.loader.exec_module(measure)
fixtures['ZBENCH.PAS']=measure.source()
fixtures['ZEMPTY.PAS']=b'begin end.'
fixtures['ZUNUSED.PAS']=b'uses ZOS;begin end.'
fixtures['ZCHECK.PAS']=b'var S:Integer;begin S:=ZOS(18,0);if S<>0 then ZOS(15,1)end.'
fixtures['ZREG.PAS']=b'uses ZOS;var R:TZOSRegisters;S:Integer;begin R.A:=0;R.H:=0;R.BC:=0;R.DE:=0;S:=ZOSRegisters(18,R);if S<>0 then ZOS(15,1)end.'
stale=bytearray(fixtures['ZOS.ZPU']);stale[12]^=1
checksum=2166136261
for at,value in enumerate(stale):
 if not 28<=at<32:checksum=((checksum^value)*16777619)&0xffffffff
stale[28:32]=checksum.to_bytes(4,'little')
fixtures['BAD/ZOS.ZPU']=bytes(stale)
fixtures['BAD/STALE.PAS']=b'var S:Integer;begin S:=ZOS(18,0)end.'
fixtures['NUMERIC.PAS']=b"program Numeric;begin Writeln(ZOS(18,0));Writeln('ZOS-NUMERIC-OK')end."
fixtures['BADVAR.PAS']=b'var N,S:Integer;begin N:=18;S:=ZOS(N,0)end.'
fixtures['BADMAP.PAS']=b'var S:Integer;begin S:=ZOS(23,0)end.'
fixtures['BADCOUNT.PAS']=b'var S,N:Integer;B:Byte;begin S:=ZOS(0,1,B,N)end.'
fixtures['SHADOW.PAS']=b"uses ZOS;function ZOS(A,B:Integer):Integer;begin ZOS:=A+B end;begin if ZOS(18,4)<>22 then Halt;Writeln('ZOS-SHADOW-OK')end."
fixtures['HIDDEN.PAS']=b'uses ZOS;begin Dispatch end.'
fixtures['QUALIFY.PAS']=b'uses ZOS;begin ZOS.Dispatch end.'
fixtures['ZEXIT.PAS']=b'uses ZOS;begin ZOS(ZOS_EXIT,73)end.'
fixtures['ZOVER.PAS']=b"uses ZOS;var B:Byte;begin B:=0;B:=ZOS(ZOS_EXEC,0,'H:/CHILD.COM','hello',B);Halt end."
scratch=int.from_bytes((ROOT/'build/pascal_runtime.private').read_bytes()[52:54],'little')
fixtures['ZOSREAL.PAS']=("uses ZOS;const Scratch=%d;var X:Real;N,I:Integer;Saved:array[0..255]of Byte;begin X:=Sqrt(9.0);for I:=0 to 255 do Saved[I]:=Mem[Scratch+I];N:=ZOS(ZOS_MSLEEP,0);for I:=0 to 255 do if Saved[I]<>Mem[Scratch+I]then Halt;if(N<>0)or(Abs(X-3.0)>0.0001)then Halt;Writeln('ZOS-REAL-OK')end."%scratch).encode()
fixtures['ZOSUNIT.PAS']=b'unit ZOSUnit;interface function Ping:Integer;implementation function Ping:Integer;begin Ping:=ZOS(18,0)end;end.'
fixtures['UNITCALL.PAS']=b"uses ZOSUnit;begin if Ping<>0 then Halt;Writeln('ZOS-UNIT-OK')end."
# Child checks command length and its first byte before returning status42.
fixtures['CHILD.COM']=bytes.fromhex('79fe05200a1afe682005262a2e0fcf26012e0fcf')
fixtures['ZPAGE.PAS']=b"uses ZOS;var Padding:array[0..10199]of Byte;B:array[0..299]of Byte;N:Word;F,Z:Integer;procedure C(Ok:Boolean);begin if not Ok then Z:=1 div Z end;begin Z:=0;C((Word(Addr(B))and $3FFF)>$3FFF-SizeOf(B));F:=ZOS(ZOS_OPEN,'H:/PAGE.BIN',ZOS_O_CREAT or ZOS_O_TRUNC or ZOS_O_RDWR);C(F>=0);FillChar(B,300,42);N:=300;C(ZOS(ZOS_WRITE,F,B,N)=0);C(N=300);C(ZOS(ZOS_CLOSE,F)=0);C(ZOS(ZOS_RM,'H:/PAGE.BIN')=0);Writeln('ZOS-PAGE-OK')end."
fixtures['ZIORES.PAS']=b"uses ZOS;var F:File;S,Z:Integer;begin Z:=0;Assign(F,'H:/ABSENT.TEST');{$I-}Reset(F);{$I+}S:=ZOS(ZOS_MSLEEP,0);if(S<>0)or(IOResult<>1)then Z:=1 div Z;Writeln('ZOS-IORESULT-OK')end."
# The recovered editor/importer accepts at most127 bytes per source line.
fixtures={name:data.replace(b';',b';\n') if name.upper().endswith('.PAS')else data for name,data in fixtures.items()}
class Progress(n.DebugConsole):
 def command(self,command,capture=False):
  if command!='@syscall-completion':return super().command(command,capture)
  for _ in range((args.cycles+1999999999)//2000000000):
   super().command('run 2000000000',True)
   regs=super().command('regs',True)
   prefix=n.memory(super().command('rb 0x4000 32',True))
   root=bytes(prefix.get(0x4000+i,0)for i in range(32))==fixtures[target+'.bin'][:32]
   if root:
    state=n.memory(super().command(f"rb 0x{symbols['_test_passed']:x} 32",True))
    word=lambda key:state.get(symbols['_'+key],0)|(state.get(symbols['_'+key]+1,0)<<8)
    code=n.memory(super().command(f"rb 0x{symbols['_cc']+emitted_offset:x} 2",True))
    emitted=code.get(symbols['_cc']+emitted_offset,0)|(code.get(symbols['_cc']+emitted_offset+1,0)<<8)
    print(json.dumps({'stage':word('test_stage'),'checks':word('test_index'),'failed':word('test_failed'),'emitted':emitted,'pc':re.search(r'PC=([0-9a-fA-F]+)',regs)[1]}),flush=True)
    if 'PC=fc00 'in regs and state.get(symbols['_test_done'])==255:return regs
  return regs
commands=['run 100000000']+n.program_launch(target)+[f"bp 0x{symbols['_test_stop']:x}",'@syscall-completion','s','regs','rb 0x4000 32']
fields=('test_done','test_passed','test_failed','test_index','test_stage','test_error','test_position','test_first_error','test_first_position','test_runtime_code','test_runtime_pc')
commands += [f"rb 0x{symbols['_'+field]:x} 2"for field in fields]+[f"rb 0x{symbols['_test_failures']:x} 128",'quit']
raw=n.execute('syscalls',commands,fixtures,timeout=900,base_programs=(target,'zpascal'),console_class=Progress);frame=re.split(r'\[DBG\] PC=',raw)[-1];memory=n.memory(frame)
values={field:memory.get(symbols['_'+field],0)|(memory.get(symbols['_'+field]+1,0)<<8)for field in fields}
values['test_done']=memory.get(symbols['_test_done'],0)
report={'passed':values['test_done']==255 and values['test_failed']==0 and values['test_index']==112 and values['test_stage']==22,'expected_checks':112,'native':values,'failures':[memory.get(symbols['_test_failures']+2*i,0)|(memory.get(symbols['_test_failures']+2*i+1,0)<<8)for i in range(min(values['test_failed'],64))],'fixture_sha256':{k:hashlib.sha256(v).hexdigest()for k,v in fixtures.items()},'capture':str(n.RUN)}
if report['passed']:
 # A direct shell launch leaves room for EXEC_PRESERVE; the nested harness can
 # legitimately exhaust CONFIG_KERNEL_MAX_NESTED_PROGRAMS instead.
 program=(n.RUN/'syscalls/hostfs/SYSCALLS.COM').read_bytes()
 data=int.from_bytes(program[24:26],'little');done=int.from_bytes(program[0xa7:0xa9],'little') # JP at public RT_DONE ($40A6)
 commands=['run 100000000']+n.type_text('H:/SYSCALLS.COM')+['bp 0x4000','tap ENTER','run 1000000000','bc 0x4000',f'bp 0x{done:x}','s','run 1000000000','regs',f'rb 0x{data:x} 8','quit']
 direct=n.execute('syscalls-direct',commands,{'SYSCALLS.COM':program,'CHILD.COM':fixtures['CHILD.COM']},timeout=120,base_programs=())
 frame=re.split(r'\[DBG\] PC=',direct)[-1];state=n.memory(frame)
 status=state.get(data+4,255)|(state.get(data+5,255)<<8)
 report['direct_exec']={'passed':frame.startswith(f'{done:04x} ')and status==0,'preserve_status':status,'capture':str(n.RUN)}
 report['passed']=report['direct_exec']['passed']
 if report['passed']:
  exit_program=(n.RUN/'syscalls/hostfs/ZEXIT.COM').read_bytes()
  report['exit_cleanup_failure']=measure.exit_cleanup_failure(n,exit_program)
  report['passed']=report['exit_cleanup_failure']['passed']
(ROOT/'build/syscalls-test.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']
