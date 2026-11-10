#!/usr/bin/env python3
"""Capture original Snake pixels and wipe timing through its own VRAM mapping."""
import hashlib
import importlib.util
import json
import re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tests/acceptance/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
n.EMULATOR=Path('/opt/homebrew/bin/zeal-native')
image=(n.ZEAL/'Zeal-VideoBoard-SDK/examples/snake/build/snake.bin').read_bytes()
assert hashlib.sha256(image).hexdigest()=='25c7e8c59f6a4b3c7ef804f60b6fbee5bf9f1b30acf6dd7372a595e194950388'
pattern=bytes.fromhex('d3f0')
unmaps=[0x4000+i for i in range(0x28f3)if image[i:i+len(pattern)]==pattern]
assert len(unmaps)==73
pages={};observations={}
class Capture(n.DebugConsole):
 def command(self,cmd,capture=False):
  if cmd!='@pixels':return super().command(cmd,capture)
  call=lambda c:super(Capture,self).command(c)
  def block(at,count):
   data={}
   for i in range(0,count,256):data.update(n.memory(call(f'rb 0x{at+i:x} {min(256,count-i)}')))
   return bytes(data[at+i]for i in range(count))
  for address in unmaps:call(f'bp 0x{address:x}')
  call('bp 0x4c0e')
  for _ in range(400):
   call('run 2000000000');regs=call('regs')
   if 'PC=4c0e 'in regs:break
   assert any(f'PC={address:04x} 'in regs for address in unmaps)
   mmu=call('mmu')
   page=int(re.search(r'0000 -> ([0-9a-fA-F]+)',mmu)[1],16)//16384
   if page in (64,68,69):pages[page]=block(0,16384)
   call('s')
  else:raise AssertionError('initialization incomplete')
  for address in unmaps:call(f'bc 0x{address:x}')
  call('bc 0x4c0e');call('s')
  call('bp 0x42f1');output=call('run 2000000000')
  assert 'PC=42f1 'in call('regs')
  start=int(re.search(r'cyc=(\d+)',call('s'))[1]);call('bc 0x42f1');call('bp 0x42a7')
  output=call('run 2000000000');assert 'PC=42a7 'in call('regs')
  end=int(re.search(r'cyc=(\d+)',call('s'))[1]);observations['wipe_tstates']=end-start
  return 'Original pixels and wipe timing captured\n'
commands=['run 100000000']+n.type_text('H:/SNAKE.COM')+['bp 0x4000','tap ENTER','run 2000000000','bc 0x4000','s','@pixels','quit']
n.execute('snake-original-pixels',commands,{'SNAKE.COM':image},timeout=180,console_class=Capture)
f=n.RUN/'snake-original-pixels/input-error.json';failure=json.loads(f.read_text())if f.exists()else None
sha=lambda data:hashlib.sha256(data).hexdigest()
report=dict(reference_sha256=sha(image),failure=failure,capture=str(n.RUN),**observations)
if not failure:
 report.update(snake_sha256=sha(pages[68][:4096]),checker_sha256=sha(pages[68][0x2000:0x2300]),numbers_sha256=sha(pages[68][0x3000:0x3b00]),
  font_sha256={letter:sha(pages[69][(ord(letter)-64)*256:(ord(letter)-63)*256])for letter in 'ACEKNORS'},
  font_palette=list(pages[64][0xe40:0xe4c]))
(ROOT/'tests/fixtures/snake/reference-pixels.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2));raise SystemExit(bool(failure))
