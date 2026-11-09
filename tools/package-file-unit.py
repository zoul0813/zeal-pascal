#!/usr/bin/env python3
"""Build the compiler-private, selectively linked native file unit in ZDE."""
import argparse,hashlib,json,os,re,struct,subprocess
from pathlib import Path
from runtime_objects import read_object,read_archive,library_closure
ROOT=Path(__file__).resolve().parents[1]
EXPORTS={'dispatch':'_zf_entry','table':'_zf_operations_address',
 **{name:'_zf_op_'+name for name in ('reset','rewrite','close','read','write','position','seek','flush','erase','rename','blocks','text_read','text_readln','predicate','text_write','begin','path')}}
def fnv(data):
 h=2166136261
 for b in data:h=((h^b)*16777619)&0xffffffff
 return h

def split_assembly(text,prefix):
 """Give each compiler-emitted function its own ordinary ASxxxx area."""
 current='_'+prefix+'_constants';result=[];count=0
 for line in text.splitlines():
  if re.search(r'; Function [A-Za-z_]',line):
   count+=1;current='_'+prefix+'_'+str(count);result.append('\t.area '+current)
  if re.match(r'\s*\.area _(TEXT|CODE)\b',line):line='\t.area '+current
  result.append(line)
 assert count
 return '\n'.join(result)+'\n'

def build(directory,cc,assembler,sdk,payload_only=False):
 directory=directory.resolve();out=directory/'direct-file';out.mkdir(exist_ok=True)
 paths=[]
 for source,prefix in [('runtime/files/operations.c','F'),('common/path.c','P')]:
  asm=out/(prefix+'.asm')
  subprocess.run([cc,'-mz80','--std-c99','--opt-code-size','-DZP_DIRECT_FILE','-I'+str(ROOT/'src'),'-I'+str(sdk/'include'),'-S',str(ROOT/'src'/source),'-o',str(asm)],check=True)
  split=out/(prefix+'-split.asm');split.write_text(split_assembly(asm.read_text(),prefix))
  rel=out/(prefix+'.rel');subprocess.run([assembler,'-g','-o',str(rel),str(split)],check=True);paths.append(rel)
 for source in ('runtime/files/direct.asm',):
  rel=out/(Path(source).name+'.rel');subprocess.run([assembler,'-g','-o',str(rel),str(ROOT/'src'/source)],check=True);paths.append(rel)
 # Use the SDK's actual syscall wrappers, each in its own linkable area.
 glue=(sdk/'src/zeal8bitos.asm').read_text()
 public=set(re.findall(r'(?m)^\s*\.globl\s+(\w+)',glue))
 glue=re.sub(r'(?m)^([A-Za-z_][A-Za-z_0-9]*):',lambda m:('\t.area _SDK'+('stdio' if m[1] in ('_getchar','_putchar') else m[1])+'\n' if m[1] in public else '')+m[0],glue)
 asm=out/'sdk.asm';asm.write_text(glue)
 rel=out/'sdk.rel';subprocess.run([assembler,'-g','-o',str(rel),str(asm)],check=True);paths.append(rel)
 objects=[read_object(p)for p in paths];members=read_archive(sdk/'lib/z80.lib')
 selected=library_closure(objects,members);objects +=[members[i].object for i in selected]
 code=[];data=[]
 for i,obj in enumerate(objects):
  for j,a in enumerate(obj.areas):
   if not a.size:continue
   if a.name in ('_INITIALIZED','_INITIALIZER','_GSINIT','_GSFINAL'):
    raise ValueError('native file state requires unsupported C initialization: '+a.name)
   if a.absolute:raise ValueError('file backend must be relocatable')
   (data if a.name in ('_DATA','_BSS','_INITIALIZED')else code).append((i,j,a))
 bases={};fragments=[];payload=bytearray();data_size=0
 for i,j,a in code:
  bases[i,j]=len(payload);fragments.append((i,j,a));payload+=a.data
 code_bytes=len(payload)
 for i,j,a in data:
  if a.initialized or a.relocations:raise ValueError('unexpected initialized native file state')
  bases[i,j]=len(payload);payload+=bytes(a.size);data_size+=a.size
 symbols={}
 for i,obj in enumerate(objects):
  for s in obj.symbols:
   if s.defined:
    value=s.value+(bases[i,s.area]if s.area is not None and obj.areas[s.area].size else 0)
    if s.name in symbols and symbols[s.name]!=value:raise ValueError('duplicate symbol '+s.name)
    symbols[s.name]=value
 def locate(value):
  for f,(i,j,a)in enumerate(fragments):
   if bases[i,j]<=value<bases[i,j]+a.size:return f,value-bases[i,j]
  if code_bytes<=value<len(payload):return len(fragments),value-code_bytes
  raise ValueError(f'target outside file unit: {value}')
 references=[];records=bytearray()
 for i,j,a in fragments:
  first=len(references)
  for r in sorted(a.relocations,key=lambda r:r.site):
   addend=r.addend
   target=(symbols[objects[i].symbols[r.target].name]if r.symbol else bases[i,r.target])
   value=target+addend
   if value>=65536 and addend>=32768:value-=65536
   f,offset=locate(value);kind=('word16','low8','high8').index(r.kind)
   references.append((r.site,1,f|(kind<<14),offset))
  records+=struct.pack('<4H',bases[i,j],a.size,first,len(references)-first)
 if data_size:records+=struct.pack('<4H',code_bytes,data_size,len(references),0)
 count=len(fragments)+bool(data_size);exports=bytearray()
 for name,symbol in EXPORTS.items():
  fragment,offset=locate(symbols[symbol]);assert fragment<len(fragments)
  exports+=struct.pack('<4B5H',len(name),0,0,0,65535,0,fragment,offset,0)+name.encode()
 unit=b'sysfile';path=b'T:/lib/SYSFILE.PAS';start=48+len(unit)+len(path)
 metadata=unit+path+exports+bytes(count)
 blob=b'ZPF1'+struct.pack('<6H',1,count,len(references),len(payload),0,0)+records+b''.join(struct.pack('<4H',*r)for r in references)+payload
 header=bytearray(48);header[:4]=b'ZPU1';struct.pack_into('<HH',header,4,1,48+len(metadata)+len(blob));# Profile is stamped after this payload participates in its fingerprint.
 struct.pack_into('<IHBB3H',header,12,fnv(b' '),1,len(unit),len(path),len(EXPORTS),0,0)
 struct.pack_into('<H',header,26,48+len(metadata));struct.pack_into('<HH',header,32,code_bytes,data_size)
 struct.pack_into('<6H',header,36,0,0,start,start,start,0)
 artifact=header+metadata+blob;assert len(artifact)<=65534 and len(payload)<32768
 struct.pack_into('<I',artifact,28,fnv(artifact[:28]+artifact[32:]))
 (directory/'sysfile.payload').write_bytes(artifact)
 report={'bytes':len(artifact),'code_bytes':code_bytes,'data_bytes':data_size,'fragments':count,'relocations':len(references),'sdk_members':[members[i].name for i in selected],'sha256':hashlib.sha256(artifact).hexdigest()}
 (directory/'direct-file.payload.json').write_text(json.dumps(report,indent=2)+'\n')
 if not payload_only:return stamp(directory)
 print(json.dumps(report))


