#!/usr/bin/env python3
"""Qualify the compact runtime with physical RAM and detached saved execution."""
import argparse,importlib.util,json,re,struct,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
n=module('native',ROOT/'tests/acceptance/test-native.py');byte=module('byte_unit',ROOT/'tests/acceptance/test-linked-addresses.py')
CASES={
 'MIN':b'begin end.',
 'ALIAS':b'var X,Z:Integer;Y:Integer absolute X;begin Z:=0;X:=42;if Y<>42 then X:=1 div Z;Y:=9;if X<>9 then X:=1 div Z end.',
 'EMPTY':b'var N,Z:Integer;R:record end;begin Z:=0;if Addr(R)<>Addr(Z)+2 then N:=1 div Z end.',
 'DEEP':("type TText=string[255];var S:TText;D,Z:Integer;procedure Step(V:TText);begin D:=D+1;if D<32 then Step(V);if V<>S then Z:=1 div Z end;begin D:=0;Z:=0;S:="+"+\n".join("'"+'x'*n+"'"for n in(63,64,64,64))+";Step(S);if D<>32 then Z:=1 div Z end.").encode(),
 'STACK':b"type TText=string[255];var S:TText;Z:Integer;procedure Check(A,B,C,D,E,F,G:TText);begin if(A<>S)or(G<>S)then Z:=1 div Z end;begin Z:=0;S:='stack';Check(S,S,S,S,S,S,S)end.",
 'RECURSIVE':b'{$A-}var N,Z:Integer;function Fact(V:Integer):Integer;begin if V=0 then Fact:=1 else Fact:=V*Fact(V-1)end;begin Z:=0;N:=Fact(5);if N<>120 then N:=1 div Z end.',
 'HELPERS':b"var N,Z:Integer;B:Boolean;S:string[4];begin Z:=0;Randomize;N:=Random(7);if(N<0)or(N>6)then N:=1 div Z;B:=KeyPressed;S:=ParamStr(1);if(ParamCount<>0)or(S<>'')then N:=1 div Z end.",
 'STDIO':b"begin WriteLn(Output,'STANDARD-OK')end.",
 'LOWSTART':b'begin end.',
 'OPAQUE':b'begin Inline($00)end.',
 'FORCED':b'begin {$RUNTIME REAL}Inline($00)end.',
 'PROC':b'var N:Integer;procedure Step;begin N:=N*2 end;begin N:=21;Step;if N<>42 then halt end.',
 'RECURSE':b'var N:Integer;procedure Step;begin N:=N+1;if N<5 then Step end;begin N:=0;Step;if N<>5 then halt end.',
 'INT':b'var N:Integer;begin N:=6*7;if N<>42 then halt end.',
 'REAL':b'var R:Real;begin R:=1.5*4;if R<>6.0 then halt end.',
 'LIB':b'uses ByteUnit;begin if Value<>42 then halt end.',
 'CRT':b'uses Crt;begin ClrScr;GotoXY(1,2);TextColor(2);if WhereX<>1 then halt;NormVideo end.',
 'SPACE':('var Data:array[1..27000]of Byte;I:Integer;begin\n'
  'for I:=1 to 27000 do Data[I]:=I mod 251;\n'
  'Inline('+('/\n'.join('/'.join('$00'for _ in range(min(25,11000-i)))for i in range(0,11000,25)))+');\n'
  'if Addr(Data)>=$6000 then halt;\n'
  'for I:=1 to 27000 do if Data[I]<>I mod 251 then halt end.').encode(),
}
pages=module('program_pages',ROOT/'tests/acceptance/test-program-pages.py')
CASES.update({k:pages.CASES[k] for k in ('BLOCKIO','TYPEDIO','TEXTIO')})
CASES['SHADOW']=pages.CASES['TYPEDIO']
CASES['LIBFILE']=b'uses ByteUnit;'+pages.CASES['TYPEDIO'].replace(b'if N<>42 then',b'if(N<>42)or(Value<>42)then')
procedure_cases={k:CASES.pop(k) for k in ('PROC','RECURSE')}
CASES={k:(v.replace(b'begin',b'var FailureZero:Integer;\nbegin FailureZero:=0;',1).replace(b'then halt',b'then FailureZero:=1 div FailureZero')if b'then halt'in v else v)for k,v in CASES.items()}
CASES.update({k:v.replace(b'var N:Integer;',b'var N,FailureZero:Integer;',1).replace(b'then halt',b'then FailureZero:=1 div FailureZero')for k,v in procedure_cases.items()})
CASES={k:v.replace(b';',b';\n')for k,v in CASES.items()}
def startup_probes(name,image,checks):
 private=(ROOT/'build/pascal_runtime.private').read_bytes()
 dispatch=struct.unpack_from('<H',private,50)[0]-0x4000
 entry=struct.unpack_from('<H',image,dispatch+1)[0]-0x4000
 root=struct.unpack_from('<H',image,20)[0]-0x4000
 assert image[entry:entry+7].startswith(bytes.fromhex('3a78feb7c2'))
 kernel=n.kernel_symbols();pages=1 if image[18]&4 else 2
 for enabled in (False,True):
  for rejected in (False,True):
   data=bytearray(image);data[entry:entry+7]=bytes([0xfb if enabled else 0xf3])+bytes(6)
   body=bytes([0xed,0x57,0xf5,0xe1,0x7d,0xe6,4,0xfe,4 if enabled else 0,0x28,5,0x21,15,2,0xfb,0xcf,0x21,15,0,0xfb,0xcf])
   data[root:root+len(body)]=body
   if rejected:
    site=data.index(bytes.fromhex('f1472e1acf'),entry)
    data[site+1]=0x41 # BC is zero after LDIR: pfree must reject page zero.
   label=f'{name}-iff{int(enabled)}-reject{int(rejected)}';file='PROBE.COM'
   commands=['run 100000000']+n.type_text('H:/'+file,ticks=2000000)
   commands+=[f"bp 0x{kernel['zos_loader_exit']:x}",'tap ENTER','run 10000000000','regs',f"rb 0x{kernel['_page_owners']:x} 32",f"bc 0x{kernel['zos_loader_exit']:x}",f"bp 0x{kernel['keyboard_read']:x}",'s','run 1000000000','regs',f"rb 0x{kernel['_page_owners']:x} 32",'quit']
   raw=n.execute('compact-startup-'+label.lower(),commands,{file:bytes(data)},timeout=180)
   frames=re.split(r'\[DBG\] PC=',raw)[1:]
   checks[label+' checkpoints']=len(frames)==2
   if len(frames)!=2:continue
   owned=n.memory(frames[0]);clean=n.memory(frames[1]);status=1 if rejected else 0
   checks[label+' status and IFF']=frames[0].startswith(f"{kernel['zos_loader_exit']:04x} ")and f'HL={status:02x}0f'in frames[0]
   checks[label+' owned pages']=sum(owned.get(kernel['_page_owners']+i)==2 for i in range(32))==(pages+1 if rejected else pages)
   checks[label+' all pages reclaimed']=all(clean.get(kernel['_page_owners']+i,255)!=2 for i in range(32))

