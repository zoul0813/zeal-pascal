#!/usr/bin/env python3
"""Check final-link code residency against the actual ZDE-linked product objects."""
import collections
import hashlib
import json
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[1]
BUILD=ROOT/'build'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
map_text=(BUILD/'zpascal.map').read_text()
symbols={name:int(addr,16)for addr,name in re.findall(
 r'^\s*([0-9A-Fa-f]{8})\s+(_\w+)(?:\s+\S+)?\s*$',map_text,re.M)}
metadata=json.loads((BUILD/'zpascal.banks.json').read_text())
assert sha(BUILD/'zpascal.zpl')==metadata['library_sha256']
assert (BUILD/'zpascal.bin').stat().st_size==metadata['resident_bytes']
for bank,size in enumerate(metadata['bank_bytes'],1):
 assert int(re.search(r'^\s*([0-9A-Fa-f]{8})\s+l__BANK'+str(bank)+r'\s',map_text,re.M)[1],16)==size
 assert size<=16384
names=re.findall(r'CODE_\w+',re.search(r'enum \{(.*?)\};',(ROOT/'src/library.h').read_text(),re.S)[1])
lent={names.index(n)+1 for n in ('CODE_EXPRESSIONS','CODE_FILE_COMPILER','CODE_TRANSFER')}
functions={};inputs={}
for object_name in re.findall(r'(\S+\.c\.rel)',(BUILD/'CMakeFiles/zpascal.dir/link.txt').read_text()):
 obj=BUILD/object_name;asm=BUILD/(object_name[:-4]+'.asm')
 assert obj.is_file() and asm.is_file(),object_name
 inputs[object_name]=sha(obj);inputs[str(asm.relative_to(BUILD))]=sha(asm)
 unit=asm.stem;area=None;current=None
 for line in asm.read_text().splitlines():
  match=re.match(r'\s*\.area\s+_(\w+)',line)
  if match:area=match[1]
  match=re.match(r'; Function (\w+)',line)
  if match:
   current=(unit,match[1]);functions[current]={'area':area,'lines':[],'calls':set(),'sites':[],'unmodeled_calls':[]}
  elif current:functions[current]['lines'].append(line)
for key,f in functions.items():
 # Some SDCC files select their code area immediately after the function comment.
 for line in f['lines']:
  match=re.match(r'\s*\.area\s+_(\w+)',line)
  if match:f['area']=match[1];break
 f['bank']=int(f['area'][4:])if f['area'].startswith('BANK')else 0
 # ASxxxx truncates long map names. Use the compiled area for private functions;
 # verify exported functions against the linked map, including truncated names.
 symbol='_'+key[1];linked=symbols.get(symbol,symbols.get(symbol[:32]))
 if any(re.match(r'^_'+re.escape(key[1])+r'::',line)for line in f['lines']):
  assert linked is not None and linked>>16==f['bank'],key
 previous=''
 for index,line in enumerate(f['lines']):
  if not line.strip() or line.lstrip().startswith(';'):continue
  match=re.match(r'\s*(?:call|jp)\s+(?:\w+,\s*)?#?(_\w+)\s*$',line)
  if match:
   target=match[1]
   if target=='___sdcc_bcall_ehl':
    far=re.match(r'\s*ld\s+hl,\s*#(_\w+)\s*$',previous)
    assert far,('unresolved banked call',key,previous)
    target=far[1]
   f['calls'].add(target[1:]);f['sites'].append((index,target[1:]))
  if re.match(r'\s*call\s+',line) and not match:
   target=line.split(None,1)[1].split(',')[-1].strip()
   local=any(re.match(r'^'+re.escape(target)+r':',body)for body in f['lines'])
   if not local:f['unmodeled_calls'].append(line)
  previous=line
public={key[1]:key for key,f in functions.items()if any(
 re.match(r'^_'+re.escape(key[1])+r'::',line)for line in f['lines'])}
def resolve(unit,name):
 if (unit,name)in functions:return(unit,name)
 if name in public:return public[name]
 symbol='_'+name
 assert symbol in symbols or symbol[:32]in symbols,('unresolved call',unit,name)
 return('resident-assembly',name)