def stamp(directory):
 """Bind the compiled payload to its runtime fingerprint without recompiling."""
 payload=(directory/'sysfile.payload').read_bytes()
 report=json.loads((directory/'direct-file.payload.json').read_text())
 if (len(payload)<48 or payload[:6]!=b'ZPU1\1\0' or
     struct.unpack_from('<H',payload,6)[0]!=len(payload) or payload[8:12]!=bytes(4) or
     struct.unpack_from('<I',payload,28)[0]!=fnv(payload[:28]+payload[32:]) or
     report['sha256']!=hashlib.sha256(payload).hexdigest()):
  raise ValueError('invalid or stale SYSFILE payload')
 profile=(directory/'pascal_runtime.profile').read_bytes()
 if len(profile)!=4:raise ValueError('runtime fingerprint must contain four bytes')
 artifact=bytearray(payload);artifact[8:12]=profile
 checksum=fnv(artifact[:28]+artifact[32:]);struct.pack_into('<I',artifact,28,checksum)
 report['sha256']=hashlib.sha256(artifact).hexdigest()
 (directory/'SYSFILE.ZPU').write_bytes(artifact)
 (directory/'file-unit.h').write_text(f'/* Compiler-private file backend identity. */\n#define SYSFILE_CHECKSUM {checksum}UL\n')
 (directory/'direct-file.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))


if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('directory',type=Path)
 mode=p.add_mutually_exclusive_group();mode.add_argument('--payload-only',action='store_true');mode.add_argument('--stamp',action='store_true')
 p.add_argument('--cc');p.add_argument('--assembler');p.add_argument('--sdk',type=Path);a=p.parse_args()
 if a.stamp:stamp(a.directory)
 else:
  if not all((a.cc,a.assembler,a.sdk)):p.error('payload compilation requires --cc, --assembler and --sdk')
  build(a.directory,a.cc,a.assembler,a.sdk,a.payload_only)
