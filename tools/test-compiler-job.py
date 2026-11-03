#!/usr/bin/env python3
"""Execute the production banked compiler job, adoption and publication on Zeal."""
import hashlib
import importlib.util
import json
import struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('native',ROOT/'tools/test-native.py')
n=importlib.util.module_from_spec(spec);spec.loader.exec_module(n)
sym=n.symbols('zjobtest')
irq_spec=importlib.util.spec_from_file_location('span_irq',ROOT/'tools/test-span-irq.py')
irq=importlib.util.module_from_spec(irq_spec);irq_spec.loader.exec_module(irq)
irq_events,irq_timings,irq_proof={},[],{}
SpanConsole=irq.span_console(n,sym,(ROOT/'build/zjobtest.bin').read_bytes(),irq_events,irq_timings,irq_proof)
commands=['run 100000000']+n.program_launch('zjobtest')+[
    '@irq-spans',f"bp 0x{sym['_test_stop']:x}",'run 200000000000','s','regs','mmu']
for name in ('_test_done','_test_stage','_test_passed','_test_failed'):
    commands += [f'rb 0x{sym[name]:x} 2']
commands += [f"rb 0x{sym['_test_failures']:x} 128",'quit']
fixtures={f'zjobtest.{ext}':(ROOT/f'build/zjobtest.{ext}').read_bytes()for ext in ('bin','zpl')}
fixtures.update({'JOB.PAS':b'{original disk source retained}\r\n',
    'PART.INC':b'const answer=42;\r\n','BAD.INC':b'const answer=;\r\n',
    'DISK.PAS':b"begin writeln('DISK-ROOT')end.",
    'LARGE.PAS':(ROOT/'tests/native/chain-call.pas').read_bytes()})
inputs=[ROOT/'tools/test-span-irq.py',ROOT/'CMakeLists.txt',Path(__file__).resolve(),n.EMULATOR,n.ROM]
inputs+=list((ROOT/'src').glob('*'))+list((ROOT/'tests/native').glob('*'))
inputs += [ROOT/f'build/zjobtest.{ext}'for ext in ('bin','zpl','banks.json')]
hashes={str(p):n.sha(p)for p in inputs if p.is_file()}
raw=n.execute('compiler-job',commands,fixtures,timeout=1800,console_class=SpanConsole)
memory=n.memory(raw);word=lambda name:memory.get(sym[name],0)|memory.get(sym[name]+1,0)<<8
disk=n.RUN/'compiler-job/hostfs'
checks={'native completion':memory.get(sym['_test_done'])==255 and word('_test_stage')==6,
    'job compile, RAM/saved run, source errors and page ownership':word('_test_failed')==0 and word('_test_passed')>=60,
    'RAM and saved native programs execute':(disk/'JOBS.TXT').is_file()and(disk/'JOBS.TXT').read_bytes()==b'JOB-OK\r\n'*2,
    'Memory and saved compilation never silently save sources':all((disk/name).read_bytes()==content for name,content in fixtures.items()if name.endswith(('.PAS','.INC'))),
    'all owned transactions are cleaned':not any(p.name.startswith('ZP')for p in disk.iterdir()),
    'include reads cannot retarget main outputs':(disk/'JOB.COM').is_file()and not(disk/'JOB.ZMP').exists()and not(disk/'PART.COM').exists()}
try:
    image=(disk/'JOB.COM').read_bytes();chain=(disk/'JOB.CHN').read_bytes()
    from artifact_format import validate_image
    validate_image(image)
    checks['versioned main has checked image ranges']=True
    checks['versioned CHN has checked header and extent']=chain[:5]==b'ZCHN\2'and len(chain)==96+struct.unpack_from('<H',chain,12)[0]and chain[20:84]==bytes(64)
except(OSError,ValueError,struct.error,AssertionError)as error:
    checks['independent artifact validation']=False
    print(repr(error))
checks['live read/fill/backward/forward keyboard IRQs preserve all span assertions']=(
    set(irq_events)=={1,2,3,4}and all(e.get('passed')for e in irq_events.values())and irq_proof.get('original_span_checks_pass',False))
checks['all qualification inputs unchanged']=all(n.sha(Path(p))==digest for p,digest in hashes.items())
report={'passed':all(checks.values()),'scope':__doc__+' Overlays remain deferred; this is bounded emulator acceptance.',
    'checks':checks,'native_passed':word('_test_passed'),'native_failed':word('_test_failed'),
    'native_stage':word('_test_stage'),'span_irq':{'events':irq_events,'proof':irq_proof,'timings':irq_timings},'capture_directory':str(n.RUN.relative_to(ROOT)),'input_sha256':hashes}
(n.RUN/'compiler-job-report.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2));assert report['passed']
