#!/usr/bin/env python3
"""Measure program-entry-to-title and LZ calls on unchanged C/Pascal Snake."""
import argparse,hashlib,importlib.util,json,re
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
sha=lambda b:hashlib.sha256(b).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--snake',type=Path,default=ROOT/'build/zvb/SNAKE.COM')
 p.add_argument('--no-lz',action='store_true',help='Profile the raw-asset Snake variant')
 p.add_argument('--output',type=Path,default=ROOT/'docs/snake-startup-evidence.json')
 p.add_argument('--emulator',type=Path,default=Path('/opt/homebrew/bin/zeal-native'))
 a=p.parse_args();n.EMULATOR=a.emulator
 image=a.snake.read_bytes();found={};results={}
 def execute(name,code,cls,command):
  commands=['run 100000000']+n.type_text('H:/SNAKE.COM')+['bp 0x4000','tap ENTER','run 2000000000',command,'quit']
  n.execute(name,commands,{'SNAKE.COM':code},timeout=300,console_class=cls)
  f=n.RUN/name/'input-error.json'
  if f.exists():raise RuntimeError(f.read_text())
 def leaf_entry(method):
  source=(ROOT/'libraries/ZVBGFX.INC').read_text()
  body=re.search(r'(?:procedure|function)\s+'+method+r'\b.*?Inline\((.*?)\)',source,re.S)[1]
  code=bytes(int(x,16)for x in re.findall(r'\$([0-9A-Fa-f]{2})',body))
  assert image.count(code)==1,method
  at=image.index(code);frame=image.rfind(bytes.fromhex('dde5dd210000dd39'),0,at)
  assert frame>=7 and image[frame-7]==0xcd
  return 0x4000+frame-7
 class Console(n.DebugConsole):
  def read(self,at,size):
   memory=n.memory(super().command(f'rb 0x{at:x} {size}'))
   return bytes(memory[at+i]for i in range(size))
  def word(self,at):return int.from_bytes(self.read(at,2),'little')
  def regs(self):return {key:int(value,16)for key,value in re.findall(r'\b(PC|SP|IX)=([0-9a-fA-F]+)',super().command('regs'))}
  def cycle(self):return int(re.search(r'cyc=(\d+)',super().command('s'))[1])
  def target(self,ret):
   assert self.read(ret-3,1)==b'\xcd',(ret,self.read(ret-3,3))
   return self.word(ret-2)
 class Discover(Console):
  def command(self,cmd,capture=False):
   if cmd!='@discover':return super().command(cmd,capture)
   call=lambda c:super(Discover,self).command(c)
   transfer=leaf_entry('VRAMTransfer');sprite=leaf_entry('GfxSpriteRender')
   call('bc 0x4000');call(f'bp 0x{transfer:x}');call(f'bp 0x{sprite:x}');call('s')
   for _ in range(100):
    call('run 2000000000');r=self.regs()
    if r['PC']==transfer:
     flags=self.word(r['SP']+2);page=self.word(r['SP']+12)
     if not a.no_lz and page>=68 and flags<2:
      encoded=self.word(r['IX']);lz=self.word(encoded)
      target=self.target(self.word(lz+2))
      assert found.get('lz',target)==target
      found['lz']=target
    else:
     assert r['PC']==sprite,r
     found['title']=self.target(self.word(r['IX']+2));break
    call('s')
   else:raise AssertionError('title not reached')
   return json.dumps(found)+'\n'
 execute('snake-startup-discover',image,Discover,'@discover')
 # Inspect actual native relocation of every internal decoder CALL/JP word.
 raw=re.search(r'Inline\((.*?)\)',(ROOT/'libraries/ZVBDECODE.INC').read_text(),re.S)[1]
 pattern=b'';relocations=[];at=0
 for token in raw.split('/'):
  token=token.strip()
  if token.startswith('$'):pattern+=re.escape(bytes([int(token[1:],16)]));at+=1
  else:
   assert re.fullmatch(r'\*[+-]\d+',token),token
   relocations.append((at,int(token[1:])));pattern+=b'..';at+=2
 matches=list(re.finditer(pattern,image,re.S))
 if not a.no_lz:
  assert len(matches)==1,'decoder not uniquely linked'
  pos=matches[0].start()
  assert all(int.from_bytes(image[pos+off:pos+off+2],'little')==0x4000+pos+off+delta for off,delta in relocations)
  decoder={'address':0x4000+pos,'bytes':at,'relocated_internal_words':len(relocations)}
 else:
  assert not matches,'raw build retains decoder'
  decoder=None
 c=(n.ZEAL/'Zeal-VideoBoard-SDK/examples/snake/build/snake.bin').read_bytes()
 assert sha(c)=='25c7e8c59f6a4b3c7ef804f60b6fbee5bf9f1b30acf6dd7372a595e194950388'
 maptext=(n.ZEAL/'Zeal-VideoBoard-SDK/examples/snake/build/snake.map').read_text()
 def c_symbol(name):return int(re.search(r'\b([0-9A-Fa-f]{8})\s+_'+name+r'\s',maptext)[1],16)
 for label,code,lz,title in [('pascal',image,found.get('lz'),found['title']),('c',c,c_symbol('gfx_tileset_load_lz'),c_symbol('title_play'))]:
  class Profile(Console):
   def command(self,cmd,capture=False):
    if cmd!='@profile':return super().command(cmd,capture)
    call=lambda v:super(Profile,self).command(v)
    call('bc 0x4000');start=self.cycle();call(f'bp 0x{title:x}');
    if lz is not None:call(f'bp 0x{lz:x}')
    calls=[];ret=None;begin=None
    for _ in range(12):
     call('run 2000000000');r=self.regs()
     if r['PC']==title:
      end=self.cycle();assert ret is None
      results[label]={'sha256':sha(code),'file_bytes':len(code),'startup_tstates':end-start,'lz_calls':calls,'lz_tstates':sum(row['tstates']for row in calls),'capture':str(n.RUN)}
      break
     elif r['PC']==lz:
      assert ret is None
      ret=self.word(r['SP']);call(f'bp 0x{ret:x}')
      size=self.word(r['SP']+(8 if label=='pascal' else 2))
      begin=self.cycle()
     else:
      assert ret is not None and r['PC']==ret,r
      elapsed=self.cycle()-begin;calls.append({'compressed_bytes':size,'tstates':elapsed})
      call(f'bc 0x{ret:x}');ret=None
    else:raise AssertionError('startup incomplete')
    return 'startup profile complete\n'
  execute('snake-startup-'+label,code,Profile,'@profile')
 report={'clock_hz':10000000,'scope':'CPU startup from COM entry through title entry; disk loading and title animation excluded. C contains full font; Pascal contains selected glyphs.','emulator':str(n.EMULATOR),'decoder':decoder,'programs':results}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