def private_unit_probes(checks):
 sym=n.symbols('zpascal');entry=sym['_cc_fail']&65535;bank=sym['b_cc_fail']-1
 stock=(ROOT/'build/SYSFILE.ZPU').read_bytes()
 bad=bytearray(stock);bad[12]^=1;struct.pack_into('<I',bad,28,byte.fnv(bad[:28]+bad[32:]))
 source=b"var F:file;begin Assign(F,'H:/UNUSED.DAT');Rewrite(F);Close(F)end."
 for label,error,unit,unit_mode in [('stale',251,bytes(bad),False),('missing',250,None,False),('reserved',249,stock,True)]:
  seen=[]
  class FailureConsole(n.DebugConsole):
   def command(self,command,capture=False):
    if command!='@expected-failure':return super().command(command,capture)
    call=lambda value:super(FailureConsole,self).command(value)
    call(f'bp 0x{entry:x}')
    try:
     for _ in range(100):
      call('run 2000000000');frame=call('regs');mapping=call('mmu')
      page=n.memory(call(f"rb 0x{sym['_code_pages']+bank:x} 1")).get(sym['_code_pages']+bank,255)
      if f'PC={entry:04x} 'in frame and page!=255 and f'8000 -> {page*16384:06x}'in mapping:
       sp=int(re.search(r'SP=([0-9a-fA-F]{4})',frame)[1],16)
       seen.append(n.memory(call(f'rb 0x{sp+5:x} 1')).get(sp+5));self.acceptance.append(frame);return frame
      call('s')
     raise RuntimeError('expected compile failure was not reached')
    finally:call(f'bc 0x{entry:x}')
  def remove(command,response,directory):
   if unit is None and command=='run 100000000':(directory/'hostfs/SYSFILE.ZPU').unlink(missing_ok=True)
  fixtures={'PROBE.PAS':b'unit SysFile;interface implementation end.'if unit_mode else source}
  if unit is not None:fixtures['SYSFILE.ZPU']=unit
  typed=lambda text:n.type_text(text,ticks=100000000)
  commands=['run 100000000']+n.program_launch()+['run 100000000']+typed('nwH:/PROBE.PAS\n')
  if unit_mode:commands+=typed('ouq')
  commands+=['tap C','@expected-failure',n.ImageStop(sym['_key_read'],'zpascal.bin'),'quit']
  n.execute('compact-private-'+label,commands,fixtures,timeout=900,console_class=FailureConsole,after_command=remove)
  checks['private file unit '+label+' diagnostic']=seen==[error]
  folder=n.RUN/('compact-private-'+label)/'hostfs'
  checks['private file unit '+label+' publishes nothing']=not(folder/'PROBE.COM').exists()and not(folder/'UNUSED.DAT').exists()
  if unit_mode:checks['reserved authoring preserves shipped unit']=(folder/'SYSFILE.ZPU').read_bytes()==stock

