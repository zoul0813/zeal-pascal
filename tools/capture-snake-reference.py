#!/usr/bin/env python3
"""Hash-bound native trace of the unchanged SDK Snake binary using its own VRAM mapping."""
import importlib.util,json,re,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]; spec=importlib.util.spec_from_file_location('n',root/'tests/acceptance/test-native.py');n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n);n.EMULATOR=Path('/opt/homebrew/bin/zeal-native')
sdk=n.ZEAL/'Zeal-VideoBoard-SDK/examples/snake'; image=(sdk/'build/snake.bin').read_bytes()
assert hashlib.sha256(image).hexdigest()=='25c7e8c59f6a4b3c7ef804f60b6fbee5bf9f1b30acf6dd7372a595e194950388'
trace=[]; trace_cycles=[]; obs={}
class Ref(n.DebugConsole):
 def command(self,cmd,capture=False):
  if cmd!='@ref':return super().command(cmd,capture)
  call=lambda c:super(Ref,self).command(c)
  call('bp 0x4f49');call('bp 0x4c98')
  for _ in range(500):
   stop=call('run 2000000000'); regs=call('regs')
   if 'PC=4c98 ' in regs:break
   assert 'PC=4f49 ' in regs,regs
   sp=int(re.search('SP=([a-f0-9]+)',regs)[1],16);m=n.memory(call(f'rb 0x{sp+2:x} 3'))
   trace.append([m[sp+2],m[sp+3]+256*m[sp+4]])
   trace_cycles.append(int(re.search(r'cyc=(\d+)',call('s'))[1]))
  else:raise AssertionError('title incomplete')
  call('bc 0x4f49');call('bc 0x4c98');call('s')
  # Stop on first end_game call (no keyboard); inspect state before wipe.
  call('bp 0x42f1');call('run 2000000000');assert 'PC=42f1 ' in call('regs')
  obs['death_entry']=call('regs');call('bc 0x42f1');call('s')
  call('bp 0x4ee3');call('run 2000000000');assert 'PC=4ee3 ' in call('regs');call('bc 0x4ee3')
  obs['place_disassembly']=call('u 0x4ee3 32')
  for _ in range(80):
   call('s');mmu=call('mmu')
   if '0000 -> 100000' in mmu:break
  else:raise AssertionError('C VRAM mapping was not reached')
  obs['mmu']=mmu
  obs['palette']=list(n.memory(call('rb 0xe00 40')).values())
  obs['initial_wipe_cell']=call('rb 0x1000 20')
  # Capture exact VRAM tiles, font, and both tilemap layers before any changes.
  import hashlib
  def block(at,count):
   data={}
   for i in range(0,count,256):data.update(n.memory(call(f'rb 0x{at+i:x} {min(256,count-i)}')))
   return bytes(data[at+i] for i in range(count))
  obs['board']=list(block(0,1200));obs['foreground']=list(block(0x1000,1200))
  obs['sprites']=list(block(0x2800,48))
  # Capture asset pages through the original loader's own mapped writes next run.


  # Palette leaf maps VRAM page0; break on its mapping instruction boundary.
  # Instead inspect via supported console VRAM command inventory.

  return call('regs')
commands=['run 100000000']+n.type_text('H:/SNAKE.COM')+['bp 0x4000','tap ENTER','run 2000000000','bc 0x4000','s','@ref','quit']
n.execute('snake-c-reference',commands,{'SNAKE.COM':image},timeout=180,console_class=Ref)
report=dict(binary_bytes=len(image),sha256=hashlib.sha256(image).hexdigest(),title_trace=trace,title_cycles=trace_cycles,observations=obs,capture=str(n.RUN))
f=n.RUN/'snake-c-reference/input-error.json';report['failure']=json.loads(f.read_text()) if f.exists() else None
(root/'tests/fixtures/snake/c-observation.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(failure=report['failure'], title_calls=len(trace), capture=str(n.RUN))))