def bank_of(key):
 if key in functions:return functions[key]['bank']
 symbol='_'+key[1];value=symbols.get(symbol,symbols.get(symbol[:32]));assert value is not None
 return value>>16
roots=('cc_place_code','cc_units_link','cc_units_write')
closure={}
for root in roots:
 pending=[public[root]];seen=set()
 while pending:
  key=pending.pop()
  if key in seen:continue
  seen.add(key);assert bank_of(key)not in lent,('callee in lent bank',root,key)
  assert not re.fullmatch(r'_sdcc_call_\w+',key[1]),('indirect call needs audit',root,key)
  if key not in functions:continue
  assert not functions[key]['unmodeled_calls'],('unmodeled call',key,functions[key]['unmodeled_calls'])
  calls=functions[key]['calls']
  if key[1]==root:
   sites=functions[key]['sites']
   first=next(i for i,n in sites if n=='code_phase_unit_finish_begin')
   last=next(i for i,n in sites if n=='code_phase_unit_finish_end')
   # Activation placement after end is allowed to use its restored bank.
   calls={name for i,name in sites if first<i<last}|{'cc_fail'}
  pending.extend(resolve(key[0],name)for name in calls)
 closure[root]=seen
reverse=collections.defaultdict(set)
for key,f in functions.items():
 for name in f['calls']:reverse[resolve(key[0],name)].add(key)
callers=set();pending=[public[n]for n in roots]
while pending:
 key=pending.pop()
 if key in callers:continue
 callers.add(key);assert bank_of(key)not in lent,('active caller in lent bank',key)
 pending.extend(reverse[key])
assert {key[1]for key in reverse[public['code_phase_unit_finish_begin']]}==set(roots)
required={'cc_link_apply','cc_layout_finish','cc_units_link_object','cc_file_native_bind','append_metadata',
 'cc_mapping','cc_require_real','cc_require_full_memory','cc_require_hardware','cc_units_write_object',
 'cc_constant_literal','cc_next','cc_real_literal','cc_fail'}
assert required<={key[1]for seen in closure.values()for key in seen}
# Observe gating in emitted instructions and actual linked placement, not just C flags.
phase=functions[public['code_phase_unit_finish_begin']];drops=[];previous=''
for line in phase['lines']:
 if not line.strip()or line.lstrip().startswith(';'):continue
 if re.match(r'\s*call\s+_drop_bank\s*$',line):
  value=re.match(r'\s*ld\s+a,\s*#0x([0-9a-fA-F]+)\s*$',previous);assert value
  drops.append(int(value[1],16))
 previous=line
assert set(drops[:3])=={n-1 for n in lent}
assert bank_of(public['cc_source_byte'])==bank_of(public['cc_reference_position'])==names.index('CODE_FILE_RUNTIME')+1
flags=BUILD/'CMakeFiles/unit_runtime_core.dir/flags.make'
assert '-DZP_OPTIONAL_REAL'in flags.read_text()and '-DZP_UNITS_EXPERIMENT'in flags.read_text()
assert public['cc_source_byte'][0]=='compiler_sources.c'and public['code_phase_unit_finish_begin'][0]=='library_phase.c'
report={'passed':True,'scope':'Direct C call/SDK banked-call closure of each lending interval; all ancestor callers; compiled static-layout gating.',
 'map_sha256':sha(BUILD/'zpascal.map'),'library_sha256':metadata['library_sha256'],'flags_sha256':sha(flags),
 'lent_banks':sorted(lent),'source_access_bank':bank_of(public['cc_source_byte']),
 'caller_count':len(callers),'callers':[{ 'module':k[0],'function':k[1],'bank':bank_of(k)}for k in sorted(callers)],
 'intervals':{name:{'function_count':len(seen),'banks':sorted({bank_of(k)for k in seen})}for name,seen in closure.items()},
 'required_functions':{name:sorted({bank_of(k)for seen in closure.values()for k in seen if k[1]==name})for name in sorted(required)},
 'compiled_inputs_sha256':inputs}
output=BUILD/'unit-banks.json';output.write_text(json.dumps(report,indent=2)+'\n')
print('PASS:',len(callers),'active callers;',', '.join(f'{name}: {len(seen)} callees'for name,seen in closure.items()),'; no lent-bank code or unresolved/indirect calls')
print(output)