def main():
 inputs=[ROOT/p for p in ('build/zpascal.bin','build/zpascal.zpl','build/CRT.ZPU','build/SYSFILE.ZPU','build/runtime-layout.json','tests/acceptance/test-compact-programs.py')]
 hashes={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--case',action='append',choices=CASES);args=p.parse_args();checks={};metrics={};sym=n.symbols('zpascal')
 for name in args.case or CASES:
  source=CASES[name];directory='shadow/'if name=='SHADOW'else'';assert max(map(len,source.splitlines()))<=127
  typed=lambda text:n.type_text(text,ticks=100000000)
  commands=['run 100000000']+n.program_launch()+['run 100000000']+typed('n')+typed('wH:/'+directory+name+'.PAS\n')
  commands+=['tap R',n.ImageStop(sym['_ram_enter'],'zpascal.bin'),f"rb 0x{sym['_ram_pages']:x} 3",n.ImageStop(sym['_key_read'],'zpascal.bin'),f"rb 0x{sym['_ram_result']:x} 128"]
  commands+=['tap ESC','run 100000000']+typed('ocs5a00\nq'if name=='LOWSTART'else'ocq')+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin'),f"rb 0x{sym['_compiler_invocation']:x} 2",'regs']+[f'rb 0x{at:x} 256'for at in range(0xfc00,0x10000,256)]+['quit']
  fixtures={directory+name+'.PAS':source,'BYTEUNIT.ZPU':byte.byte_unit(255),'CRT.ZPU':(ROOT/'build/CRT.ZPU').read_bytes()}
  if name=='SHADOW':fixtures['shadow/SYSFILE.ZPU']=b'project shadow must be ignored'
  raw=n.execute('compact-'+name.lower(),commands,fixtures,timeout=900)
  memory=n.memory(raw);folder=n.RUN/('compact-'+name.lower())/'hostfs';artifact=folder/directory/(name+'.COM')
  checks[name+' RAM success']=memory.get(sym['_ram_result']+26)==1 and memory.get(sym['_ram_result']+28)==0
  checks[name+' saved image exists']=artifact.is_file()
  if not artifact.exists():continue
  data=artifact.read_bytes();word=lambda at:struct.unpack_from('<H',data,at)[0]
  checks[name+' compact prefix']=word(132)==(16383 if name in ('REAL','FORCED') else 6042)
  if name not in ('REAL','FORCED'):checks[name+' exact integer core outside owned patches']=n.compact_prefix_matches(data)
  checks[name+' adjacent runtime and data']=word(24)==0x4000+word(132)
  checks[name+' adjacent data and code']=word(20)==(0x5a00 if name=='LOWSTART' else word(24)+word(26))
  checks[name+' complete byte stream']=word(10)==len(data)and word(28)==0x4000+len(data)
  pages=bytes(memory.get(sym['_ram_pages']+i,255)for i in range(3))
  checks[name+' physical page requirement']=len(set(pages))==(2 if name in ('REAL','BLOCKIO','TYPEDIO','TEXTIO','STDIO','SHADOW','LIBFILE') else 3 if name in ('SPACE','OPAQUE','FORCED','RECURSIVE','RECURSE','DEEP','EMPTY') else 1)
  metrics[name]={'file_bytes':len(data),'runtime_bytes':word(132),'data_base':word(24),'globals_bytes':word(26),'code_bytes':word(22),'stack_bytes':word(32),'distinct_execution_pages':len(set(pages)),'image_sha256':hashlib.sha256(data).hexdigest()}
  if name=='STACK':checks['large known stack demand recorded']=word(32)>=2304
  if name=='SPACE':checks['at least 8K beyond old static capacity']=word(26)+word(22)-(0xf400-0x8000)>=8192
  parent='zexparent';ps=n.symbols(parent);removed=[]
  def remove(command,response,directory):
   if command=='run 100000000'and not removed:
    for path in(directory/'hostfs').iterdir():
     if path.name!=parent+'.bin'and path.suffix.lower()in('.bin','.zpl','.zpu','.pas','.inc'):path.unlink();removed.append(path.name)
  commands=['run 100000000']+n.type_text('H:/'+parent+'.bin',ticks=2000000)+['tap ENTER',n.ImageStop(ps['_execute_parent_stop'],parent+'.bin'),'regs']
  commands +=[f"rb 0x{ps[k]:x} 2"for k in('_test_done','_test_passed','_test_failed')]+['quit']
  observed=n.memory(n.execute('compact-detached-'+name.lower(),commands,{name+'.COM':data,parent+'.bin':(ROOT/('build/'+parent+'.bin')).read_bytes(),'PARENT.TXT':b'\0\0H:/'+name.encode()+b'.COM'},timeout=180,after_command=remove))
  w=lambda k:observed.get(ps[k],0)|(observed.get(ps[k]+1,0)<<8)
  checks[name+' saved success without compiler or units']=bool(removed)and observed.get(ps['_test_done'])==255 and w('_test_passed')>0 and w('_test_failed')==0
  kernel=n.kernel_symbols()
  cold_commands=['run 100000000']+n.type_text('H:/'+name+'.COM',ticks=2000000)
  cold_commands+=['tap ENTER',n.ImageStop(0x40a6,name+'.COM',0),'mmu',f"bp 0x{kernel['keyboard_read']:x}",'s','run 1000000000','regs',f"rb 0x{kernel['_page_owners']:x} 32",'quit']
  cold=n.execute('compact-cold-'+name.lower(),cold_commands,{name+'.COM':data},timeout=180)
  frames=re.split(r'\[DBG\] PC=',cold)[1:]
  checks[name+' cold allocation checkpoints']=len(frames)==2
  if len(frames)==2:
   clean=n.memory(frames[1])
   mapping=dict(re.findall(r'\[DBG\] (4000|8000|c000) -> ([0-9a-f]{6})',frames[0]))
   count=len(set(mapping.values()))
   checks[name+' cold physical allocation']=frames[0].startswith('40a6 ')and count==len(set(pages))
   checks[name+' cold cleanup']=frames[1].startswith(f"{kernel['keyboard_read']:04x} ")and all(clean.get(kernel['_page_owners']+i,255)!=2 for i in range(32))
   metrics[name]['cold_execution_pages']=count
  if name in ('MIN','REAL'):startup_probes(name,data,checks)
  print(name,metrics[name],flush=True)
 if not args.case:private_unit_probes(checks)
 checks['qualification inputs stable']=all(hashlib.sha256((ROOT/p).read_bytes()).hexdigest()==digest for p,digest in hashes.items())
 report={'input_sha256':hashes,'passed':all(checks.values()),'checks':checks,'metrics':metrics,'capture_directory':str(n.RUN.relative_to(ROOT))}
 n.RUN.mkdir(parents=True,exist_ok=True);(n.RUN/'compact-programs.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));assert report['passed']
if __name__=='__main__':main()
