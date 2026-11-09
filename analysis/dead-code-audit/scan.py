from pathlib import Path
import re,json,shlex,hashlib,collections
R=Path.cwd();B=R/'build'
links=[n for n in shlex.split((B/'CMakeFiles/zpascal.dir/link.txt').read_text())if n.endswith('.rel')]
functions={};public={};area_sizes={};rootrefs=[];datarefs=[]
for obj in links:
 rel=B/obj;text=rel.read_text();unit=obj.split('.dir/',1)[1].removesuffix('.rel')
 area_sizes[unit]={n:int(size,16)for n,size in re.findall(r'^A (\S+) size ([0-9A-Fa-f]+)',text,re.M)}
 if not obj.endswith('.c.rel'):
  rootrefs.extend((unit,n.removeprefix('b'))for n in re.findall(r'^S (\S+) Ref',text,re.M))
  continue
 asm=(B/(obj[:-4]+'.asm')).read_text();current=None;area=None
 for line in asm.splitlines():
  m=re.match(r'\s*\.area (\S+)',line)
  if m:area=m[1]
  m=re.match(r'; Function (\w+)',line)
  if m:
   current=(unit,m[1]);functions[current]={'unit':unit,'name':m[1],'area':area,'lines':[],'refs':set()}
  if current:
   functions[current]['lines'].append(line)
   if re.match(r'^_'+re.escape(current[1])+r'::?',line):
    functions[current]['area']=area
    if re.match(r'^_'+re.escape(current[1])+r'::',line):public['_'+current[1]]=current
for key,f in functions.items():
 for line in f['lines']:
  s=line.split(';',1)[0].strip()
  if not s or s.startswith(('.globl','.module','.optsdcc')):continue
  s=re.sub(r'^\w+::?','',s)
  refs={'_'+n for n in re.findall(r'(?<![\w])(?:b)?_([A-Za-z]\w*)',s)}
  if s.startswith('.'):
   datarefs.extend((key[0],n)for n in refs)
  else:f['refs'].update(refs)
def resolve(unit,n):
 if n.startswith('_')and(unit,n[1:])in functions:return(unit,n[1:])
 return public.get(n)
incoming=collections.defaultdict(set)
for key,f in functions.items():
 for n in f['refs']:
  target=resolve(key[0],n)
  if target and target!=key:incoming[target].add(key)
for unit,n in datarefs:
 target=resolve(unit,n)
 if target:incoming[target].add(('data',unit))
for unit,n in rootrefs:
 target=resolve(unit,n)
 if target:incoming[target].add(('assembly',unit))
# Data/global initialization outside the function spans can also take addresses.
for obj in links:
 if not obj.endswith('.c.rel'):continue
 unit=obj.split('.dir/',1)[1].removesuffix('.rel');asm=(B/(obj[:-4]+'.asm')).read_text();before=asm.split('; Function ',1)[0]
 for line in before.splitlines():
  s=line.split(';',1)[0].strip()
  if not s or s.startswith(('.globl','.module','.optsdcc')):continue
  s=re.sub(r'^\w+::?','',s)
  for n in re.findall(r'(?<![\w])(?:b)?_([A-Za-z]\w*)',s):
   target=resolve(unit,'_'+n)
   if target:incoming[target].add(('data',unit))
# Locate function extents from assembler listings. Last-function sizes are upper
# bounds: constant pools or other trailing areas may belong to the same object.
for obj in links:
 if not obj.endswith('.c.rel'):continue
 unit=obj.split('.dir/',1)[1].removesuffix('.rel');lst=(B/(obj[:-4]+'.lst')).read_text();labels={}
 for addr,n in re.findall(r'^\s*([0-9A-Fa-f]{6})\s+\d+\s+_(\w+)::?\s*$',lst,re.M):
  if(unit,n)in functions:labels[n]=int(addr,16)
 byarea=collections.defaultdict(list)
 for n,addr in labels.items():byarea[functions[unit,n]['area']].append((addr,n))
 for area,entries in byarea.items():
  entries.sort()
  for i,(addr,n)in enumerate(entries):
   end=entries[i+1][0]if i+1<len(entries)else area_sizes[unit].get(area,addr)
   functions[unit,n]['bytes_to_next_label_or_area_end']=end-addr
   functions[unit,n]['last_in_area']=i+1==len(entries)
roots={key for key,refs in incoming.items()if any(x[0]in('assembly','data')for x in refs)}
if '_main' in public:roots.add(public['_main'])
reachable=set();pending=list(roots)
while pending:
 key=pending.pop()
 if key in reachable:continue
 reachable.add(key)
 for n in functions[key]['refs']:
  target=resolve(key[0],n)
  if target:pending.append(target)
unreachable=[{k:v for k,v in f.items()if k not in('lines','refs')}for key,f in functions.items()if key not in reachable]
candidates=[]
for key,f in functions.items():
 if incoming[key]:continue
 candidates.append({k:v for k,v in f.items()if k not in('lines','refs')})
# Product sources vs test/generator references, including files not linked into IDE.
all_sources=[p.as_posix()for p in (R/'src').rglob('*')if p.is_file()]
report={'baseline':__import__('subprocess').check_output(['git','rev-parse','HEAD']).decode().strip(),'product_sha256':{n:hashlib.sha256((B/n).read_bytes()).hexdigest()for n in('zpascal.bin','zpascal.zpl')},'linked_objects':len(links),'c_functions':len(functions),'no_incoming_reference_candidates':candidates,'unreachable_from_conservative_roots':unreachable,'data_directive_function_roots':len({resolve(u,n)for u,n in datarefs if resolve(u,n)}),'method':'All named references in data directives across complete ASM files are independent roots; function area is bound at its actual label. Conservative emitted-ASM symbol references, bank aliases, assembly object Ref symbols and data address references. Not a proof against numeric entry points or unreferenced cycles. Function spans may include constant pools; manually verify before removal.'}
(R/'analysis/dead-code-audit/linked-candidates.json').write_text(json.dumps(report,indent=2)+'\n')
for f in candidates:print(f['unit'],f['name'],f.get('bytes_to_next_label_or_area_end','?'),f.get('last_in_area'))
print('Unreachable closure',len(unreachable),'spans',sum(f.get('bytes_to_next_label_or_area_end',0)for f in unreachable));print('TOTAL',len(links),'objects',len(functions),'functions',len(candidates),'candidates')
