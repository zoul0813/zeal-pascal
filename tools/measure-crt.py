#!/usr/bin/env python3
"""Compare Pascal CRT size and CPU cycles with the historical emitter oracle."""
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import struct

ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
n=module('native',ROOT/'tests/acceptance/test-native.py')
oracle=module('crt_oracle',ROOT/'tools/package-screen.py')
OPERATIONS=('NormVideo','GotoXY(7,9)','TextColor(Green)','B := WhereX','ClrEol','ClrScr','InsLine','DelLine')

def marker(index):return bytes((0x3e,0xc0+index,0,0,0))
def marker_source(index):return 'Inline('+ '/'.join('$%02X'%v for v in marker(index))+');\n'
SOURCE=('uses Crt;\nvar B:Byte;\nbegin\nCrtInit;\nInline($F3);\n'+
        ''.join(marker_source(i)+operation+';\n' for i,operation in enumerate(OPERATIONS))+
        marker_source(len(OPERATIONS))+'Inline($FB);\nCrtExit\nend.\n').encode()

def compile_client(label,unit,source=SOURCE):
    sym=n.symbols('zpascal');typed=lambda value:n.type_text(value,ticks=100000000)
    commands=['run 100000000']+n.program_launch()+['run 100000000']+typed('n')
    commands+=typed('wH:/BENCH.PAS\n')+typed('ocq')+['tap C',n.ImageStop(sym['_key_read'],'zpascal.bin'),'quit']
    n.execute(label,commands,{'CRT.ZPU':unit,'BENCH.PAS':source},timeout=600,base_programs=('zpascal',))
    folder=n.RUN/label
    if (folder/'input-error.json').exists():raise RuntimeError((folder/'input-error.json').read_text())
    return (folder/'hostfs/BENCH.COM').read_bytes()


def measure(label,image):
    addresses=[];cycles=[]
    for i in range(len(OPERATIONS)+1):
        offsets=[at for at in range(len(image)) if image.startswith(marker(i),at)]
        if len(offsets)!=1:raise ValueError('Benchmark marker is not unique')
        addresses.append(0x4000+offsets[0])
    class Probe(n.DebugConsole):
        def command(self,command,capture=False):
            if command!='@measure':return super().command(command,capture)
            call=lambda value:super(Probe,self).command(value)
            call(f'bp 0x{addresses[0]:x}')
            call('tap ENTER')
            for at in addresses:
                state=call('regs')
                if f'PC={at:04x} ' not in state:
                    call('run 2000000000');state=call('regs')
                if f'PC={at:04x} ' not in state:raise RuntimeError('Benchmark missed a marker: '+state)
                call(f'bc 0x{at:x}')
                cycles.append(int(re.search(r'cyc=(\d+)',call('s'))[1]))
                if len(cycles)<len(addresses):call(f'bp 0x{addresses[len(cycles)]:x}')
            call('run 100000000')
            return 'CRT benchmark complete\n'
    commands=['run 100000000']+n.type_text('H:/BENCH.COM',ticks=100000000)+['@measure','quit']
    n.execute(label,commands,{'BENCH.COM':image},timeout=300,console_class=Probe,base_programs=())
    if len(cycles)!=len(addresses):raise RuntimeError('Incomplete timing capture')
    return {operation:cycles[i+1]-cycles[i] for i,operation in enumerate(OPERATIONS)}


def main():
    directory=n.RUN/'oracle';directory.mkdir(parents=True)
    for name in ('pascal_runtime.private','pascal_runtime.profile','runtime-layout.h'):
        shutil.copyfile(ROOT/'build'/name,directory/name)
    oracle.build(directory)
    units={'historical':(directory/'CRT.ZPU').read_bytes(),
           'pascal':(ROOT/'build/CRT.ZPU').read_bytes()}
    rows={}
    for label,unit in units.items():
        image=compile_client('crt-size-'+label,unit)
        sizes={}
        for case,source in {
            'unused':b'uses Crt;begin end.',
            'color':b'uses Crt;begin TextColor(Green)end.',
            'clear_eol':b'uses Crt;begin ClrEol end.'
        }.items():
            small=compile_client('crt-size-'+label+'-'+case,unit,source)
            flags=small[18]
            sizes[case]={'com_bytes':len(small),'image_flags':flags,
                         'execution_pages':1 if flags&4 else 2 if flags&2 else 3}
        rows[label]={'single_routine_sizes':sizes,'unit_bytes':len(unit),'unit_code_bytes':struct.unpack_from('<H',unit,32)[0],
                     'benchmark_com_bytes':len(image),'image_flags':image[18],
                     'cycles':measure('crt-cycles-'+label,image)}
    report={'passed':True,'measurements':rows,'capture':str(n.RUN),
            'scope':'Standalone COM, interrupts disabled during timed calls; includes caller guards and marker padding. Not physical-board timing.',
            'compiler_sha256':{name:hashlib.sha256((ROOT/'build'/name).read_bytes()).hexdigest() for name in ('zpascal.bin','zpascal.zpl')},
            'unit_sha256':{label:hashlib.sha256(data).hexdigest() for label,data in units.items()}}
    (n.RUN/'crt-measurements.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':main()
