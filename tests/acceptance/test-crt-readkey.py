#!/usr/bin/env python3
"""Exercise Crt ReadKey through native RAM and saved programs with physical key events."""
from pathlib import Path
import importlib.util,re,struct,json,argparse
root=Path(__file__).resolve().parents[2];sp=importlib.util.spec_from_file_location('native',root/'tests/acceptance/test-native.py');n=importlib.util.module_from_spec(sp);sp.loader.exec_module(n);parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--emulator',type=Path);parser.add_argument('--pending',action='store_true');args=parser.parse_args();n.EMULATOR=args.emulator or n.EMULATOR;sym=n.symbols('zzvbtest')
names=re.findall(r'RP_[A-Z_]+',(root/'src/compiler/internal.h').read_text().split('enum { RP_INPUT_READY',1)[1].split('RP_COUNT }',1)[0]);names.insert(0,'RP_INPUT_READY');private=(root/'build/pascal_runtime.private').read_bytes();raw=struct.unpack_from('<H',private,2*names.index('RP_INPUT_RAW_KEY'))[0];runtime=(root/'build/pascal_runtime.zpl').read_bytes();gold=runtime[raw-0x4000:raw-0x4000+16];hits=[]
# Absolute private-state operands relocate with the compact runtime. Match only
# the decoder's opcodes and branch displacement, then verify the live image.
fixed=(0,3,4,5,6,7,10,13,14,15)
def decoder_at(data,at):return all(data[at+i]==gold[i]for i in fixed)
class Keys(n.DebugConsole):
 def command(self,cmd,capture=False):
  if cmd!='@read-keys':return super().command(cmd,capture)
  call=lambda v:super(Keys,self).command(v)
  stop=sym['_test_stop'];entry=sym['_run_build'];saved=sym['_exec'];phase=0;actual=None
  call(f'bp 0x{entry:x}');call(f'bp 0x{stop:x}')
  for _ in range(1000):
   call('run 2000000000');r=call('regs')
   if phase==0 and f'PC={entry:04x} 'in r:
    payload=(self.directory/'hostfs/SMOKE.COM').read_bytes()
    offsets=[i for i in range(len(payload)-len(gold)+1)if decoder_at(payload,i)]
    assert len(offsets)==1,'saved compact ReadKey decoder is not unique'
    actual=0x4000+offsets[0]
    call(f'bc 0x{entry:x}');call(f'bp 0x{actual:x}');phase=1
   elif phase==2 and f'PC={saved:04x} 'in r:
    call(f'bc 0x{saved:x}');call(f'bp 0x{actual:x}');phase=3
   elif actual is not None and f'PC={actual:04x} 'in r:
    mem=n.memory(call(f'rb 0x{actual:x} 16'))
    if decoder_at(bytes(mem.get(actual+i,0)for i in range(16)),0):
     call('tap A');hits.append(True);call(f'bc 0x{actual:x}')
     if phase==1:call(f'bp 0x{saved:x}');phase=2
     else:phase=4
   elif f'PC={stop:04x} 'in r:
    mem=n.memory(call('rb 0x4000 32'))
    if bytes(mem.get(0x4000+i,0)for i in range(32))==f['zzvbtest.bin'][:32]:return 'native ReadKey completed\n'
   call('s')
  raise RuntimeError('ReadKey completion missing')
f={f'zzvbtest.{ext}':(root/f'build/zzvbtest.{ext}').read_bytes()for ext in('bin','zpl')};f.update({'SKIPHW.RUN':b'1','PREBUILT.RUN':b'1','CLIENTS.RUN':b'1','CRT.ZPU':(root/'build/CRT.ZPU').read_bytes(),'SMOKE.PAS':b"uses Crt;var C:Char;Z:Integer;begin Z:=0;C:=ReadKey;if C<>'a'then Z:=1 div Z end."})
if args.pending:
 f['SMOKE.PAS']=b"uses Crt;var C:Char;Z,E:Integer;begin Z:=0;\nwhile not KeyPressed do begin end;\n{$I-}Reset(Input);C:=ReadKey;E:=IOResult;{$I+}\nif(C<>#0)or(E<>$22)then Z:=1 div Z;\nif not KeyPressed then Z:=1 div Z;\nC:=ReadKey;if C<>'a'then Z:=1 div Z end."
cmd=['run 100000000']+n.program_launch('zzvbtest')+['@read-keys','regs']+[f'rb 0x{sym[k]:x} 2'for k in('_test_done','_test_passed','_test_failed','_test_index')]+['quit'];r=n.execute('crt-readkey',cmd,f,timeout=600,base_programs=('zpascal',),console_class=Keys);assert not (n.RUN/'crt-readkey/input-error.json').exists();assert len(hits)==2;frame=re.split(r'\[DBG\] PC=',r)[-1];mem=n.memory(frame);assert mem.get(sym['_test_failed'],255)==0;image=(n.RUN/'crt-readkey/hostfs/SMOKE.COM').read_bytes();assert args.pending or image[18]&4;print(json.dumps({'keypress_injections':len(hits),'pending_error_case':args.pending,'image_flags':image[18],'capture':str(n.RUN)}))
